// Copyright 2020 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stats_manager.h"

#include "client/compile_task.h"
#include "client/time_util.h"

namespace devtools_goma {
namespace rbe {

void StatsManager::Accumulate(const CompileTask* task) {
  Accumulate(task->DumpCommandSpec(), task->stats());
}

void StatsManager::Accumulate(const CommandSpec& command_spec,
                              const CompileStats& task_stats) {
  AUTO_EXCLUSIVE_LOCK(lock, &mu_);
  const CompilerKey key{command_spec.name(), command_spec.version(),
                        command_spec.target(), command_spec.binary_hash()};
  auto& stats = per_compiler_stats_[key];
  const auto& task_duration = task_stats.total_rbe_execution_time;
  if (task_stats.cache_hit()) {
    ++stats.num_cached;
    stats.total_cached_exec_duration += task_duration;
  } else {
    ++stats.num_noncached;
    stats.total_noncached_exec_duration += task_duration;
  }
}

Json::Value StatsManager::DumpStats() const {
  Json::Value result(Json::arrayValue);
  AUTO_SHARED_LOCK(lock, &mu_);
  for (const auto& kv : per_compiler_stats_) {
    Json::Value json_kv;
    Json::Value json_key;
    const auto& key = kv.first;
    json_key["name"] = key.name;
    json_key["version"] = key.version;
    json_key["target"] = key.target;
    json_key["binary_hash"] = key.binary_hash;
    const auto& stats = kv.second;
    json_kv["compiler"] = std::move(json_key);
    json_kv["total_cached_exec_duration"] =
        FormatDurationToThreeDigits(stats.total_cached_exec_duration);
    json_kv["total_noncached_exec_duration"] =
        FormatDurationToThreeDigits(stats.total_noncached_exec_duration);
    json_kv["num_cached"] = stats.num_cached;
    json_kv["num_noncached"] = stats.num_noncached;
    result.append(std::move(json_kv));
  }
  return result;
}

}  // namespace rbe
}  // namespace devtools_goma
