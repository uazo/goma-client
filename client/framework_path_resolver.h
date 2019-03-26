// Copyright 2013 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_FRAMEWORK_PATH_RESOLVER_H_
#define DEVTOOLS_GOMA_CLIENT_FRAMEWORK_PATH_RESOLVER_H_

#include <string>
#include <vector>

#include "basictypes.h"

namespace devtools_goma {

class FrameworkPathResolver {
 public:
  explicit FrameworkPathResolver(std::string cwd);
  ~FrameworkPathResolver() {}

  // Returns list of files in the framework.
  std::string ExpandFrameworkPath(const std::string& framework) const;
  void SetSyslibroot(const std::string& syslibroot) {
    syslibroot_ = syslibroot;
  }
  void AppendSearchpaths(const std::vector<std::string>& searchpaths);

 private:
  std::string FrameworkFile(const std::string& syslibroot,
                            const std::string& dirname,
                            const std::string& name,
                            const std::vector<std::string>& candidates) const;

  const std::string cwd_;
  std::string syslibroot_;
  std::vector<std::string> searchpaths_;
  std::vector<std::string> default_searchpaths_;

  DISALLOW_COPY_AND_ASSIGN(FrameworkPathResolver);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_FRAMEWORK_PATH_RESOLVER_H_
