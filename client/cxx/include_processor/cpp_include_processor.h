// Copyright 2010 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_H_

#include <map>
#include <set>
#include <string>

#include "basictypes.h"
#include "compiler_flags.h"
#include "cpp_parser.h"
#include "cxx/cxx_compiler_info.h"
#include "file_stat_cache.h"
#include "include_file_finder.h"

namespace devtools_goma {

class Content;
class GCCFlags;

class CppIncludeProcessor {
 public:
  CppIncludeProcessor() = default;

  // Enumerates all include files. When FileStats are created for them,
  // we cache them in |file_stat_cache| so that we can reuse them later,
  // because creating FileStat is so slow especially on Windows.
  bool GetIncludeFiles(const std::string& filename,
                       const std::string& current_directory,
                       const CompilerFlags& compiler_flags,
                       const CxxCompilerInfo& compiler_info,
                       std::set<std::string>* include_files,
                       FileStatCache* file_stat_cache);

  const CppParser* cpp_parser() const { return &cpp_parser_; }

  int total_files() const;
  int skipped_files() const;

 private:
  // Returns a vector of tuple<filepath, dir_index>.
  std::vector<std::pair<std::string, int>>
  CalculateRootIncludesWithIncludeDirIndex(
      const std::vector<std::string>& root_includes,
      const std::string& current_directory,
      const CompilerFlags& compiler_flags,
      IncludeFileFinder* include_file_finder,
      std::set<std::string>* include_files);

  bool AddClangModulesFiles(const GCCFlags& flags,
                            const std::string& current_directory,
                            std::set<std::string>* include_files,
                            FileStatCache* file_stat_cache) const;

  CppParser cpp_parser_;

  friend class CppIncludeProcessorTest;

  DISALLOW_COPY_AND_ASSIGN(CppIncludeProcessor);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_INCLUDE_PROCESSOR_H_
