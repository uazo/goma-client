// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "http_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  std::vector<absl::string_view> pieces;
  devtools_goma::HttpChunkParser().Parse(input, &pieces);

  return 0;
}
