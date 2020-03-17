// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "java/java_compiler_info_builder.h"

#include <memory>

#include "gtest/gtest.h"
#include "path.h"
#include "unittest_util.h"

namespace devtools_goma {

class JavacCompilerInfoBuilderTest : public ::testing::Test {
 public:
  void SetUp() override {
    tmpdir_util_ =
        std::make_unique<TmpdirUtil>("javac_compiler_info_builder_unittest");
    tmpdir_util_->SetCwd("");
  }

 protected:
  std::unique_ptr<TmpdirUtil> tmpdir_util_;
};

TEST_F(JavacCompilerInfoBuilderTest, GetJavacVersion) {
  static const char kVersionInfo[] = "javac 1.6.0_43\n";

  std::string version;
  JavacCompilerInfoBuilder::ParseJavacVersion(kVersionInfo, &version);
  EXPECT_EQ("1.6.0_43", version);
}

}  // namespace devtools_goma
