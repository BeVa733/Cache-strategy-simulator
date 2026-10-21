#pragma once

#include <iostream>
#include <list>
#include <unordered_map>

#include "../include/CachePolicy.hpp"

template <typename KeyType> class LIRSPolicy : public CachePolicy<KeyType> {

public:
  explicit LIRSPolicy(std::size_t Size) : Capacity_(Size) {}

  std::string getPolicyName() override { return "LIRS"; }

  bool needInsertInCache(const KeyType &) override { return true; }

  void onCacheInsert(const KeyType &Key) override {

    auto EntryIt = Entries_.find(Key);

		// If Key in Recents - edit it
    if (EntryIt != Entries_.end()) {
      if (EntryIt->second.resident) {
        std::cerr << "[LIRS] Error: try to insert extra element (corrupted)\n";
        return;
      }

      // if Entry was in Recents it will be a LIR
			if (LIRCapacity_ < )
      EntryIt->seond.resident = true;
      EntryIt->second.Type     = LIR;
      Recents_.splice(Recents_.begin(), Recents_, EntryIt->second.RecentIt);
    } else 

    if (List_.size() >= Capacity_) {
      std::cerr << "[LRU] Error: try to insert extra element (corrupted)\n";
      return;
    }

    List_.push_front(Key);
    Recents_.emplace(Key, Recents_.begin());
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
  enum EntryType {
    HIR = 0,
    LIR = 1
  };

  struct Entry final {
    enum EntryType Type;
    bool resident;

    typename std::list<KeyType>::iterator HIRIt;
    typename std::list<KeyType>::iterator RecentIt;
  };

  std::list<KeyType> HIRs_;
  std::list<KeyType> Recents_;

  std::unordered_map<KeyType, Entry> Entries_;

	std::size_t LIRSize_;
  std::size_t Capacity_;
  std::size_t HIRCapacity_;
  std::size_t LIRCapacity_;

  // Last element in Recents must be a LIR
  void pruning() {
    for (auto It = Recents_.back(); Entries_[*It].Type != LIR; --It) {
      Recents_.erase(It);
    }
  }
};
