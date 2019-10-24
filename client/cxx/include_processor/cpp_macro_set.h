// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_MACRO_SET_H_
#define DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_MACRO_SET_H_

#include "absl/container/flat_hash_set.h"

namespace devtools_goma {

struct Macro;

class MacroSet {
 public:
  MacroSet() {}
  void Set(const Macro* m) { macros_.insert(m); }
  void Remove(const Macro* m) { macros_.erase(m); }
  bool Has(const Macro* m) const { return macros_.contains(m); }

  void Union(const MacroSet& other) {
    macros_.insert(other.macros_.begin(), other.macros_.end());
  }

  void Intersection(const MacroSet& other) {
    absl::flat_hash_set<const Macro*> intersection;

    for (const Macro* x : macros_) {
      if (other.Has(x)) {
        intersection.insert(x);
      }
    }

    macros_ = std::move(intersection);
  }

  bool empty() const { return macros_.empty(); }

 private:
  absl::flat_hash_set<const Macro*> macros_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_CXX_INCLUDE_PROCESSOR_CPP_MACRO_SET_H_
