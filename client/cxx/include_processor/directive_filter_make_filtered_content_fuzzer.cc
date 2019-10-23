// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "client/content.h"
#include "client/cxx/include_processor/directive_filter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::unique_ptr<devtools_goma::Content> content =
      devtools_goma::Content::CreateFromBuffer(
          reinterpret_cast<const char*>(data), size);
  devtools_goma::DirectiveFilter::MakeFilteredContent(*content);

  return 0;
}
