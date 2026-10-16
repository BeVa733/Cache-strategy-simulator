#pragma once

#include "../policies/2Q.hpp"
#include "../policies/ARC.hpp"
#include "../policies/LFU.hpp"
#include "../policies/LRU.hpp"
#include "../policies/OPT.hpp"
#include "CachePolicy.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

/// Forward declaration of a cache level used by MultiLevelCache.
template <typename ValueType, typename KeyType> class CacheLevel;

template <typename ValueType, typename KeyType> class MultiLevelCache {

  /// Class that coordinates lookup and movement of elements across cache
  /// levels.

public:
  /// Vector of cache levels ordered according to lookup priority.
  using CacheLevelVector =
      std::vector<std::unique_ptr<CacheLevel<ValueType, KeyType>>>;

  /// Constructor with reading config file
  explicit MultiLevelCache(const char *ConfigFilename,
                           const std::vector<KeyType> &RequestTrace) {

    using PolicyPtr = std::unique_ptr<CachePolicy<KeyType>>;

    // this hash map make connection between string policy names and class
    // costructors. Append new policies here after impementation
    std::unordered_map<
        std::string,
        std::function<PolicyPtr(std::size_t, const std::vector<KeyType> &)>>
        Constructors{
            {"LRU",
             [](std::size_t Capacity, const std::vector<KeyType> &) {
               return std::make_unique<LRUPolicy<KeyType>>(Capacity);
             }},

            {"LFU",
             [](std::size_t Capacity, const std::vector<KeyType> &) {
               return std::make_unique<LFUPolicy<KeyType>>(Capacity);
             }},

            {"2Q",
             [](std::size_t Capacity, const std::vector<KeyType> &) {
               return std::make_unique<TwoQPolicy<KeyType>>(Capacity);
             }},

            {"ARC",
             [](std::size_t Capacity, const std::vector<KeyType> &) {
               return std::make_unique<ARCPolicy<KeyType>>(Capacity);
             }},

            {"OPT",
             [](std::size_t Capacity,
                const std::vector<KeyType> &Trace) {
               return std::make_unique<OPTPolicy<KeyType>>(Capacity, Trace);
             }},
        };

    std::ifstream ConfigFile(ConfigFilename);
    if (!ConfigFile) {
      throw std::runtime_error(std::string("Unable to open config file: ") +
                               ConfigFilename);
    }

    // Read number of cache levels
    if (!(ConfigFile >> NumLevels_)) {
      throw std::runtime_error(
          "Incorrect config format: expected number of cache levels");
    }

    // read policy names in cycle
    for (std::size_t i = 0; i < NumLevels_; ++i) {
      std::string PolicyName;
      if (!(ConfigFile >> PolicyName)) {
        throw std::runtime_error(
            "Incorrect config format: expected policy name for level " +
            std::to_string(i));
      }

      // find policy in hash map
      auto It = Constructors.find(PolicyName);
      if (It == Constructors.end()) {
        throw std::runtime_error("Unknown cache policy: " + PolicyName);
      }

      // get level capacity from stdin
      std::size_t LevelCapacity;
      if (!(std::cin >> LevelCapacity)) {
        throw std::runtime_error("Unable to read capacity for cache level " +
                                 std::to_string(i));
      }

      // Add cache level into levels vector
      CacheLevels.emplace_back(std::make_unique<CacheLevel<ValueType, KeyType>>(
          LevelCapacity, It->second(LevelCapacity, RequestTrace)));
    }
  }

  /// Return a cached value for \p Key or std::nullopt after a cache miss.
  std::optional<ValueType *> accessElement(KeyType Key) {
    ValueType *Value = nullptr;
    std::size_t FoundLevel = CacheLevels.size();

    // Complete the lookup before accessing the slow data source.
    for (std::size_t Level = 0; Level < CacheLevels.size(); ++Level) {
      std::optional<ValueType *> CachedValue =
          CacheLevels[Level]->findElement(Key);
      if (!CachedValue) {
        continue;
      }

      Value      = *CachedValue;
      FoundLevel = Level;
      ++HitCount_;

      CacheLevels[Level]->Policy->onCacheHit(Key);

      // Don't need promotion if in 1 level
      if (Level == 0) {
        return Value;
      }

      CacheLevels[Level]->eraseElement(Key);
      CacheLevels[Level]->Policy->onCacheErase(Key);
      break;
    }

    if (FoundLevel == CacheLevels.size()) {
      ++MissCount_;
      Value = slowGetPage(Key); // return nullopt if no Page, but in this task
                                // in couldn't happend
    }

    // Insert into 1 level. Every displaced entry becomes the candidate for the
    // next level; a candidate displaced from the last level is delete.
    KeyType PendingKey      = Key;
    ValueType *PendingValue = Value;

    for (auto &CL : CacheLevels) {
      if (!CL->Policy->needInsertInCache(PendingKey)) {
        continue;
      }

      if (CL->hasFreeSpace()) {
        if (CL->insertElement(PendingKey, PendingValue)) {
          CL->Policy->onCacheInsert(PendingKey);
        }
        break;
      }

      std::optional<KeyType> VictimKey = CL->Policy->selectVictim();
      if (!VictimKey) {
        continue;
      }

      std::optional<ValueType *> VictimValue = CL->eraseElement(*VictimKey);
      if (!VictimValue) {
        continue;
      }
      CL->Policy->onCacheErase(*VictimKey);

      if (CL->insertElement(PendingKey, PendingValue)) {
        CL->Policy->onCacheInsert(PendingKey);
      }

      PendingKey   = *VictimKey;
      PendingValue = *VictimValue;
    }

    return Value;
  }

  /// Function for immitate slow process of getting value
  ValueType *slowGetPage(KeyType Key) {
    (void)Key; // подавить предупреждение о неиспользуемом параметре
    return nullptr;
  }

private:
  /// Cache levels number
  std::size_t NumLevels_{0};

  /// Cache levels searched by access().
  CacheLevelVector CacheLevels;

public:
  /// Number of successful lookups.
  uint32_t HitCount_{0};
  /// Number of unsuccessful lookups.
  uint32_t MissCount_{0};
};

template <typename ValueType, typename KeyType> class CacheLevel {

  /// A single cache level that stores values and applies one replacement
  /// policy.

public:
  CacheLevel(std::size_t Capacity, std::unique_ptr<CachePolicy<KeyType>> Policy)
      : Capacity_(Capacity), Policy(std::move(Policy)) {}

  /// Return the value associated with \p Key or std::nullopt if it exists.
  std::optional<ValueType *> findElement(const KeyType &Key) {

    auto It = Map.find(Key);

    if (It != Map.end()) {
      return It->second;
    } else {
      return std::nullopt;
    }
  }

  /// Insert \p Key with \p Value in cache
  bool insertElement(const KeyType &Key, ValueType *Value) {

    if (Map.find(Key) == Map.end()) {
      Map.emplace(Key, Value);
      return true;
    } else {
      std::cerr
          << "[CACHE] Warning: try to insert new element with existing key\n";
      return false;
    }
  }

  /// Remove \p Key and return its value for promotion or cascading eviction.
  std::optional<ValueType *> eraseElement(const KeyType &Key) {
    auto It = Map.find(Key);
    if (It == Map.end()) {
      return std::nullopt;
    }

    ValueType *Value = It->second;
    Map.erase(It);
    return Value;
  }

  /// Return true if is enough space for one element in Cache
  bool hasFreeSpace(void) { return Map.size() < Capacity_; }

private:
  /// Max number of elements in this cache level
  std::size_t Capacity_;

  /// Values currently stored in this cache level
  std::unordered_map<KeyType, ValueType *> Map;

public:
  /// Replacement policy used when this level.
  std::unique_ptr<CachePolicy<KeyType>> Policy;
};