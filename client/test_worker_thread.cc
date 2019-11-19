// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_worker_thread.h"

namespace devtools_goma {

void TestWorkerThread::Start() {
  PlatformThread::Create(this, &handle_);
}

void TestWorkerThread::Join() {
  PlatformThread::Join(handle_);
  handle_ = kNullThreadHandle;
}

PlatformThreadHandle TestWorkerThread::handle() const {
  return handle_;
}

}  // namespace devtools_goma
