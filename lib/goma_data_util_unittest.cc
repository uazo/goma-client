// Copyright 2014 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "lib/goma_data_util.h"

#include "gtest/gtest.h"
#include "prototmp/goma_data.pb.h"

namespace devtools_goma {

TEST(GomaProtoUtilTest, IsSameSubprogramShouldBeTrueOnEmptyProto) {
  ExecReq req;
  ExecResp resp;

  EXPECT_TRUE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsSameSubprogramShouldIgnorePath) {
  ExecReq req;
  ExecResp resp;

  SubprogramSpec dummy_spec;
  dummy_spec.set_binary_hash("dummy_hash");

  SubprogramSpec* spec;
  spec = req.add_subprogram();
  *spec = dummy_spec;
  spec->set_path("request/path");

  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec;
  spec->set_path("response/path");

  EXPECT_TRUE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsSameSubprogramShouldBeTrueIfSameEntries) {
  ExecReq req;
  ExecResp resp;

  SubprogramSpec dummy_spec;
  dummy_spec.set_path("dummy_path");
  dummy_spec.set_binary_hash("dummy_hash");

  SubprogramSpec dummy_spec2;
  dummy_spec.set_path("dummy_path2");
  dummy_spec.set_binary_hash("dummy_hash2");

  SubprogramSpec* spec;
  spec = req.add_subprogram();
  *spec = dummy_spec;
  spec = req.add_subprogram();
  *spec = dummy_spec2;

  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec;
  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec2;

  EXPECT_TRUE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsSameSubprogramShouldBeTrueEvenIfOderIsDifferent) {
  ExecReq req;
  ExecResp resp;

  SubprogramSpec dummy_spec;
  dummy_spec.set_path("dummy_path");
  dummy_spec.set_binary_hash("dummy_hash");

  SubprogramSpec dummy_spec2;
  dummy_spec.set_path("dummy_path2");
  dummy_spec.set_binary_hash("dummy_hash2");

  SubprogramSpec* spec;
  spec = req.add_subprogram();
  *spec = dummy_spec;
  spec = req.add_subprogram();
  *spec = dummy_spec2;

  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec2;
  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec;

  EXPECT_TRUE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsSameSubprogramShouldBeFalseOnSizeMismatch) {
  ExecReq req;
  ExecResp resp;

  SubprogramSpec* spec;
  spec = req.add_subprogram();
  spec->set_path("dummy_path");
  spec->set_binary_hash("dummy_hash");

  EXPECT_FALSE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsSameSubprogramShouldBeFalseOnContentsMismatch) {
  ExecReq req;
  ExecResp resp;

  SubprogramSpec dummy_spec;
  dummy_spec.set_path("dummy_path");

  SubprogramSpec* spec;
  spec = req.add_subprogram();
  *spec = dummy_spec;
  spec->set_binary_hash("dummy_hash");

  spec = resp.mutable_result()->add_subprogram();
  *spec = dummy_spec;
  spec->set_binary_hash("different_hash");

  EXPECT_FALSE(IsSameSubprograms(req, resp));
}

TEST(GomaProtoUtilTest, IsValidFileBlob) {
  // Make sure no false negatives.
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE);
    blob.set_file_size(10);
    blob.set_content("0123456789");
    EXPECT_TRUE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_META);
    blob.set_file_size(3 * 1024 * 1024);
    blob.add_hash_key("9633160e593892033e6a323631000f36457383c2");
    blob.add_hash_key("b155db10844d1ce7049a12e8c05e7eb6e45d7275");
    EXPECT_TRUE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_CHUNK);
    blob.set_file_size(2 * 1024 * 1024 + 10);
    blob.set_offset(2 * 1024 * 1024);
    blob.set_content("0123456789");
    EXPECT_TRUE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_REF);
    blob.set_file_size(10);
    blob.add_hash_key("9633160e593892033e6a323631000f36457383c2");
    EXPECT_TRUE(IsValidFileBlob(blob));
  }
  // We don't have to check each invalid case.
  {
    FileBlob blob;
    // Unspecified type.
    EXPECT_FALSE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE);
    blob.set_file_size(10);
    blob.set_content("012345678");
    // Content does not match size.
    EXPECT_FALSE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_META);
    blob.set_file_size(300);
    blob.add_hash_key("9633160e593892033e6a323631000f36457383c2");
    // Single hash key.
    EXPECT_FALSE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_CHUNK);
    blob.set_content("0123456789");
    // No offset.
    EXPECT_FALSE(IsValidFileBlob(blob));
  }
  {
    FileBlob blob;
    blob.set_blob_type(FileBlob::FILE_REF);
    blob.set_content("0123456789");
    // No file_size.
    EXPECT_FALSE(IsValidFileBlob(blob));
  }
}

}  // namespace devtools_goma
