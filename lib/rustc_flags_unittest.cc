// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/rustc_flags.h"

#include "glog/logging.h"
#include "gtest/gtest.h"

namespace devtools_goma {

TEST(RustcFlagsTest, Basic) {
  const std::vector<std::string> args{
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

  const std::string cwd = ".";

  RustcFlags flags(args, cwd);
  EXPECT_TRUE(flags.is_successful());
  EXPECT_EQ(
      std::vector<std::string>{"/home/goma/.cargo/registry/src/"
                               "github.com-1ecc6299db9ec823/rand-0.5.3/src/"
                               "lib.rs"},
      flags.input_filenames());
  EXPECT_EQ(
      std::vector<std::string>{"/home/goma/tmp/cargo-test/target/debug/deps"},
      flags.output_dirs());
}

TEST(RustcFlagsTest, IsRustcCommand) {
  EXPECT_TRUE(RustcFlags::IsRustcCommand("rustc"));
  EXPECT_TRUE(RustcFlags::IsRustcCommand("/usr/bin/rustc"));
  EXPECT_TRUE(RustcFlags::IsRustcCommand(
      "/usr/local/google/home/goma/.cargo/bin/rustc"));

  EXPECT_TRUE(RustcFlags::IsRustcCommand("rustc.exe"));

  EXPECT_FALSE(RustcFlags::IsRustcCommand("foo"));
  EXPECT_FALSE(RustcFlags::IsRustcCommand("bar"));
}

TEST(RustcFlagsTest, GetCompilerName) {
  EXPECT_EQ("rustc", RustcFlags::GetCompilerName("rustc"));
  EXPECT_EQ("rustc", RustcFlags::GetCompilerName("rustc.exe"));
}

}  // namespace devtools_goma
