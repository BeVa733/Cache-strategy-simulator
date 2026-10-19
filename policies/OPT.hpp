#pragma once

#include "../include/CachePolicy.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

// OPT (Belady's algorithm) — theoretical optimal policy.
// Victim is the key whose next use is farthest in the future.
// Requires the whole request trace in advance — used as a baseline.
template <typename KeyType> class OPTPolicy : public CachePolicy<KeyType> {
public:
  OPTPolicy(std::size_t capacity, const std::vector<KeyType> &requestTrace)
      : capacity_(capacity), requestTrace_(requestTrace) {}

  std::string getPolicyName() override { return "OPT"; }

  // Caller must invoke it for each request.
  void onRequest(const KeyType &key) override {
    if (currentPosition_ >= requestTrace_.size()) {
      throw std::out_of_range("OPT: request trace is exhausted");
    }

    // Key must match trace at current position.
    if (!(requestTrace_[currentPosition_] == key)) {
      throw std::logic_error("OPT: request does not match the trace");
    }

    ++currentPosition_;
  }

  bool needInsertInCache(const KeyType &) override { return capacity_ > 0; }

  void onCacheInsert(const KeyType &key) override {
    if (residents_.size() >= capacity_) {
      throw std::logic_error("OPT: cache capacity exceeded");
    }

    // insert().second == false means key already present.
    if (!residents_.insert(key).second) {
      throw std::logic_error("OPT: duplicate cache insertion");
    }
  }

  // OPT does not react to hits: victim choice depends only on the future.
  void onCacheHit(const KeyType &key) override {
    if (residents_.find(key) == residents_.end()) {
      throw std::logic_error("OPT: hit on a non-resident key");
    }
  }

  void onCacheErase(const KeyType &key) override {
    if (residents_.erase(key) == 0) {
      throw std::logic_error("OPT: erasing a non-resident key");
    }
  }

  std::optional<KeyType> selectVictim() override {
    if (residents_.empty()) {
      return std::nullopt;
    }

    auto victim                 = residents_.begin();
    std::size_t farthestNextUse = 0;

    for (auto it = residents_.begin(); it != residents_.end(); ++it) {

      std::size_t nextUse = currentPosition_;

      while (nextUse < requestTrace_.size() &&
             !(requestTrace_[nextUse] == *it)) {
        ++nextUse;
      }

      // Element will not be in the future — evict immediately.
      if (nextUse == requestTrace_.size()) {
        return *it;
      }

      // Else find farthest next use.
      if (nextUse > farthestNextUse) {
        farthestNextUse = nextUse;
        victim          = it;
      }
    }

    return *victim;
  }

private:
  std::size_t capacity_;
  const std::vector<KeyType> &requestTrace_;
  std::size_t currentPosition_ = 0;
  std::unordered_set<KeyType> residents_;
};
