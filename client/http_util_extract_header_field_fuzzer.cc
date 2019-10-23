// Copyright 2019 Google LLC. The Goma Authors. All Rights Reserved.

#include <string>

#include "http_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);

  constexpr absl::string_view kTestFieldName = "Content-Type";
  devtools_goma::ExtractHeaderField(input, kTestFieldName);
  // Note: second arg (field_name) of ExtractHeaderField should not be fuzzed,
  // because it must not have initial or trailing whitespace to pass a DCHECK.

  return 0;
}
