// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef DEVTOOLS_GOMA_BASE_OPTIONS_H_
#define DEVTOOLS_GOMA_BASE_OPTIONS_H_

#include "absl/strings/string_view.h"
#include "status.h"

namespace file {

class Options {
 private:
  // Don't construct Options directly. Use friend functions.
  Options() = default;
  Options(const Options&) = default;

  // can be used from CreateDir only
  int creation_mode() const { return creation_mode_; }

  int overwrite() const { return overwrite_; }

  int creation_mode_ = 0;
  bool overwrite_ = false;

  friend Options Defaults();
  friend Options CreationMode(int mode);
  friend Options Overwrite();

  friend util::Status CreateDir(absl::string_view path, const Options& options);
  friend util::Status Copy(absl::string_view from,
                           absl::string_view to,
                           const Options& options);
};

Options Defaults();
// TODO: Use mode_t? It does not exist on Win, though...
Options CreationMode(int mode);
Options Overwrite();

}  // namespace file

#endif  // DEVTOOLS_GOMA_BASE_OPTIONS_H_
