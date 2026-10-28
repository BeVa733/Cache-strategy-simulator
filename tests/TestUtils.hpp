#pragma once

// Shared by PolicyTests.cpp and Compare.cpp: run the real cache on a trace,
// and an ideal (Belady) cache to compare against.

#include "../include/Cache.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using Trace = std::vector<int>;

// Run MultiLevelCache with the given levels on the trace and return hits.
// The cache reads policy names from a config file, so we write a temporary one.
inline std::size_t runSystem(const std::vector<std::string> &Policies,
                             const std::vector<std::size_t> &Capacities,
                             const Trace &Keys) {
  std::ofstream Config("test_config.tmp");
  Config << Policies.size() << '\n';
  for (const auto &P : Policies) {
    Config << P << '\n';
  }
  Config.close();

  MultiLevelCache<int> Cache("test_config.tmp", Capacities, Keys);
  for (int Key : Keys) {
    Cache.accessElement(Key);
  }

  std::remove("test_config.tmp");
  return Cache.hitCount();
}

// One level with one policy.
inline std::size_t runPolicy(const std::string &Policy, std::size_t Capacity,
                             const Trace &Keys) {
  return runSystem({Policy}, {Capacity}, Keys);
}

// Ideal cache (Belady). We know the whole trace in advance, so on eviction we
// drop the key whose next use is farthest away. No real policy can beat it.
inline std::size_t runIdeal(std::size_t Capacity, const Trace &Keys) {
  const std::size_t Never = Keys.size();
  std::vector<std::size_t> NextUse(Keys.size(), Never);
  std::unordered_map<int, std::size_t> LastSeen;
  for (std::size_t I = Keys.size(); I-- > 0;) {
    auto It = LastSeen.find(Keys[I]);
    if (It != LastSeen.end()) {
      NextUse[I] = It->second;
    }
    LastSeen[Keys[I]] = I;
  }

  std::unordered_map<int, std::size_t> Cache; // key -> next use
  std::size_t Hits = 0;
  for (std::size_t I = 0; I < Keys.size(); ++I) {
    if (Cache.count(Keys[I])) {
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
    Cache[Keys[I]] = NextUse[I];
  }
  return Hits;
}
