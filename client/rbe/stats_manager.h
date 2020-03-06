// Copyright 2020 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_RBE_STATS_MANAGER_H_
#define DEVTOOLS_GOMA_CLIENT_RBE_STATS_MANAGER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/time/time.h"
#include "base/compiler_specific.h"
#include "base/lockhelper.h"
#include "client/compile_stats.h"
#include "json/value.h"

MSVC_PUSH_DISABLE_WARNING_FOR_PROTO()
#include "prototmp/goma_data.pb.h"
MSVC_POP_WARNING()

namespace devtools_goma {
class CompileTask;

namespace rbe {

// Manages the accumulated RBE stats from finished CompileTask. This class is
// thread safe.
//
class StatsManager {
 public:
  // Accumulates the RBE stats of |task|. This means |task| has finished.
  void Accumulate(const CompileTask* task) LOCKS_EXCLUDED(mu_);
  void Accumulate(const CommandSpec& command_spec,
                  const CompileStats& task_stats) LOCKS_EXCLUDED(mu_);

  // Dumps the accumulated stats into |json|.
  // * Output format:
  // [
  //   {
  //     "compiler": {
  //       # Fields from CommandSpec to identify a compiler
  //       "name": <str>,
  //       "version": <str>,
  //       "target": <str>,
  //       "binary_hash": <str>,
  //     },
  //     "num_cached": <int>,
  //     "num_noncached": <int>
  //     # Converted from absl::Duration
  //     "total_cached_exec_duration": <str>,
  //     "total_noncached_exec_duration": <str>,
  //   },
  //   ...
  // ]
  Json::Value DumpStats() const LOCKS_EXCLUDED(mu_);

 private:
  // A subset of CommandSpec
  struct CompilerKey {
    std::string name;
    std::string version;
    std::string target;
    std::string binary_hash;

    friend bool operator==(const CompilerKey& lhs, const CompilerKey& rhs) {
      return lhs.name == rhs.name && lhs.version == rhs.version &&
             lhs.target == rhs.target && lhs.binary_hash == rhs.binary_hash;
    }

    template <typename H>
    friend H AbslHashValue(H h, const CompilerKey& c) {
      return H::combine(std::move(h), c.name, c.version, c.target,
                        c.binary_hash);
    }
  };

  struct PerCompilerStats {
    int num_cached = 0;
    int num_noncached = 0;
    absl::Duration total_cached_exec_duration;
    absl::Duration total_noncached_exec_duration;
  };

  mutable ReadWriteLock mu_;
  absl::flat_hash_map<CompilerKey, PerCompilerStats> per_compiler_stats_
      GUARDED_BY(mu_);
};

}  // namespace rbe
}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_RBE_STATS_MANAGER_H_
