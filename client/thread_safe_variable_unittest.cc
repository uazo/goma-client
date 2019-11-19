// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "thread_safe_variable.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/synchronization/notification.h"
#include "test_worker_thread.h"

namespace devtools_goma {
namespace {

using ThreadSafeInt = ThreadSafeVariable<int>;

class Worker : public TestWorkerThread {
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
  std::vector<std::unique_ptr<Worker>> workers;
  for (int i = 0; i < kNumWorkers; ++i) {
    auto worker = absl::make_unique<Worker>(kNumRuns, &n, &variable);
    worker->Start();
    workers.push_back(std::move(worker));
  }
  EXPECT_EQ(variable.get(), 0);
  n.Notify();
  for (auto& w : workers) {
    w->Join();
  }
  EXPECT_EQ(variable.get(), kNumRuns * kNumWorkers);
}

TEST(ThreadSafeVariableTest, ArgForwarding) {
  ThreadSafeVariable<absl::optional<int>> var;
  EXPECT_FALSE(var.get());
  var.set(42);
  EXPECT_EQ(var.get(), 42);
  var.set(absl::nullopt);
  EXPECT_FALSE(var.get());
}

}  // namespace devtools_goma
