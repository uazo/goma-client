// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "thread_safe_variable.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/notification.h"
#include "platform_thread.h"

namespace devtools_goma {
namespace {

using ThreadSafeInt = ThreadSafeVariable<int>;

class Worker : public PlatformThread::Delegate {
 public:
  Worker(int num_runs, absl::Notification* n, ThreadSafeInt* variable)
      : num_runs_(num_runs), n_(n), variable_(variable) {}

  void ThreadMain() override {
    n_->WaitForNotification();
    auto inc = [](int* val) { *val += 1; };
    for (int i = 0; i < num_runs_; ++i) {
      variable_->Run(inc);
    }
  }

 private:
  const int num_runs_;
  absl::Notification* const n_;
  ThreadSafeInt* const variable_;
};

}  // namespace

TEST(ThreadSafeVariableTest, Basic) {
  constexpr int kNumRuns = 100;
  constexpr int kNumWorkers = 10;

  absl::Notification n;
  ThreadSafeInt variable(0);
  std::vector<std::pair<std::unique_ptr<Worker>, PlatformThreadHandle>> workers;
  for (int i = 0; i < kNumWorkers; ++i) {
    auto worker = absl::make_unique<Worker>(kNumRuns, &n, &variable);
    PlatformThreadHandle h;
    PlatformThread::Create(worker.get(), &h);
    workers.push_back(std::make_pair(std::move(worker), h));
  }
  EXPECT_EQ(variable.get(), 0);
  n.Notify();
  for (const auto& wh : workers) {
    PlatformThread::Join(wh.second);
  }
  EXPECT_EQ(variable.get(), kNumRuns * kNumWorkers);
}

}  // namespace devtools_goma
