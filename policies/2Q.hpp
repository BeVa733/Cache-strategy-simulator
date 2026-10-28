#pragma once

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "CachePolicy.hpp"

template <typename KeyType> class TwoQPolicy : public CachePolicy<KeyType> {
public:
  TwoQPolicy(std::size_t Size)
      : Capacity_(Size), A1Capacity_(std::max<std::size_t>(1, Size / 4)),
        A1OutCapacity_(std::max<std::size_t>(1, Size / 2)) {
  } // 25% to A1Capacity

  std::string getPolicyName() const override { return "2Q"; }

  bool needInsertInCache(const KeyType &Key) override {
    InsertInAm_ = false;
    VictimToA1Out_.reset();

    if (Capacity_ == 0) {
      return false;
    }

    auto It = A1OutIndex_.find(Key);
    if (It != A1OutIndex_.end()) {
      A1Out_.erase(It->second);
      A1OutIndex_.erase(It);
      InsertInAm_ = true;
    }

    return true;
  }

  void onCacheInsert(const KeyType &Key) override {
    if (Index_.size() >= Capacity_) {
      std::cerr << "[2Q] Error: try to insert extra element (corrupted)\n";
      return;
    }

    if (Index_.find(Key) != Index_.end()) {
      std::cerr << "[2Q] Error: try to insert existing key (corrupted)\n";
      return;
    }

    if (InsertInAm_) {
      Am_.push_front(Key);
      Index_[Key] = Entry{Queue::Am, Am_.begin()};
    } else {
      A1In_.push_front(Key);
      Index_[Key] = Entry{Queue::A1In, A1In_.begin()};
    }

    InsertInAm_ = false;
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
      if (VictimToA1Out_ && *VictimToA1Out_ == Key) {
        addToA1Out(Key);
      }
    } else {
      Am_.erase(E.It);
    }
    Index_.erase(It);
    VictimToA1Out_.reset();
  }

  std::optional<KeyType> selectVictim() override {
    VictimToA1Out_.reset();

    if (A1In_.size() > A1Capacity_) {
      VictimToA1Out_ = A1In_.back();
      return VictimToA1Out_;
    }
    if (!Am_.empty()) {
      return Am_.back();
    }
    if (!A1In_.empty()) {
      VictimToA1Out_ = A1In_.back();
      return VictimToA1Out_;
    }

    std::cerr << "[2Q] Warning: try to search victim in empty cache\n";
    return std::nullopt;
  }

private:
  void addToA1Out(const KeyType &Key) {
    A1Out_.push_front(Key);
    A1OutIndex_[Key] = A1Out_.begin();

    while (A1Out_.size() > A1OutCapacity_) {
      A1OutIndex_.erase(A1Out_.back());
      A1Out_.pop_back();
    }
  }

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

  std::list<KeyType> A1In_;  // newcomers, FIFO
  std::list<KeyType> Am_;    // hot keys, LRU
  std::list<KeyType> A1Out_; // ghost queue for evicted newcomers
  std::unordered_map<KeyType, ListIt> A1OutIndex_;

  std::size_t Capacity_;   // total number of slots
  std::size_t A1Capacity_; // newcomers quota
  std::size_t A1OutCapacity_;

  bool InsertInAm_{false};
  std::optional<KeyType> VictimToA1Out_;
};
