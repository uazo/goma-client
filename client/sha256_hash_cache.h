// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_SHA256_HASH_CACHE_H_
#define DEVTOOLS_GOMA_CLIENT_SHA256_HASH_CACHE_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "base/lockhelper.h"
#include "client/atomic_stats_counter.h"
#include "client/file_stat.h"

namespace devtools_goma {

class SHA256HashCache {
 public:
  SHA256HashCache(const SHA256HashCache&) = delete;
  SHA256HashCache& operator=(const SHA256HashCache&) = delete;

  static SHA256HashCache* instance();

  // If |path| exsts in |sha256_cache| and filestat is not updated,
  // the value is returned.
  // Otherwise, calculate sha256 hash from |path|, and put the result
  // to |sha256_cache| with filestat.
  // Returns false if calculating sha256 hash from |path| failed.
  bool GetHashFromCacheOrFile(const std::string& path, std::string* hash);

  int64_t total() const { return total_.value(); }
  int64_t hit() const { return hit_.value(); }

 private:
  SHA256HashCache() = default;
  ~SHA256HashCache() = default;

  friend class SHA256HashCacheTest;

  static void Init();
  static void Quit();

  static SHA256HashCache* instance_;

  using ValueT = std::pair<FileStat, std::string>;
  ReadWriteLock mu_;
  // |filepath| -> (filestat, hash of file)
  // We suppose the size of the hash map is quite small.
  // If it is not true, I suggest to use LinkedUnorderedMap instead.
  absl::flat_hash_map<std::string, ValueT> cache_ GUARDED_BY(mu_);

  // counter for test.
  StatsCounter total_;
  StatsCounter hit_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_SHA256_HASH_CACHE_H_
