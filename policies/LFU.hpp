#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

#include "CachePolicy.hpp"

template <typename KeyType> class LFUPolicy : public CachePolicy<KeyType> {
public:
  LFUPolicy(std::size_t) {}

  std::string getPolicyName() const override { return "LFU"; }

  bool needInsertInCache(const KeyType &Key) override { return true; }

  void onCacheInsert(const KeyType &Key) override { Freq_[Key] = 1; }

  void onCacheHit(const KeyType &Key) override { ++Freq_.find(Key)->second; }

  void onCacheErase(const KeyType &Key) override { Freq_.erase(Key); }

  std::optional<KeyType> selectVictim() override {
    if (Freq_.empty())
      return std::nullopt;

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
};
