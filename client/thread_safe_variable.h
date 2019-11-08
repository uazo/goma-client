// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_THREAD_SAFE_VARIABLE_H_
#define DEVTOOLS_GOMA_CLIENT_THREAD_SAFE_VARIABLE_H_

#include <functional>
#include <type_traits>

#include "autolock_timer.h"

namespace devtools_goma {

// A simple class that wrapps a copyable type to make the underlying variable
// thread safe. This is intended to only store POD-ish, cheap-to-copy variables.
// Think of it as an atomic implemented with mutex.
template <typename T,
          typename = std::enable_if_t<std::is_copy_constructible<T>::value &&
                                      std::is_copy_assignable<T>::value>>
class ThreadSafeVariable {
 public:
  ThreadSafeVariable() = default;

  template <typename U>
  explicit ThreadSafeVariable(U&& v) : storage_(std::forward<U>(v)) {}

  // Returns the underlying object by making a copy. The copy is intentional so
  // that the data will not be modified without the lock.
  T get() const LOCKS_EXCLUDED(mu_) {
    AUTO_SHARED_LOCK(lock, &mu_);
    return storage_;
  }

  // Sets the underlying object.
  template <typename U>
  void set(U&& v) LOCKS_EXCLUDED(mu_) {
    AUTO_EXCLUSIVE_LOCK(lock, &mu_);
    storage_ = std::forward<U>(v);
  }

  // Runs a function to read the object. |mu_| will be held during the execution
  // of |f|.
  void Run(const std::function<void(const T&)>& f) LOCKS_EXCLUDED(mu_) {
    AUTO_SHARED_LOCK(lock, &mu_);
    f(storage_);
  }

  // Runs a function that could potentially modify the object. |mu_| will be
  // held during the execution of |f|.
  void Run(const std::function<void(T*)>& f) LOCKS_EXCLUDED(mu_) {
    AUTO_EXCLUSIVE_LOCK(lock, &mu_);
    f(&storage_);
  }

 private:
  mutable ReadWriteLock mu_;
  T storage_ GUARDED_BY(mu_);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_THREAD_SAFE_VARIABLE_H_
