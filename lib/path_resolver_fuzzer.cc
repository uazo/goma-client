// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "path_resolver.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  using devtools_goma::PathResolver;
  PathResolver::ResolvePath(input, PathResolver::kPosixPathSep);
  PathResolver::ResolvePath(input, PathResolver::kWin32PathSep);

  return 0;
}
