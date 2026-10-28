// Tests for cache policies. Run: ./build/policy_tests  (or ctest --test-dir build)
//
// Three groups:
//   1. hand-written traces with hit counts worked out by hand in the comments
//      (m = miss, h = hit, {...} = cache contents);
//   2. properties that must hold on any trace, checked on random traces:
//      no policy beats the ideal cache, two levels behave as expected;
//   3. a large run: a million accesses must finish quickly and give the
//      hit rate that the pattern predicts.

#include "TestUtils.hpp"

#include <chrono>
#include <random>

static const std::vector<std::string> Policies = {"LRU", "LFU", "2Q"};
static int Failed = 0;

static void check(const std::string &Name, bool Ok, const std::string &Info) {
  std::cout << (Ok ? "[ok]   " : "[FAIL] ") << Name << ": " << Info << '\n';
  if (!Ok) {
    ++Failed;
  }
}

static void expectHits(const std::string &Name, const std::string &Policy,
                       std::size_t Capacity, const Trace &Keys,
                       std::size_t Expected) {
  std::size_t Got = runPolicy(Policy, Capacity, Keys);
  check(Name, Got == Expected,
        "expected " + std::to_string(Expected) + ", got " + std::to_string(Got));
}

static Trace randomTrace(std::mt19937 &Rng, std::size_t Length, int MaxKey) {
  std::uniform_int_distribution<int> Key(1, MaxKey);
  Trace Keys(Length);
  for (int &K : Keys) {
    K = Key(Rng);
  }
  return Keys;
}

// ---- 1. hand-written cases -------------------------------------------------

static void handCases() {
  std::cout << "--- hand-written traces ---\n";
  for (const std::string &P : Policies) {
    // 1m 1h 1h
    expectHits(P + ": same key", P, 1, {1, 1, 1}, 2);
    // capacity 1 can never hold both keys
    expectHits(P + ": thrash", P, 1, {1, 2, 1, 2}, 0);
    // first pass misses, second pass hits
    expectHits(P + ": set fits", P, 3, {1, 2, 3, 1, 2, 3}, 3);
  }

  // 1m 2m 1h {1,2}; 3m evicts 2 (older) {1,3}; 2m
  expectHits("LRU: evicts oldest", "LRU", 2, {1, 2, 1, 3, 2}, 1);
  // loop of 4 keys in cache of 3 always evicts the next needed key
  expectHits("LRU: loop", "LRU", 3, {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4}, 0);

  // 1m 1h 1h {1:3}; 2m {1:3,2:1}; 3m evicts 2 {1:3,3:1}; 2m evicts 3; 1h
  expectHits("LFU: evicts rarest", "LFU", 2, {1, 1, 1, 2, 3, 2, 1}, 3);

  // capacity 4 -> A1In quota 1. 1m 1h (1 -> Am); 2m 3m 4m (A1In = 4 3 2);
  // 5m: A1In over quota -> evicts 2; 6m evicts 3; 1h still in Am.
  // LRU on the same trace evicts 1 at "5" and gets only 1 hit.
  expectHits("2Q: keeps hot key", "2Q", 4, {1, 1, 2, 3, 4, 5, 6, 1}, 2);
  expectHits("LRU: loses hot key", "LRU", 4, {1, 1, 2, 3, 4, 5, 6, 1}, 1);

  // L1 = LRU(1), L2 = LRU(1). 1m -> L1{1}; 2m -> L1{2} L2{1};
  // 1h in L2 -> promoted, 2 demoted: L1{1} L2{2}; 2h in L2.
  check("2 levels: promotion/demotion",
        runSystem({"LRU", "LRU"}, {1, 1}, {1, 2, 1, 2}) == 2, "expected 2");
}

// ---- 2. properties on random traces ----------------------------------------

static void properties() {
  std::cout << "--- properties on random traces ---\n";
  std::mt19937 Rng(42);
  const std::size_t Capacity = 64;
  bool NotAboveIdeal = true, TwoLevelsEqualOne = true, SecondLevelHelps = true;

  for (int I = 0; I < 5; ++I) {
    Trace Keys = randomTrace(Rng, 20000, 3 * Capacity);
    std::size_t Ideal = runIdeal(Capacity, Keys);

    for (const auto &P : Policies) {
      // Belady is optimal: a real policy may equal it but never exceed it.
      if (runPolicy(P, Capacity, Keys) > Ideal) {
        NotAboveIdeal = false;
      }
      // L1 decisions do not depend on L2, so adding a second level can only
      // turn some misses into hits.
      std::size_t Single = runPolicy(P, Capacity / 2, Keys);
      std::size_t Double = runSystem({P, P}, {Capacity / 2, Capacity / 2}, Keys);
      if (Double < Single) {
        SecondLevelHelps = false;
      }
    }
    // Two LRU levels with promotion/demotion are exactly one LRU of the
    // combined size: the demoted key is the LRU key of the whole system.
    if (runSystem({"LRU", "LRU"}, {Capacity / 2, Capacity / 2}, Keys) !=
        runPolicy("LRU", Capacity, Keys)) {
      TwoLevelsEqualOne = false;
    }
  }
  check("no policy beats the ideal cache", NotAboveIdeal, "5 traces");
  check("second level never reduces hits", SecondLevelHelps, "5 traces");
  check("LRU+LRU == LRU of combined size", TwoLevelsEqualOne, "5 traces");
}

// ---- 3. large run ----------------------------------------------------------

static void largeRun() {
  std::cout << "--- large run: 1M accesses, capacity 1024 ---\n";
  std::mt19937 Rng(7);
  const std::size_t Capacity = 1024;
  const std::size_t Length = 1000000;
  // Uniform keys over twice the capacity: once warm, the cache holds half of
  // the keys, so every policy must land near a 50% hit rate.
  Trace Keys = randomTrace(Rng, Length, 2 * Capacity);

  for (const auto &P : Policies) {
    auto Start = std::chrono::steady_clock::now();
    std::size_t Hits = runPolicy(P, Capacity, Keys);
    double Seconds = std::chrono::duration<double>(
                         std::chrono::steady_clock::now() - Start)
                         .count();
    double Rate = 100.0 * Hits / Length;

    std::ostringstream Info;
    Info.precision(2);
    Info << std::fixed << Rate << "% hits, " << Seconds << " s";
    check(P + ": large run", Rate > 48.0 && Rate < 52.0 && Seconds < 15.0,
          Info.str());
  }
}

int main() {
  handCases();
  properties();
  largeRun();
  std::cout << '\n' << Failed << " failed\n";
  return Failed == 0 ? 0 : 1;
}
