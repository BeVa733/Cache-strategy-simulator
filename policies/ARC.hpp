#pragma once

#include "CachePolicy.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <list>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

template <typename KeyType> class ARCPolicy : public CachePolicy<KeyType> {

private:
  enum class ListType {
    FreshCache_,
    HotCache_,
    FreshGhosts_,
    HotGhosts_
  };

  struct Entry {
    ListType List;
    typename std::list<KeyType>::iterator Iterator;
  };

public:
  explicit ARCPolicy(std::size_t Capacity)
      : Capacity_(Capacity), TargetSize_(0) {}

  std::string getPolicyName() const override { return "ARC"; }

  bool needInsertInCache(const KeyType &Key) override {

    InsertAsHotCache_         = false;
    VictimGoesToGhost_        = false;
    LastMissWasFromHotGhosts_ = false;
    DiscardFreshVictim_       = false;

    if (Capacity_ == 0) {
      return false;
    }

    auto It = Entries_.find(Key);

    if (It != Entries_.end()) {

      // Key comeback from FreshGhosts_.
      // Increase TargetSize_.
      if (It->second.List == ListType::FreshGhosts_) {

        std::size_t Delta =
            FreshGhosts_.empty()
                ? 1
                : std::max<std::size_t>(1, HotGhosts_.size() /
                                               FreshGhosts_.size());

        TargetSize_ = std::min(Capacity_, TargetSize_ + Delta);
        eraseEntry(Key);

        InsertAsHotCache_  = true;
        VictimGoesToGhost_ = true;
        return true;
      }

      // Key comeback from HotGhosts_.
      // Decrease TargetSize_.
      if (It->second.List == ListType::HotGhosts_) {

        std::size_t Delta =
            HotGhosts_.empty()
                ? 1
                : std::max<std::size_t>(1, FreshGhosts_.size() /
                                               HotGhosts_.size());

        TargetSize_ = (TargetSize_ >= Delta) ? (TargetSize_ - Delta) : 0;
        eraseEntry(Key);

        LastMissWasFromHotGhosts_ = true;
        InsertAsHotCache_         = true;
        VictimGoesToGhost_        = true;
        return true;
      }
    }

    std::size_t T1PlusB1 = FreshCache_.size() + FreshGhosts_.size();

    if (T1PlusB1 == Capacity_) {

      if (FreshCache_.size() < Capacity_) {
        removeLRU(ListType::FreshGhosts_);
        VictimGoesToGhost_ = true;
      } else {
        // When T1 itself occupies the whole cache, ARC discards its LRU key
        // instead of adding it to B1.
        DiscardFreshVictim_ = true;
      }
    } else {

      std::size_t DirectorySize = FreshCache_.size() + HotCache_.size() +
                                  FreshGhosts_.size() + HotGhosts_.size();

      if (DirectorySize >= Capacity_) {

        if (DirectorySize == 2 * Capacity_) {
          removeLRU(ListType::HotGhosts_);
        }

        VictimGoesToGhost_ = true;
      }
    }

    return true;
  }

  void onCacheInsert(const KeyType &Key) override {

    if (Entries_.find(Key) != Entries_.end()) {
      std::cerr << "[ARC] Error: try to insert existing key (corrupted)\n";
      return;
    }

    if (FreshCache_.size() + HotCache_.size() >= Capacity_) {
      std::cerr << "[ARC] Error: try to insert extra element (corrupted)\n";
      return;
    }

    addToFront(Key,
               InsertAsHotCache_ ? ListType::HotCache_ : ListType::FreshCache_);

    InsertAsHotCache_   = false;
    VictimGoesToGhost_  = false;
    DiscardFreshVictim_ = false;
  }

  void onCacheHit(const KeyType &Key) override {

    auto It = Entries_.find(Key);
    if (It == Entries_.end()) {
      std::cerr << "[ARC] Error: hit on unknown key (corrupted)\n";
      return;
    }

    if (It->second.List == ListType::FreshCache_) {
      moveToFront(Key, ListType::FreshCache_, ListType::HotCache_);
    } else if (It->second.List == ListType::HotCache_) {
      moveToFront(Key, ListType::HotCache_, ListType::HotCache_);
    }
  }

  void onCacheErase(const KeyType &Key) override {

    auto It = Entries_.find(Key);
    if (It == Entries_.end()) {
      std::cerr << "[ARC] Error: erase on unknown key (corrupted)\n";
      return;
    }

    ListType CurrentList = It->second.List;

    if (CurrentList == ListType::FreshCache_) {
      if (VictimGoesToGhost_) {
        moveToFront(Key, ListType::FreshCache_, ListType::FreshGhosts_);
      } else {
        eraseEntry(Key);
      }
    } else if (CurrentList == ListType::HotCache_) {
      if (VictimGoesToGhost_) {
        moveToFront(Key, ListType::HotCache_, ListType::HotGhosts_);
      } else {
        eraseEntry(Key);
      }
    } else {
      std::cerr
          << "[ARC] Error: try to erase non-resident element (corrupted)\n";
      return;
    }

    VictimGoesToGhost_  = false;
    DiscardFreshVictim_ = false;
  }

  std::optional<KeyType> selectVictim(void) override {

    if (FreshCache_.empty() && HotCache_.empty()) {
      std::cerr << "[ARC] Warning: try to search victim in empty cache\n";
      return std::nullopt;
    }

    if (DiscardFreshVictim_) {
      return FreshCache_.back();
    }

    // Victim is from FreshCache_, if FreshCache_ more than target size,
    // or if last miss was from HotGhosts_.
    if (!FreshCache_.empty() &&
        (FreshCache_.size() > TargetSize_ ||
         (LastMissWasFromHotGhosts_ && FreshCache_.size() == TargetSize_))) {
      return FreshCache_.back();
    }

    if (!HotCache_.empty()) {
      return HotCache_.back();
    }

    // If HotCache_ is empty, but FreshCache_ is not empty.
    return FreshCache_.back();
  }

private:
  void addToFront(const KeyType &Key, ListType List) {
    std::list<KeyType> &TargetList = getList(List);
    TargetList.push_front(Key);
    Entries_.emplace(Key, Entry{List, TargetList.begin()});
  }

  void moveToFront(const KeyType &Key, ListType From, ListType To) {

    auto It = Entries_.find(Key);
    if (It == Entries_.end()) {
      std::cerr << "[ARC] Error: move on unknown key (corrupted)\n";
      return;
    }

    std::list<KeyType> &FromList = getList(From);
    std::list<KeyType> &ToList   = getList(To);

    // If From == To - splice reaplce elemnt in the begining of the same list.
    // Update LRU-position.
    ToList.splice(ToList.begin(), FromList, It->second.Iterator);

    It->second.List     = To;
    It->second.Iterator = ToList.begin();
  }

  void eraseEntry(const KeyType &Key) {

    auto It = Entries_.find(Key);
    if (It == Entries_.end()) {
      std::cerr << "[ARC] Error: erase on unknown key (corrupted)\n";
      return;
    }

    std::list<KeyType> &List = getList(It->second.List);

    List.erase(It->second.Iterator);
    Entries_.erase(It);
  }

  void removeLRU(ListType List) {
    std::list<KeyType> &TargetList = getList(List);
    if (TargetList.empty()) {
      return;
    }

    KeyType Key = TargetList.back();
    eraseEntry(Key);
  }

  std::list<KeyType> &getList(ListType List) {

    switch (List) {
    case ListType::FreshCache_:
      return FreshCache_;
    case ListType::HotCache_:
      return HotCache_;
    case ListType::FreshGhosts_:
      return FreshGhosts_;
    case ListType::HotGhosts_:
      return HotGhosts_;
    }

    // Unreachable with correct usage, but we protect against it anyway.
    throw std::logic_error("ARCPolicy::getList: unknown ListType");
  }

private:
  std::list<KeyType> FreshCache_;
  std::list<KeyType> HotCache_;
  std::list<KeyType> FreshGhosts_;
  std::list<KeyType> HotGhosts_;

  std::unordered_map<KeyType, Entry> Entries_;

  std::size_t Capacity_;

  // Target size FreshCache_.
  // Increases on FreshGhosts_ miss, decreases on HotGhosts_ miss.
  std::size_t TargetSize_;

  bool InsertAsHotCache_{false};
  bool VictimGoesToGhost_{false};
  bool LastMissWasFromHotGhosts_{false};
  bool DiscardFreshVictim_{false};
};
