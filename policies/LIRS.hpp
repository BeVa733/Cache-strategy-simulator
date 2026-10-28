#pragma once

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>

#include "CachePolicy.hpp"

template <typename KeyType> class LIRSPolicy : public CachePolicy<KeyType> {

private:
  enum class EntryType {
    HIR,
    LIR
  };
  using ListIt = typename std::list<KeyType>::iterator;

  struct Entry final {
    EntryType Type;
    bool Resident;
    bool InStack;
    bool InQueue;
    ListIt StackIt;
    ListIt QueueIt;
  };

public:
  explicit LIRSPolicy(std::size_t Size)
      : Capacity_(Size), HIRCapacity_(calculateHIRCapacity(Size)),
        LIRCapacity_(Size - HIRCapacity_) {}

  std::string getPolicyName() const override { return "LIRS"; }

  bool needInsertInCache(const KeyType &) override { return Capacity_ != 0; }

  void onCacheInsert(const KeyType &Key) override {
    if (Capacity_ == 0) {
      return;
    }

    auto It = Entries_.find(Key);
    if (It != Entries_.end() && It->second.Resident) {
      std::cerr << "[LIRS] Error: try to insert resident element\n";
      return;
    }

    if (It == Entries_.end()) {
      Entry NewEntry{EntryType::HIR, false, false, false, ListIt{}, ListIt{}};
      It = Entries_.emplace(Key, NewEntry).first;
    }

    Entry &Entry   = It->second;
    Entry.Resident = true;

    // A ghost HIR has a short inter-reference distance, so its new copy is
    // promoted to LIR.
    if (Entry.InStack) {
      if (LIRCapacity_ != 0) {
        Entry.Type = EntryType::LIR;
        ++LIRSize_;
      } else {
        Entry.Type = EntryType::HIR;
      }
      moveToStackFront(Key);
    } else if (LIRSize_ < LIRCapacity_) {
      Entry.Type = EntryType::LIR;
      ++LIRSize_;
      addToStackFront(Key);
    } else {
      Entry.Type = EntryType::HIR;
      addToStackFront(Key);
      addToQueueFront(Key);
    }

    if (Entry.Type == EntryType::LIR && LIRSize_ > LIRCapacity_) {
      demoteOldestLIR(Key);
    }

    pruneStack();
  }

  void onCacheHit(const KeyType &Key) override {
    auto It = Entries_.find(Key);
    if (It == Entries_.end() || !It->second.Resident) {
      std::cerr << "[LIRS] Error: hit on unknown or non-resident element\n";
      return;
    }

    Entry &Entry = It->second;
    if (Entry.Type == EntryType::LIR) {
      if (Entry.InStack) {
        moveToStackFront(Key);
      } else {
        addToStackFront(Key);
      }
      pruneStack();
      return;
    }

    // A resident HIR can be promoted only when it is already in S_. If it is
    // absent from S_, its reuse distance is larger than every LIR's, so keep
    // it HIR and only refresh its position in S_ and Queue_.
    if (!Entry.InStack) {
      addToStackFront(Key);
      removeFromQueue(Entry);
      addToQueueFront(Key);
      return;
    }

    removeFromQueue(Entry);
    moveToStackFront(Key);

    if (LIRCapacity_ == 0) {
      addToQueueFront(Key);
      return;
    }

    Entry.Type = EntryType::LIR;
    ++LIRSize_;
    demoteOldestLIR(Key);
    pruneStack();
  }

  void onCacheErase(const KeyType &Key) override {
    auto It = Entries_.find(Key);
    if (It == Entries_.end() || !It->second.Resident) {
      std::cerr << "[LIRS] Error: erase on unknown or non-resident element\n";
      return;
    }

    Entry &Entry   = It->second;
    Entry.Resident = false;

    if (Entry.Type == EntryType::HIR) {
      // Keep an evicted HIR in S_ as a ghost.
      removeFromQueue(Entry);
      if (!Entry.InStack) {
        Entries_.erase(It);
      }
      return;
    }

    // LIRs are not normal victims. This path is needed when a page is moved
    // to another cache level: the common API uses onCacheErase for both.
    if (Entry.InStack) {
      Stack_.erase(Entry.StackIt);
    }
    Entries_.erase(It);
    --LIRSize_;
    pruneStack();
  }

  std::optional<KeyType> selectVictim() override {
    if (Queue_.empty()) {
      std::cerr << "[LIRS] Warning: try to search victim in empty HIR queue\n";
      return std::nullopt;
    }
    return Queue_.back();
  }

private:
  static std::size_t calculateHIRCapacity(std::size_t Capacity) {
    if (Capacity == 0) {
      return 0;
    }
    // Keep at least one HIR slot; use roughly 10% HIR for larger caches.
    return std::max<std::size_t>(1, Capacity / 10);
  }

  void addToStackFront(const KeyType &Key) {
    Stack_.push_front(Key);
    Entry &Entry  = Entries_.at(Key);
    Entry.StackIt = Stack_.begin();
    Entry.InStack = true;
  }

  void moveToStackFront(const KeyType &Key) {
    Entry &Entry = Entries_.at(Key);
    if (!Entry.InStack) {
      addToStackFront(Key);
      return;
    }
    Stack_.splice(Stack_.begin(), Stack_, Entry.StackIt);
    Entry.StackIt = Stack_.begin();
  }

  void addToQueueFront(const KeyType &Key) {
    Queue_.push_front(Key);
    Entry &Entry  = Entries_.at(Key);
    Entry.QueueIt = Queue_.begin();
    Entry.InQueue = true;
  }

  void removeFromQueue(Entry &Entry) {
    if (!Entry.InQueue) {
      return;
    }
    Queue_.erase(Entry.QueueIt);
    Entry.InQueue = false;
  }

  void demoteOldestLIR(const KeyType &PromotedKey) {
    if (LIRSize_ <= LIRCapacity_) {
      return;
    }

    for (auto It = Stack_.end(); It != Stack_.begin();) {
      --It;
      Entry &Candidate = Entries_.at(*It);
      if (Candidate.Type == EntryType::LIR && *It != PromotedKey) {
        Candidate.Type = EntryType::HIR;
        --LIRSize_;
        addToQueueFront(*It);
        return;
      }
    }

    // Defensive fallback for an inconsistent state.
    Entry &Promoted = Entries_.at(PromotedKey);
    Promoted.Type   = EntryType::HIR;
    --LIRSize_;
    addToQueueFront(PromotedKey);
  }

  void pruneStack() {
    while (!Stack_.empty()) {
      auto It      = std::prev(Stack_.end());
      auto EntryIt = Entries_.find(*It);
      if (EntryIt == Entries_.end() || EntryIt->second.Type == EntryType::LIR) {
        return;
      }

      Entry &Entry = EntryIt->second;
      KeyType Key  = *It;
      Stack_.erase(It);
      Entry.InStack = false;
      if (!Entry.Resident) {
        Entries_.erase(Key);
      }
    }
  }

private:
  std::list<KeyType> Stack_;
  std::list<KeyType> Queue_;
  std::unordered_map<KeyType, Entry> Entries_;

  std::size_t LIRSize_{0};
  std::size_t Capacity_{0};
  std::size_t HIRCapacity_{0};
  std::size_t LIRCapacity_{0};
};
