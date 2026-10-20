#pragma once

#include <iostream>
#include <list>
#include <unordered_map>

#include "../include/CachePolicy.hpp"

template <typename KeyType> class LRUPolicy : public CachePolicy<KeyType> {

public:
  explicit LRUPolicy(std::size_t Size) : Capacity_(Size) {}

  std::string getPolicyName() const override { return "LRU"; }

  bool needInsertInCache(const KeyType &) override { return true; }

  void onCacheInsert(const KeyType &Key) override {
    if (Positions_.find(Key) != Positions_.end()) {
      std::cerr << "[LRU] Error: try to insert duplicate element (corrupted)\n";
      return;
    }

    if (List_.size() >= Capacity_) {
      std::cerr << "[LRU] Error: try to insert extra element (corrupted)\n";
      return;
    }

    List_.push_front(Key);
    Positions_.emplace(Key, List_.begin());
  }

  void onCacheHit(const KeyType &Key) override {
    auto It = Positions_.find(Key);
    if (It == Positions_.end()) {
      std::cerr << "[LRU] Error: try to access undefined element (corrupted)\n";
      return;
    }

    List_.splice(List_.begin(), List_, It->second);
  }

  void onCacheErase(const KeyType &Key) override {
    auto It = Positions_.find(Key);
    if (It == Positions_.end()) {
      std::cerr << "[LRU] Error: try to delete undefined element \n";
      return;
    }

    List_.erase(It->second);
    Positions_.erase(It);
  }

  std::optional<KeyType> selectVictim(void) override {
    if (!List_.empty()) {
      return List_.back();
    } else {
      std::cerr << "[LRU] Warning: try to search victim in empty cache\n";
      return std::nullopt;
    }
  }

private:
  std::list<KeyType> List_;
  std::unordered_map<KeyType, typename std::list<KeyType>::iterator> Positions_;

  std::size_t Capacity_;
};
