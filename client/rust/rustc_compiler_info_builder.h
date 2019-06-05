// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_INFO_BUILDER_H_
#define DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_INFO_BUILDER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "compiler_info_builder.h"
#include "gtest/gtest_prod.h"

namespace devtools_goma {

class RustcCompilerInfoBuilder : public CompilerInfoBuilder {
 public:
  ~RustcCompilerInfoBuilder() override = default;

  void SetTypeSpecificCompilerInfo(
      const CompilerFlags& flags,
      const std::string& local_compiler_path,
      const std::string& abs_local_compiler_path,
      const std::vector<std::string>& compiler_info_envs,
      CompilerInfoData* data) const override;

  void SetLanguageExtension(CompilerInfoData* data) const override;

  void SetCompilerPath(const CompilerFlags& flags,
                       const std::string& local_compiler_path,
                       const std::vector<std::string>& compiler_info_envs,
                       CompilerInfoData* data) const override;

 private:
  // Gets rustc's version and host.
  bool GetRustcVersionHost(const std::string& rustc_path,
                           const std::vector<std::string>& compiler_info_envs,
                           const std::string& cwd,
                           std::string* version,
                           std::string* host) const;
  // Parses rustc's version and host from compiler_output.
  bool ParseRustcVersionHost(absl::string_view compiler_output,
                             std::string* version,
                             std::string* host) const;

  // Collect required library and resource files which are required to
  // run rustc.
  static bool CollectRustcResources(absl::string_view real_compiler_path,
                                    std::vector<std::string>* resource_paths);

  // Add resource as EXECUTABLE_BINARY. If the resource is a symlink,
  // both the symlink and actual files are added. This is copied from
  // gcc_compiler_info_builder.
  static bool AddResourceAsExecutableBinary(
      const std::string& resource_path,
      const std::string& cwd,
      absl::flat_hash_set<std::string>* visited_paths,
      CompilerInfoData* data);

  FRIEND_TEST(RustCompilerInfoBuilderTest, ParseRustcVersionTarget);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_RUST_RUSTC_COMPILER_INFO_BUILDER_H_
