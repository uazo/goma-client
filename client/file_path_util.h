// Copyright 2010 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_FILE_PATH_UTIL_H_
#define DEVTOOLS_GOMA_CLIENT_FILE_PATH_UTIL_H_

#include <set>
#include <string>
#include <vector>

#include "file_stat.h"

namespace devtools_goma {

class ExecReq;

// Returns true if |candidate_path| (at |cwd| with PATH=|path|) is gomacc.
// Note: this is usually used to confirm the |candidate_path| is not gomacc.
// Note: You MUST call InstallReadCommandOuptutFunc beforehand.
bool IsGomacc(const std::string& candidate_path,
              const std::string& path,
              const std::string& pathext,
              const std::string& cwd);

// Find a real path name of |cmd| from |path_env|.
// It avoids to choose the file having same FileStat with |gomacc_filestat|.
// It returns true on success, and |local_compiler_path| (real compiler path)
// and |no_goma_path_env| (PATH env. without gomacc) are set.
// On Windows, |pathext_env| is used as PATHEXT parameter.
// Other platform should set empty |pathext_env| or fatal error.
// |cwd| represents current working directory.
// Note: you can use NULL to |no_goma_path_env| if you do not need them.
// Note: You MUST call InstallReadCommandOuptutFunc beforehand if you
//       use gomacc_filestat.
bool GetRealExecutablePath(const FileStat* gomacc_filestat,
                           const std::string& cmd,
                           const std::string& cwd,
                           const std::string& path_env,
                           const std::string& pathext_env,
                           std::string* local_compiler_path,
                           std::string* no_goma_path_env);

// Remove duplicate filepath from |filenames|
// for files normalized by JoinPathRepectAbsolute with |cwd|.
// Relative path is taken in high priority.
void RemoveDuplicateFiles(const std::string& cwd,
                          std::set<std::string>* filenames,
                          std::vector<std::string>* removed_files);

#ifdef _WIN32
// Resolves path extension of |cmd| using PATHEXT environment given with
// |pathext_env|.  If |cmd| is not an absolute path, it is automatically
// converted to an absolute path using |cwd|.
std::string ResolveExtension(const std::string& cmd,
                             const std::string& pathext_env,
                             const std::string& cwd);
#endif

// Validate local compiler path in |req| against |compiler_name|.
// Returns true if they match, or if no local compiler path was provided.
bool IsLocalCompilerPathValid(const std::string& trace_id,
                              const ExecReq& req,
                              const std::string& compiler_name);

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_FILE_PATH_UTIL_H_
