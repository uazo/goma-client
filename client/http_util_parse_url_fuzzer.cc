// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "http_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  devtools_goma::URL url;
  devtools_goma::ParseURL(input, &url);

  return 0;
}
