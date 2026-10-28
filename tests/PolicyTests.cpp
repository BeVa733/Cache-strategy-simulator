// Tests for cache policies. Build: cmake --build build, run:
// ./build/tests/policy_tests
//
// The suite contains hand-written cases, properties checked on deterministic
// random traces, and a large run that exercises every supported policy.

#include "Cache.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static const std::vector<std::string> Policies = {"LRU", "LFU",  "2Q",
                                                  "ARC", "LIRS", "OPT"};

static int Failed = 0;

struct RunResult {
  std::uint64_t Hits;
  std::uint64_t Misses;
  std::vector<std::uint64_t> LevelHits;
  double Cost;
};

// Give every run its own config so parallel test processes do not conflict.
static std::string configFilename() {

  static const auto RunId =
      std::chrono::steady_clock::now().time_since_epoch().count();
  static std::size_t ConfigNumber = 0;

  return "policy_test_" + std::to_string(RunId) + "_" +
         std::to_string(ConfigNumber++) + ".tmp";
}

// Run a cache configuration on the trace and collect its public metrics.
static RunResult runCache(const std::vector<std::string> &Policies,
                          const std::vector<std::size_t> &Capacities,
                          const std::vector<int> &Trace,
                          const std::vector<double> &Costs = {},
                          double MissCost                  = 1.0) {

  const std::string ConfigFilename = configFilename();
  std::ofstream Config(ConfigFilename);
  Config << Policies.size() << '\n';
  for (const std::string &Policy : Policies) {
    Config << Policy << '\n';
  }

  if (!Costs.empty()) {
    Config << "costs";
    for (double Cost : Costs) {
      Config << ' ' << Cost;
    }
    Config << "\nmiss_cost " << MissCost << '\n';
  }

  Config.close();

  MultiLevelCache<int> Cache(ConfigFilename.c_str(), Capacities, Trace);
  for (int Key : Trace) {
    Cache.accessElement(Key);
  }

  std::remove(ConfigFilename.c_str());
  return {Cache.hitCount(), Cache.missCount(), Cache.levelHitCounts(),
          Cache.totalCost()};
}

// Run one cache level with the given policy.
static RunResult runPolicy(const std::string &Policy, std::size_t Capacity,
                           const std::vector<int> &Trace) {

  return runCache({Policy}, {Capacity}, Trace);
}

static void check(const std::string &Name, bool Ok,
                  const std::string &Details) {

  std::cout << (Ok ? "[ok]   " : "[FAIL] ") << Name << ": " << Details << '\n';
  if (!Ok) {
    ++Failed;
  }
}

static void expectHits(const std::string &Name, const std::string &Policy,
                       std::size_t Capacity, const std::vector<int> &Trace,
                       std::size_t Expected) {

  const RunResult Result = runPolicy(Policy, Capacity, Trace);
  check(Name, Result.Hits == Expected,
        "expected " + std::to_string(Expected) + ", got " +
            std::to_string(Result.Hits));
}

static std::vector<int> randomTrace(std::mt19937 &Generator, std::size_t Length,
                                    int MaxKey) {

  std::uniform_int_distribution<int> KeyDistribution(1, MaxKey);
  std::vector<int> Trace(Length);
  for (int &Key : Trace) {
    Key = KeyDistribution(Generator);
  }

  return Trace;
}

// Check manually calculated policy behaviour on small traces.
static void testHandWrittenCases() {

  std::cout << "--- hand-written traces ---\n";

  for (const std::string &Policy : Policies) {
    // A disabled level must miss without invoking replacement logic.
    expectHits(Policy + ": zero capacity", Policy, 0, {1, 1, 1}, 0);
    // One miss followed by two hits.
    expectHits(Policy + ": same key", Policy, 1, {1, 1, 1}, 2);
    // A cache of size one cannot hold both alternating keys.
    expectHits(Policy + ": thrash", Policy, 1, {1, 2, 1, 2}, 0);
    // The first pass misses and the second pass hits.
    expectHits(Policy + ": set fits", Policy, 3, {1, 2, 3, 1, 2, 3}, 3);
  }

  // LRU evicts key 2 because it is the oldest key at request 3.
  expectHits("LRU: evicts oldest", "LRU", 2, {1, 2, 1, 3, 2}, 1);
  expectHits("LRU: loop", "LRU", 3, {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4}, 0);

  // LFU keeps frequently used key 1 while one-shot keys replace each other.
  expectHits("LFU: evicts rarest", "LFU", 2, {1, 1, 1, 2, 3, 2, 1}, 3);
  expectHits("LFU: keeps hot key", "LFU", 2, {1, 1, 1, 2, 3, 4, 5, 1}, 3);

  // 2Q moves a reused ghost key to the main queue and preserves it.
  const std::vector<int> HotTrace = {1, 2, 3, 4, 5, 1, 6, 7, 8, 9, 1};
  expectHits("2Q: keeps hot key", "2Q", 4, HotTrace, 1);
  expectHits("LRU: loses hot key", "LRU", 4, HotTrace, 0);

  // ARC adapts after misses in both ghost lists and keeps key 2 hot.
  expectHits("ARC: adapts to ghost hits", "ARC", 2, {1, 2, 1, 3, 2, 1, 2}, 2);

  // A reused resident HIR key becomes LIR and survives insertion of key 5.
  expectHits("LIRS: promotes reused HIR", "LIRS", 4, {1, 2, 3, 4, 4, 5, 4}, 2);

  // OPT evicts key 2 because key 1 is requested earlier in the future.
  expectHits("OPT: evicts farthest use", "OPT", 2, {1, 2, 3, 1, 2, 3}, 2);
}

// Check common invariants on several deterministic random traces.
static void testProperties() {

  std::cout << "--- properties on random traces ---\n";

  constexpr std::size_t Capacity = 32;
  constexpr std::size_t Samples  = 5;
  std::mt19937 Generator(42);
  std::vector<std::vector<int>> Traces;
  std::vector<std::uint64_t> OptimalHits;

  for (std::size_t Sample = 0; Sample < Samples; ++Sample) {
    Traces.push_back(randomTrace(Generator, 2000, 3 * Capacity));
    OptimalHits.push_back(runPolicy("OPT", Capacity, Traces.back()).Hits);
  }

  for (const std::string &Policy : Policies) {
    bool CountsAreCorrect   = true;
    bool NotAboveOptimal    = true;
    bool SecondLevelHelps   = true;
    bool LevelCountsAreGood = true;
    bool CostIsCorrect      = true;

    for (std::size_t Sample = 0; Sample < Samples; ++Sample) {
      const std::vector<int> &Trace = Traces[Sample];
      const RunResult Result        = runPolicy(Policy, Capacity, Trace);
      if (Result.Hits + Result.Misses != Trace.size()) {
        CountsAreCorrect = false;
      }

      if (Result.Hits > OptimalHits[Sample]) {
        NotAboveOptimal = false;
      }

      const RunResult Single = runPolicy(Policy, Capacity / 2, Trace);
      const RunResult Double =
          runCache({Policy, Policy}, {Capacity / 2, Capacity / 2}, Trace);
      if (Double.Hits < Single.Hits) {
        SecondLevelHelps = false;
      }

      if (Double.Hits + Double.Misses != Trace.size() ||
          Double.LevelHits[0] + Double.LevelHits[1] != Double.Hits) {
        LevelCountsAreGood = false;
      }

      const RunResult CostResult =
          runCache({Policy}, {Capacity}, Trace, {2.0}, 7.0);
      const double ExpectedCost =
          CostResult.Hits * 2.0 + CostResult.Misses * 7.0;
      if (std::abs(CostResult.Cost - ExpectedCost) > 1e-9) {
        CostIsCorrect = false;
      }
    }

    const std::string Details = std::to_string(Samples) + " traces";
    check(Policy + ": hits + misses", CountsAreCorrect, Details);
    check(Policy + ": does not beat OPT", NotAboveOptimal, Details);
    check(Policy + ": second level does not reduce hits", SecondLevelHelps,
          Details);
    check(Policy + ": level hit counts", LevelCountsAreGood, Details);
    check(Policy + ": cost accounting", CostIsCorrect, Details);
  }
}

// Check exact movement and cost accounting in a small two-level cache.
static void testTwoLevelDetails() {

  std::cout << "--- two-level details ---\n";

  const std::vector<int> Trace = {1, 2, 1, 1};
  const RunResult Result       = runCache({"LRU", "LRU"}, {1, 1}, Trace);
  const std::vector<std::uint64_t> ExpectedLevels = {1, 1};
  const bool LevelsAreCorrect = Result.Hits == 2 && Result.Misses == 2 &&
                                Result.LevelHits == ExpectedLevels;
  check("levels: promotion", LevelsAreCorrect,
        "expected hits {1, 1}, got {" + std::to_string(Result.LevelHits[0]) +
            ", " + std::to_string(Result.LevelHits[1]) + "}");

  const RunResult CostResult =
      runCache({"LRU", "LRU"}, {1, 1}, Trace, {1.0, 10.0}, 100.0);
  check("cost: two levels", CostResult.Cost == 211.0,
        "expected 211, got " + std::to_string(CostResult.Cost));
}

// Exercise long-lived policy state without imposing a machine-specific limit.
static void testLargeRun() {

  std::cout << "--- large run: 1M accesses ---\n";

  constexpr std::size_t Capacity = 1024;
  constexpr std::size_t Length   = 1000000;
  std::vector<int> Trace;
  Trace.reserve(Length);
  for (std::size_t Index = 0; Index < Length; ++Index) {
    Trace.push_back(static_cast<int>(Index % Capacity));
  }

  const std::size_t ExpectedHits = Length - Capacity;
  for (const std::string &Policy : Policies) {
    const RunResult Result = runPolicy(Policy, Capacity, Trace);
    const bool Ok = Result.Hits == ExpectedHits && Result.Misses == Capacity;
    check(Policy + ": large run", Ok,
          "expected " + std::to_string(ExpectedHits) + " hits, got " +
              std::to_string(Result.Hits));
  }
}

int main() {

  testHandWrittenCases();
  testProperties();
  testTwoLevelDetails();
  testLargeRun();

  std::cout << '\n' << Failed << " failed\n";
  return Failed == 0 ? 0 : 1;
}
