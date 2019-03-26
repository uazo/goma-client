// Copyright 2018 Google Inc. All Rights Reserved.

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
