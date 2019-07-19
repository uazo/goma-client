// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sha256_hash_cache.h"

#include "absl/base/call_once.h"
#include "client/autolock_timer.h"
#include "glog/logging.h"
#include "lib/goma_hash.h"

namespace devtools_goma {

namespace {

absl::once_flag g_init_once;

}  // namespace

bool SHA256HashCache::GetHashFromCacheOrFile(const std::string& path,
                                             std::string* hash) {
  total_.Add(1);

  FileStat filestat(path);
  if (!filestat.IsValid()) {
    return false;
  }

  {
    AUTO_SHARED_LOCK(lock, &mu_);
    const auto& it = cache_.find(path);
    if (it != cache_.end() && !filestat.CanBeNewerThan(it->second.first)) {
      *hash = it->second.second;
      hit_.Add(1);
      return true;
    }
  }

  if (!GomaSha256FromFile(path, hash)) {
    return false;
  }

  if (filestat.CanBeStale()) {
    return true;
  }

  AUTO_EXCLUSIVE_LOCK(lock, &mu_);
  cache_[path] = std::make_pair(filestat, *hash);
  return true;
}

// static
SHA256HashCache* SHA256HashCache::instance() {
  absl::call_once(g_init_once, SHA256HashCache::Init);
  return instance_;
}

// static
void SHA256HashCache::Init() {
  CHECK(!instance_);
  instance_ = new SHA256HashCache();
  atexit(SHA256HashCache::Quit);
}

// static
void SHA256HashCache::Quit() {
  delete instance_;
  instance_ = nullptr;
}

SHA256HashCache* SHA256HashCache::instance_ = nullptr;

}  // namespace devtools_goma
