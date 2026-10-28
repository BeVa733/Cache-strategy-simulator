#pragma once

#include "../policies/2Q.hpp"
#include "../policies/LFU.hpp"
#include "../policies/LRU.hpp"
#include "../policies/OPT.hpp"
#include "CachePolicy.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <istream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/// Forward declaration of a cache level used by MultiLevelCache.
template <typename KeyType> class CacheLevel;

template <typename KeyType> class MultiLevelCache {

  /// Class that coordinates lookup and movement of keys across cache levels.

public:
  /// Vector of cache levels ordered according to lookup priority.
  using CacheLevelVector = std::vector<std::unique_ptr<CacheLevel<KeyType>>>;

  /// Constructor with reading config file.
  explicit MultiLevelCache(const char *ConfigFilename,
                           const std::vector<std::size_t> &Capacities,
                           const std::vector<KeyType> &RequestTrace) {

    using PolicyPtr = std::unique_ptr<CachePolicy<KeyType>>;

    const std::unordered_map<
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
    };

    std::ifstream ConfigFile(ConfigFilename);
    if (!ConfigFile) {
      throw std::runtime_error(std::string("Unable to open config file: ") +
                               ConfigFilename);
    }

    if (!(ConfigFile >> NumLevels_) || NumLevels_ == 0) {
      throw std::runtime_error(
          "Incorrect config: expected a positive number of cache levels");
    }

    if (Capacities.size() != NumLevels_) {
      throw std::runtime_error(
          "Number of capacities does not match cache levels");
    }

    LevelCosts_.assign(NumLevels_, 1.0);
    LevelHitCounts_.assign(NumLevels_, 0);

    for (std::size_t Level = 0; Level < NumLevels_; ++Level) {
      std::string PolicyName;
      if (!(ConfigFile >> PolicyName)) {
        throw std::runtime_error(
            "Incorrect config: expected policy for level " +
            std::to_string(Level + 1));
      }

      const auto ConstructorIt = Constructors.find(PolicyName);
      if (ConstructorIt == Constructors.end()) {
        throw std::runtime_error("Unknown cache policy: " + PolicyName);
      }

      const std::size_t LevelCapacity = Capacities[Level];
      CacheLevels.emplace_back(std::make_unique<CacheLevel<KeyType>>(
          LevelCapacity, ConstructorIt->second(LevelCapacity, RequestTrace)));
    }

    readOptionalCosts(ConfigFile);
  }

  /// Process one key. Return true on a hit and false on a miss.
  bool accessElement(const KeyType &Key) {
    for (auto &CL : CacheLevels) {
      CL->Policy->onRequest(Key);
    }

    std::size_t FoundLevel = CacheLevels.size();

    for (std::size_t Level = 0; Level < CacheLevels.size(); ++Level) {
      if (!CacheLevels[Level]->findElement(Key)) {
        continue;
      }

      FoundLevel = Level;
      ++HitCount_;
      ++LevelHitCounts_[Level];
      TotalCost_ += LevelCosts_[Level];
      CacheLevels[Level]->Policy->onCacheHit(Key);

      if (Level == 0) {
        return true;
      }

      if (!CacheLevels[Level]->eraseElement(Key)) {
        throw std::logic_error("Unable to promote cached key");
      }
      CacheLevels[Level]->Policy->onCacheErase(Key);
      break;
    }

    if (FoundLevel == CacheLevels.size()) {
      ++MissCount_;
      TotalCost_ += MissCost_;
    }

    KeyType PendingKey = Key;

    for (auto &CL : CacheLevels) {
      if (!CL->hasCapacity()) {
        continue;
      }

      if (!CL->Policy->needInsertInCache(PendingKey)) {
        continue;
      }

      if (CL->hasFreeSpace()) {
        if (!CL->insertElement(PendingKey)) {
          throw std::logic_error("Duplicate key during cache insertion");
        }
        CL->Policy->onCacheInsert(PendingKey);
        break;
      }

      const std::optional<KeyType> VictimKey = CL->Policy->selectVictim();
      if (!VictimKey || !CL->eraseElement(*VictimKey)) {
        throw std::logic_error("Replacement policy selected no valid victim");
      }
      CL->Policy->onCacheErase(*VictimKey);

      if (!CL->insertElement(PendingKey)) {
        throw std::logic_error("Duplicate key during cache replacement");
      }
      CL->Policy->onCacheInsert(PendingKey);

      PendingKey = *VictimKey;
    }

    return FoundLevel != CacheLevels.size();
  }

  std::uint64_t hitCount() const { return HitCount_; }
  std::uint64_t missCount() const { return MissCount_; }
  const std::vector<std::uint64_t> &levelHitCounts() const {
    return LevelHitCounts_;
  }
  double totalCost() const { return TotalCost_; }

private:
  void readOptionalCosts(std::istream &ConfigFile) {
    std::string Option;
    while (ConfigFile >> Option) {
      if (Option == "costs") {
        for (double &Cost : LevelCosts_) {
          if (!(ConfigFile >> Cost) || !std::isfinite(Cost) || Cost < 0.0) {
            throw std::runtime_error(
                "Incorrect config: expected one non-negative cost per level");
          }
        }
      } else if (Option == "miss_cost") {
        if (!(ConfigFile >> MissCost_) || !std::isfinite(MissCost_) ||
            MissCost_ < 0.0) {
          throw std::runtime_error(
              "Incorrect config: expected a non-negative miss cost");
        }
      } else {
        throw std::runtime_error("Unknown config option: " + Option);
      }
    }
  }

  std::size_t NumLevels_{0};
  CacheLevelVector CacheLevels;
  std::vector<double> LevelCosts_;
  std::vector<std::uint64_t> LevelHitCounts_;
  double MissCost_{1.0};
  double TotalCost_{0.0};

public:
  std::uint64_t HitCount_{0};
  std::uint64_t MissCount_{0};
};

template <typename KeyType> class CacheLevel {

  /// A single cache level that stores keys and applies one replacement policy.

public:
  CacheLevel(std::size_t Capacity,
             std::unique_ptr<CachePolicy<KeyType>> ReplacementPolicy)
      : Capacity_(Capacity), Policy(std::move(ReplacementPolicy)) {}

  /// Return true if Key is stored in this level.
  bool findElement(const KeyType &Key) const {
    return Map.find(Key) != Map.end();
  }

  /// Insert Key into this level.
  bool insertElement(const KeyType &Key) {
    return Map.emplace(Key, true).second;
  }

  /// Remove Key from this level.
  bool eraseElement(const KeyType &Key) { return Map.erase(Key) != 0; }

  /// Return true if there is enough space for one key.
  bool hasFreeSpace() const { return Map.size() < Capacity_; }

  bool hasCapacity() const { return Capacity_ != 0; }

private:
  std::size_t Capacity_;

  // The mapped flag is not used: unordered_map is intentionally kept as the
  // storage structure of the original project, while values are not simulated.
  std::unordered_map<KeyType, bool> Map;

public:
  std::unique_ptr<CachePolicy<KeyType>> Policy;
};
