#pragma once

#include <algorithm>
#include <iostream>
#include <list>
#include <unordered_map>

#include "../include/CachePolicy.hpp"

template <typename KeyType> class TwoQPolicy : public CachePolicy<KeyType> {
public:
  TwoQPolicy(std::size_t Size)
      : Capacity_(Size), A1Capacity_(std::max<std::size_t>(1, Size / 4)) {
  } // 25% to A1Capacity

  std::string getPolicyName() override { return "2Q"; }

  bool needInsertInCache(const KeyType &Key) override { return true; }

  void onCacheInsert(const KeyType &Key) override {
    if (Index_.size() >= Capacity_) {
      std::cerr << "[2Q] Error: try to insert extra element (corrupted)\n";
      return;
    }

    if (Index_.find(Key) != Index_.end()) {
      std::cerr << "[2Q] Error: try to insert existing key (corrupted)\n";
      return;
    }

    A1In_.push_front(Key);
    Index_[Key] = Entry{Queue::A1In, A1In_.begin()};
  }

  void onCacheHit(const KeyType &Key) override {
    auto It = Index_.find(Key);
    if (It == Index_.end()) {
      std::cerr << "[2Q] Error: hit on unknown key (corrupted)\n";
      return;
    }

    Entry &E = It->second;
    if (E.Where == Queue::Am) {
      Am_.splice(Am_.begin(), Am_, E.It);
    } else {
      Am_.splice(Am_.begin(), A1In_, E.It);
      E.Where = Queue::Am;
    }
  }

  void onCacheErase(const KeyType &Key) override {
    auto It = Index_.find(Key);
    if (It == Index_.end()) {
      std::cerr << "[2Q] Error: try to delete undefined element\n";
      return;
    }

    const Entry &E = It->second;
    if (E.Where == Queue::A1In) {
      A1In_.erase(E.It);
    } else {
      Am_.erase(E.It);
    }
    Index_.erase(It);
  }

  std::optional<KeyType> selectVictim() override {
    if (A1In_.size() > A1Capacity_) {
      return A1In_.back();
    }
    if (!Am_.empty()) {
      return Am_.back();
    }
    if (!A1In_.empty()) {
      return A1In_.back();
    }

    std::cerr << "[2Q] Warning: try to search victim in empty cache\n";
    return std::nullopt;
  }

private:
  enum class Queue {
    A1In,
    Am
  };

  using ListIt = typename std::list<KeyType>::iterator;

  struct Entry {
    Queue Where;
    ListIt It;
  };

  std::unordered_map<KeyType, Entry> Index_;

  std::list<KeyType> A1In_; // newcomers, FIFO
  std::list<KeyType> Am_;   // hot keys, LRU

  std::size_t Capacity_;   // total number of slots
  std::size_t A1Capacity_; // newcomers quota
};
