#pragma once

#include <optional>
#include <string>

template <typename KeyType> class CachePolicy {

  /// Template class for policies, all methodes is virtual
  /// if you need to add new cache policy you need to override all of
  /// CachePolicy API

public:
  /// Return policy name for debug or beauty output
  virtual std::string getPolicyName() const = 0;

  /// Return true if pollicy need insert \p Key into Cache after miss. Most
  /// likely return true in majority of policies
  virtual bool needInsertInCache(const KeyType &Key) = 0;

  /// Make policy react for insertion of element into cache. Use it if you
  /// insert \p Key in cache manually
  virtual void onCacheInsert(const KeyType &Key) = 0;

  /// Make policy react for hit \p Key element in cache. Implementation of
  /// hidden policy logic
  virtual void onCacheHit(const KeyType &Key) = 0;

  /// Make policy react for erase \p Key element from cache. Implementation of
  /// hidden policy logic. Use it if you erase element manually
  virtual void onCacheErase(const KeyType &Key) = 0;

  /// Return Key of element that need to erase or std::nullopt if cache is
  /// empty. Use it if haven't enough space in cache.
  virtual std::optional<KeyType> selectVictim(void) = 0;

  /// Notify the policy about a new request (required only for OPT).
  virtual void onRequest(const KeyType&) {}

  virtual ~CachePolicy() = default;
};
