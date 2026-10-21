// Tests for cache policies. Build: cmake --build build, run: ./build/policy_tests
//
// Each test is: policy name, capacity, access trace, expected number of hits.
// The expected numbers are worked out by hand in the comments
// (m = miss, h = hit, {...} = cache contents after the access).

#include "../include/Cache.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

static int Failed = 0;

// Run one cache level with the given policy on the trace and return hits.
// MultiLevelCache reads the policy name from a config file and the capacity
// from std::cin, so we write a temporary file and feed std::cin from a string.
static std::size_t runPolicy(const std::string &Policy, std::size_t Capacity,
                             const std::vector<int> &Trace) {
  std::ofstream("test_config.tmp") << "1\n" << Policy << '\n';

  std::istringstream FakeInput(std::to_string(Capacity));
  std::streambuf *RealInput = std::cin.rdbuf(FakeInput.rdbuf());

  MultiLevelCache<int, int> Cache("test_config.tmp");
  for (int Key : Trace) {
    Cache.accessElement(Key);
  }

  std::cin.rdbuf(RealInput);
  std::remove("test_config.tmp");
  return Cache.HitCount_;
}

static void test(const std::string &Name, const std::string &Policy,
                 std::size_t Capacity, const std::vector<int> &Trace,
                 std::size_t Expected) {
  std::size_t Got = runPolicy(Policy, Capacity, Trace);
  bool Ok = Got == Expected;
  std::cout << (Ok ? "[ok]   " : "[FAIL] ") << Name << ": expected "
            << Expected << ", got " << Got << '\n';
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

  // 2Q, capacity 4 -> A1In quota 1
  // 1m 1h (1 -> Am); 2m 3m 4m (A1In = 4 3 2); 5m: A1In > quota -> evicts 2;
  // 6m evicts 3; 1h still in Am.  LRU on the same trace evicts 1 at "5".
  test("2Q: keeps hot key", "2Q", 4, {1, 1, 2, 3, 4, 5, 6, 1}, 2);
  test("LRU: loses hot key", "LRU", 4, {1, 1, 2, 3, 4, 5, 6, 1}, 1);

  std::cout << '\n' << Failed << " failed\n";
  return Failed == 0 ? 0 : 1;
}
