// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mypath_helper.h"

#import <Foundation/Foundation.h>

namespace devtools_goma {

std::string GetPlatformSpecificTempDirectory() {
  NSString* dir = NSTemporaryDirectory();
  if (dir == nil) {
    return std::string();
  }
  return std::string([dir UTF8String]);
}

}  // namespace devtools_goma
