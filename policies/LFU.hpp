#pragma once

#include <iostream>
#include <unordered_map>

#include "CachePolicy.hpp"

template <typename KeyType> class LFUPolicy : public CachePolicy<KeyType> {
public:
  LFUPolicy(std::size_t Size) : Capacity_(Size) {}

  std::string getPolicyName() const override { return "LFU"; }

  bool needInsertInCache(const KeyType &Key) override { return true; }

  void onCacheInsert(const KeyType &Key) override {
    if (Freq_.size() >= Capacity_) {
      std::cerr << "[LFU] Error: try to insert extra element (corrupted)\n";
      return;
    }
    Freq_[Key] = 1;
  }

  void onCacheHit(const KeyType &Key) override {
    auto It = Freq_.find(Key);
    if (It == Freq_.end()) { // if reached the end and not found anything
      std::cerr << "[LFU] Error: hit on unknown key (corrupted)\n";
      return;
    }
    ++It->second;
  }

  void onCacheErase(const KeyType &Key) override {
    if (Freq_.erase(Key) == 0) { // the erase method of the Freq_ object
      std::cerr << "[LFU] Error: try to delete undefined element\n";
    }
  }

  std::optional<KeyType> selectVictim() override {
    if (Freq_.empty()) {
      std::cerr << "[LFU] Warning: try to search victim in empty cache\n";
      return std::nullopt;
    }
    auto Victim = Freq_.begin();
    for (auto It = Freq_.begin(); It != Freq_.end(); ++It) {
      if (It->second < Victim->second) {
        Victim = It;
      }
    }
    return Victim->first;
  }

private:
  std::unordered_map<KeyType, std::size_t> Freq_;
  std::size_t Capacity_;
};
