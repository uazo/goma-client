// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rustc_compiler_info_builder.h"

#include "gtest/gtest.h"
#include "rustc_compiler_info.h"
#include "rustc_flags.h"

namespace devtools_goma {

TEST(RustCompilerInfoBuilderTest, ParseRustcVersionTarget) {
  constexpr absl::string_view kCompilerOutput =
      R"(rustc 1.29.0-nightly (9bd8458c9 2018-07-09)
binary: rustc
commit-hash: 9bd8458c92f7166b827e4eb5cf5effba8c0e615d
commit-date: 2018-07-09
host: x86_64-unknown-linux-gnu
release: 1.29.0-nightly
LLVM version: 6.0
)";

  RustcCompilerInfoBuilder builder;

  std::string version;
  std::string host;

  EXPECT_TRUE(builder.ParseRustcVersionHost(kCompilerOutput, &version, &host));
  EXPECT_EQ("1.29.0-nightly (9bd8458c9 2018-07-09)", version);
  EXPECT_EQ("x86_64-unknown-linux-gnu", host);
}

}  // namespace devtools_goma
