// Copyright 2020 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVTOOLS_GOMA_CLIENT_GET_COMPILER_INFO_PARAM_H_
#define DEVTOOLS_GOMA_CLIENT_GET_COMPILER_INFO_PARAM_H_

#include <string>
#include <vector>

#include "compiler_info_cache.h"
#include "compiler_info_state.h"
#include "worker_thread.h"

namespace devtools_goma {

struct GetCompilerInfoParam {
  // request
  WorkerThread::ThreadId thread_id = kInvalidThreadId;
  std::string trace_id;
  CompilerInfoCache::Key key;
  const CompilerFlags* flags = nullptr;
  std::vector<std::string> run_envs;

  // response
  ScopedCompilerInfoState state;
  // cache_hit=true > fast cache hit, didn't run in worker thread
  // cache_hit=false,updated=true > cache miss, updated with compiler output
  // cache_hit=false,update=false > cache miss->cache hit in worker thread
  bool cache_hit = false;
  bool updated = false;

  GetCompilerInfoParam() = default;
  // Move only
  GetCompilerInfoParam(GetCompilerInfoParam&&) = default;
  GetCompilerInfoParam& operator=(GetCompilerInfoParam&&) = default;

  GetCompilerInfoParam(const GetCompilerInfoParam&) = delete;
  GetCompilerInfoParam& operator=(const GetCompilerInfoParam&) = delete;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_GET_COMPILER_INFO_PARAM_H_
