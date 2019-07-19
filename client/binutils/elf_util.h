// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_UTIL_H_
#define DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_UTIL_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace devtools_goma {

// Parse contents in ld.so.conf, and returns library search path.
// The returned value would be used by ElfDepParser.
std::vector<std::string> ParseLdSoConf(absl::string_view content);

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_UTIL_H_
