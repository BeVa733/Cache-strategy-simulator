// Compare policies on generated access patterns. Run: ./build/compare
//
// Prints hit rate (%) of every policy on every pattern, plus the hit rate of
// an ideal cache (Belady: evicts the key needed farthest in the future).

#include "../include/Cache.hpp"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

static const std::size_t Capacity = 32;
static const std::size_t Length = 5000;

static std::size_t runPolicy(const std::string &Policy,
                             const std::vector<int> &Trace) {
  std::ofstream("cmp_config.tmp") << "1\n" << Policy << '\n';

  std::istringstream FakeInput(std::to_string(Capacity));
  std::streambuf *RealInput = std::cin.rdbuf(FakeInput.rdbuf());

  MultiLevelCache<int, int> Cache("cmp_config.tmp");
  for (int Key : Trace) {
    Cache.accessElement(Key);
  }

  std::cin.rdbuf(RealInput);
  std::remove("cmp_config.tmp");
  return Cache.HitCount_;
}

// Ideal cache. For every position we know when the key is used next, and
// on eviction we drop the key whose next use is farthest away.
static std::size_t runIdeal(const std::vector<int> &Trace) {
  const std::size_t Never = Trace.size();
  std::vector<std::size_t> NextUse(Trace.size(), Never);
  std::unordered_map<int, std::size_t> LastSeen;
  for (std::size_t I = Trace.size(); I-- > 0;) {
    if (LastSeen.count(Trace[I])) {
      NextUse[I] = LastSeen[Trace[I]];
    }
    LastSeen[Trace[I]] = I;
  }

  std::unordered_map<int, std::size_t> Cache; // key -> next use
  std::size_t Hits = 0;
  for (std::size_t I = 0; I < Trace.size(); ++I) {
    if (Cache.count(Trace[I])) {
      ++Hits;
    } else if (Cache.size() >= Capacity) {
      auto Victim = Cache.begin();
      for (auto It = Cache.begin(); It != Cache.end(); ++It) {
        if (It->second > Victim->second) {
          Victim = It;
        }
      }
      Cache.erase(Victim);
    }
    Cache[Trace[I]] = NextUse[I];
  }
  return Hits;
}

// ---- access patterns -------------------------------------------------------

static std::mt19937 Rng(1);

static int randomKey(int From, int To) {
  return std::uniform_int_distribution<int>(From, To)(Rng);
}

// Keys from a range 4x the cache, no locality.
static std::vector<int> uniform() {
  std::vector<int> T(Length);
  for (int &K : T) {
    K = randomKey(1, 4 * Capacity);
  }
  return T;
}

// Cyclic scan a bit larger than the cache: LRU's worst case.
static std::vector<int> loop() {
  std::vector<int> T(Length);
  for (std::size_t I = 0; I < Length; ++I) {
    T[I] = I % (Capacity + Capacity / 4) + 1;
  }
  return T;
}

// Small hot set, every third access is a key that never repeats.
static std::vector<int> hotPlusScan() {
  std::vector<int> T(Length);
  int Cold = 10000;
  for (std::size_t I = 0; I < Length; ++I) {
    T[I] = (I % 3 == 2) ? Cold++ : randomKey(1, Capacity / 2);
  }
  return T;
}

// Hot set switches to a different one halfway through.
static std::vector<int> phaseChange() {
  std::vector<int> T(Length);
  for (std::size_t I = 0; I < Length; ++I) {
    T[I] = (I < Length / 2) ? randomKey(1, Capacity / 2)
                            : randomKey(1000, 1000 + Capacity / 2);
  }
  return T;
}

int main() {
  const std::vector<std::string> Policies = {"LRU", "LFU", "2Q"};

  struct Pattern {
    std::string Name;
    std::vector<int> (*Generate)();
  };
  const std::vector<Pattern> Patterns = {
      {"uniform", uniform},
      {"loop", loop},
      {"hot + scan", hotPlusScan},
      {"phase change", phaseChange},
  };

  std::cout << "capacity " << Capacity << ", " << Length
            << " accesses, hit rate %\n\n";
  std::cout << std::left << std::setw(14) << "pattern";
  for (const auto &P : Policies) {
    std::cout << std::right << std::setw(8) << P;
  }
  std::cout << std::setw(8) << "ideal" << '\n';

  std::cout << std::fixed << std::setprecision(1);
  for (const auto &Pat : Patterns) {
    std::vector<int> Trace = Pat.Generate();
    std::cout << std::left << std::setw(14) << Pat.Name;
    for (const auto &P : Policies) {
      std::cout << std::right << std::setw(8)
                << 100.0 * runPolicy(P, Trace) / Length;
    }
    std::cout << std::setw(8) << 100.0 * runIdeal(Trace) / Length << '\n';
  }
  return 0;
}
