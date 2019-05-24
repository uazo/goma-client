// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rustc_include_processor.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace devtools_goma {

TEST(RustcIncludeProcessorTest, ParseDepsInfo) {
  constexpr absl::string_view kDepsInfo =
      R"(main: ./main.rs
main.d: ./main.rs
./main.rs
)";
  std::set<std::string> required_files;
  std::string error_reason;
  EXPECT_TRUE(RustcIncludeProcessor::ParseRustcDeps(kDepsInfo, &required_files,
                                                    &error_reason));
  EXPECT_EQ(required_files, std::set<std::string>{"./main.rs"});
}

TEST(RustcIncludeProcessorTest, RewriteArgsTest) {
  const std::vector<std::string> test_args{
      "rustc",
      "--crate-name",
      "rand",
      "/home/goma/.cargo/registry/src/github.com-1ecc6299db9ec823/rand-0.5.3/"
      "src/lib.rs",
      "--crate-type",
      "lib",
      "--emit=dep-info,link",
      "-C",
      "debuginfo=2",
      "--cfg",
      "feature=\"alloc\"",
      "--cfg",
      "feature=\"cloudabi\"",
      "--cfg",
      "feature=\"default\"",
      "--cfg",
      "feature=\"fuchsia-zircon\"",
      "--cfg",
      "feature=\"libc\"",
      "--cfg",
      "feature=\"rand_core\"",
      "--cfg",
      "feature=\"std\"",
      "--cfg",
      "feature=\"winapi\"",
      "-C",
      "metadata=732894137054066a",
      "-C",
      "extra-filename=-732894137054066a",
      "--out-dir",
      "/home/goma/tmp/cargo-test/target/debug/deps",
      "-L",
      "dependency=/home/goma/tmp/cargo-test/target/debug/deps",
      "--extern",
      "libc=/home/goma/tmp/cargo-test/target/debug/deps/"
      "liblibc-463874d8fa76eafc.rlib",
      "--extern",
      "rand_core=/home/goma/tmp/cargo-test/target/debug/deps/"
      "librand_core-77ec6d8abf82a269.rlib",
      "--cap-lints",
      "allow",
  };
  const std::vector<std::string> expected_args{
      "rustc",
      "--crate-name",
      "rand",
      "/home/goma/.cargo/registry/src/github.com-1ecc6299db9ec823/rand-0.5.3/"
      "src/lib.rs",
      "--crate-type",
      "lib",
      "-C",
      "debuginfo=2",
      "--cfg",
      "feature=\"alloc\"",
      "--cfg",
      "feature=\"cloudabi\"",
      "--cfg",
      "feature=\"default\"",
      "--cfg",
      "feature=\"fuchsia-zircon\"",
      "--cfg",
      "feature=\"libc\"",
      "--cfg",
      "feature=\"rand_core\"",
      "--cfg",
      "feature=\"std\"",
      "--cfg",
      "feature=\"winapi\"",
      "-C",
      "metadata=732894137054066a",
      "-C",
      "extra-filename=-732894137054066a",
      "-L",
      "dependency=/home/goma/tmp/cargo-test/target/debug/deps",
      "--extern",
      "libc=/home/goma/tmp/cargo-test/target/debug/deps/"
      "liblibc-463874d8fa76eafc.rlib",
      "--extern",
      "rand_core=/home/goma/tmp/cargo-test/target/debug/deps/"
      "librand_core-77ec6d8abf82a269.rlib",
      "--cap-lints",
      "allow",
      "--emit=dep-info",
      "-o",
      "lib.d",
  };

  std::vector<std::string> returned_args;
  std::string error_reason;
  EXPECT_TRUE(RustcIncludeProcessor::RewriteArgs(
      test_args, "lib.d", &returned_args, &error_reason));
  EXPECT_EQ(returned_args, expected_args);
}

}  // namespace devtools_goma