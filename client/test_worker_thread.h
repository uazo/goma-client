// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_TEST_WORKER_THREAD_H_
#define DEVTOOLS_GOMA_CLIENT_TEST_WORKER_THREAD_H_

#include "platform_thread.h"

namespace devtools_goma {

class TestWorkerThread : public PlatformThread::Delegate {
 public:
  void Start();
  void Join();

  PlatformThreadHandle handle() const;

 private:
  PlatformThreadHandle handle_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_TEST_WORKER_THREAD_H_
