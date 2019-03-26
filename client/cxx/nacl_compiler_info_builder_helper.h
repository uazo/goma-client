// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_NACL_COMPILER_INFO_BUILDER_HELPER_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_NACL_COMPILER_INFO_BUILDER_HELPER_H_

#include <string>
#include <vector>

namespace devtools_goma {

class NaClCompilerInfoBuilderHelper {
 public:
#ifdef _WIN32
  // GetNaClToolchainRoot is a part of hack needed for
  // the (build: Windows, target: NaCl) compile.
  static std::string GetNaClToolchainRoot(
      const std::string& normal_nacl_gcc_path);
#endif

  static void CollectPNaClClangResources(
      const std::string& locla_compiler_path,
      const std::string& cwd,
      std::vector<std::string>* resource_paths);
  static void CollectNaClGccResources(const std::string& local_compiler_path,
                                      const std::string& cwd,
                                      std::vector<std::string>* resource_paths);
  static void CollectNaClClangResources(
      const std::string& local_compiler_path,
      const std::string& cwd,
      std::vector<std::string>* resource_paths);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_NACL_COMPILER_INFO_BUILDER_HELPER_H_
