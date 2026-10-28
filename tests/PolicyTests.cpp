// Tests for cache policies. Build: cmake --build build, run:
// ./build/tests/policy_tests
//
// Each test is: policy name, capacity, access trace, expected number of hits.
// The expected numbers are worked out by hand in the comments
// (m = miss, h = hit, {...} = cache contents after the access).

#include "Cache.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

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

static void test(const std::string &Name, const std::string &Policy,
                 std::size_t Capacity, const std::vector<int> &Trace,
                 std::size_t Expected) {

  RunResult Result = runPolicy(Policy, Capacity, Trace);
  bool Ok          = Result.Hits == Expected;
  std::cout << (Ok ? "[ok]   " : "[FAIL] ") << Name << ": expected " << Expected
            << ", got " << Result.Hits << '\n';
  if (!Ok) {
    ++Failed;
  }
}

static void testCountInvariant() {

  const std::vector<int> Trace = {1, 2, 1, 3, 1};
  RunResult Result             = runPolicy("LRU", 2, Trace);
  const std::uint64_t Total    = Result.Hits + Result.Misses;
  bool Ok                      = Total == Trace.size();

  std::cout << (Ok ? "[ok]   " : "[FAIL] ")
            << "counts: hits + misses: expected " << Trace.size() << ", got "
            << Total << '\n';
  if (!Ok) {
    ++Failed;
  }
}

static void testLevels() {

  const std::vector<int> Trace = {1, 2, 1, 1};
  RunResult Result             = runCache({"LRU", "LRU"}, {1, 1}, Trace);
  const std::vector<std::uint64_t> Expected = {1, 1};
  bool Ok =
      Result.Hits == 2 && Result.Misses == 2 && Result.LevelHits == Expected;

  std::cout << (Ok ? "[ok]   " : "[FAIL] ")
            << "levels: promotion: expected hits {1, 1}, got {"
            << Result.LevelHits[0] << ", " << Result.LevelHits[1] << "}\n";
  if (!Ok) {
    ++Failed;
  }
}

static void testCost() {

  const std::vector<int> Trace = {1, 2, 1, 1};
  RunResult Result =
      runCache({"LRU", "LRU"}, {1, 1}, Trace, {1.0, 10.0}, 100.0);
  const double Expected = 211.0;
  bool Ok               = Result.Cost == Expected;

  std::cout << (Ok ? "[ok]   " : "[FAIL] ") << "cost: two levels: expected "
            << Expected << ", got " << Result.Cost << '\n';
  if (!Ok) {
    ++Failed;
  }
}

int main() {

  // Basic behaviour every policy must have.
  for (std::string P : {"LRU", "LFU", "2Q"}) {
    // 1m 1h 1h
    test(P + ": same key", P, 1, {1, 1, 1}, 2);
    // 1m 2m 1m 2m: capacity 1 can never hold both
    test(P + ": thrash", P, 1, {1, 2, 1, 2}, 0);
    // first pass all misses, second pass all hits
    test(P + ": set fits", P, 3, {1, 2, 3, 1, 2, 3}, 3);
  }

  // LRU
  // 1m 2m 1h {1,2}; 3m evicts 2 (older) {1,3}; 2m
  test("LRU: evicts oldest", "LRU", 2, {1, 2, 1, 3, 2}, 1);
  // loop of 4 keys in cache of 3: always evicts the next needed key
  test("LRU: loop", "LRU", 3, {1, 2, 3, 4, 1, 2, 3, 4, 1, 2, 3, 4}, 0);

  // LFU
  // 1m 1h 1h {1:3}; 2m {1:3,2:1}; 3m evicts 2 {1:3,3:1}; 2m evicts 3; 1h
  test("LFU: evicts rarest", "LFU", 2, {1, 1, 1, 2, 3, 2, 1}, 3);
  // 1 x3 {1:3}; 2,3,4,5 each evict the previous one-shot key; 1h
  test("LFU: keeps hot key", "LFU", 2, {1, 1, 1, 2, 3, 4, 5, 1}, 3);

  // 2Q, capacity 4: 1 is evicted to A1Out, then returns to Am on reuse.
  // The following scan evicts 1 from LRU, while 2Q keeps it in Am.
  const std::vector<int> HotTrace = {1, 2, 3, 4, 5, 1, 6, 7, 8, 9, 1};
  test("2Q: keeps hot key", "2Q", 4, HotTrace, 1);
  test("LRU: loses hot key", "LRU", 4, HotTrace, 0);

  // ARC adapts after misses in both ghost lists and then keeps key 2 hot.
  test("ARC: adapts to ghost hits", "ARC", 2, {1, 2, 1, 3, 2, 1, 2}, 2);

  // Reusing resident HIR key 4 promotes it to LIR, so it survives insertion 5.
  test("LIRS: promotes reused HIR", "LIRS", 4, {1, 2, 3, 4, 4, 5, 4}, 2);

  // OPT evicts 2 before request 3 because 1 is needed earlier in the future.
  test("OPT: evicts farthest use", "OPT", 2, {1, 2, 3, 1, 2, 3}, 2);

  testCountInvariant();
  testLevels();
  testCost();

  std::cout << '\n' << Failed << " failed\n";
  return Failed == 0 ? 0 : 1;
}
