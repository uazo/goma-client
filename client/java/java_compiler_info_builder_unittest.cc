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

#ifndef _WIN32
TEST_F(JavacCompilerInfoBuilderTest, AddResourceAsExecutableBinary) {
  const auto cwd = tmpdir_util_->realcwd();
  auto AddResourceAsExecutableBinary =
      JavacCompilerInfoBuilder::AddResourceAsExecutableBinary;
  const std::string compiler_path = "javac";
  const std::string compiler_data = "javac contents";

  {
    // Test compiler file that doesn't exist.
    CompilerInfoData data;
    EXPECT_FALSE(AddResourceAsExecutableBinary(compiler_path, cwd, &data));
    EXPECT_TRUE(data.has_error_message());
  }

  {
    // Test compiler file that does exist.
    tmpdir_util_->CreateTmpFile(compiler_path, compiler_data);
    const auto full_compiler_path = file::JoinPath(cwd, compiler_path);
    chmod(full_compiler_path.c_str(), 0755);  // Make it executable.

    CompilerInfoData data;
    EXPECT_TRUE(AddResourceAsExecutableBinary(compiler_path, cwd, &data));
    EXPECT_FALSE(data.has_error_message());

    ASSERT_EQ(1U, data.resource_size());
    const auto& resource = data.resource(0);
    EXPECT_EQ(compiler_path, resource.name());
    EXPECT_EQ(CompilerInfoData::EXECUTABLE_BINARY, resource.type());
    EXPECT_TRUE(resource.is_executable());
  }

  // TODO: Test symlink.
}
#endif  // _WIN32

}  // namespace devtools_goma
