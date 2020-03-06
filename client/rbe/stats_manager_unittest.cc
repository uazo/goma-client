// Copyright 2020 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stats_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace devtools_goma {
namespace rbe {

CommandSpec MakeCommandSpec(std::string version) {
  CommandSpec spec;
  spec.set_name("gcc");
  spec.set_version(std::move(version));
  spec.set_target("linux");
  spec.set_binary_hash("123abc");
  return spec;
}

TEST(StatsManager, Basic) {
  const auto spec1 = MakeCommandSpec("10.0");
  const auto spec2 = MakeCommandSpec("10.1");
  CompileStats stats1;
  stats1.set_cache_hit(true);
  stats1.total_rbe_execution_time = absl::Seconds(1);
  CompileStats stats2;
  stats2.set_cache_hit(false);
  stats2.total_rbe_execution_time = absl::Seconds(2);
  CompileStats stats3;
  stats3.set_cache_hit(false);
  stats3.total_rbe_execution_time = absl::Seconds(3);
  CompileStats stats4;
  stats4.set_cache_hit(false);
  stats4.total_rbe_execution_time = absl::Seconds(4);

  StatsManager stats_mgr;
  stats_mgr.Accumulate(spec1, stats1);
  stats_mgr.Accumulate(spec1, stats2);
  stats_mgr.Accumulate(spec2, stats3);
  stats_mgr.Accumulate(spec2, stats4);

  auto json = stats_mgr.DumpStats();
  ASSERT_EQ(json.size(), 2);

  constexpr char kCompiler[] = "compiler";
  constexpr char kVersion[] = "version";
  constexpr char kNumCached[] = "num_cached";
  constexpr char kNumNoncached[] = "num_noncached";
  constexpr char kTotalCachedExecDuration[] = "total_cached_exec_duration";
  constexpr char kTotalNoncachedExecDuration[] =
      "total_noncached_exec_duration";
  // Json::Value::iterator doesn't comply to C++'s type trait.
  std::vector<Json::Value> json_stats = {json[0], json[1]};
  std::sort(json_stats.begin(), json_stats.end(),
            [=](const Json::Value& lhs, const Json::Value& rhs) -> bool {
              return lhs[kCompiler][kVersion] < rhs[kCompiler][kVersion];
            });
  const auto& js0 = json_stats[0];
  // spec1: stats1, stats2
  EXPECT_EQ(js0[kCompiler][kVersion], spec1.version());
  EXPECT_EQ(js0[kNumCached], 1);
  EXPECT_EQ(js0[kNumNoncached], 1);
  EXPECT_EQ(js0[kTotalCachedExecDuration], "1 s");
  EXPECT_EQ(js0[kTotalNoncachedExecDuration], "2 s");
  const auto& js1 = json_stats[1];
  // spec2: stats3, stats4
  EXPECT_EQ(js1[kCompiler][kVersion], spec2.version());
  EXPECT_EQ(js1[kNumCached], 0);
  EXPECT_EQ(js1[kNumNoncached], 2);
  EXPECT_EQ(js1[kTotalCachedExecDuration], "0");
  EXPECT_EQ(js1[kTotalNoncachedExecDuration], "7 s");
}

}  // namespace rbe
}  // namespace devtools_goma
