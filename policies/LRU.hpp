#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "CachePolicy.hpp"

template <typename KeyType> class LRUPolicy : public CachePolicy<KeyType> {

public:
  explicit LRUPolicy(std::size_t) {}

  std::string getPolicyName() const override { return "LRU"; }

  bool needInsertInCache(const KeyType &) override { return true; }

  void onCacheInsert(const KeyType &Key) override {
    List_.push_front(Key);
    Positions_.emplace(Key, List_.begin());
  }

  void onCacheHit(const KeyType &Key) override {
    List_.splice(List_.begin(), List_, Positions_.find(Key)->second);
  }

  void onCacheErase(const KeyType &Key) override {
    auto It = Positions_.find(Key);
    List_.erase(It->second);
    Positions_.erase(It);
  }

  std::optional<KeyType> selectVictim(void) override {
    if (List_.empty())
      return std::nullopt;

    return List_.back();
  }

private:
  std::list<KeyType> List_;
  std::unordered_map<KeyType, typename std::list<KeyType>::iterator> Positions_;
};
