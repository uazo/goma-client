// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elf_dep_parser.h"

#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "base/path.h"
#include "client/binutils/elf_util.h"
#include "client/mypath.h"
#include "gmock/gmock.h"

namespace devtools_goma {

TEST(ElfDepParserTest, GetElfHeader) {
  const std::string test_dir =
      file::JoinPath(GetMyDirectory(), "..", "..", "test");
  absl::optional<ElfDepParser::ElfHeader> elf_header =
      ElfDepParser::GetElfHeader(file::JoinPath(test_dir, "libc.so"));
  EXPECT_FALSE(elf_header.has_value());

  elf_header = ElfDepParser::GetElfHeader(file::JoinPath(test_dir, "libdl.so"));
  ElfDepParser::ElfHeader expected_elf_header;
  expected_elf_header.ei_class = 2;      // ELF64
  expected_elf_header.ei_data = 1;       // little endian
  expected_elf_header.ei_osabi = 0;      // System V
  expected_elf_header.e_machine = 0x3e;  // AMD64

  EXPECT_TRUE(elf_header.has_value());
  EXPECT_EQ(expected_elf_header, *elf_header);
}

// This test confirms that ElfDepParser uses cwd parameter given to
// the constructor.  This is regression test for b/137909009.
TEST(ElfDepParserTest, GetDepsSmokeWithDifferentCwd) {
  std::vector<std::string> searchpath = LoadLdSoConf("/etc/ld.so.conf");
  ElfDepParser edp(file::JoinPath(GetMyDirectory(), "..", "..", "test"),
                   searchpath, true);
  absl::flat_hash_set<std::string> deps;
  EXPECT_TRUE(edp.GetDeps("libdl.so", &deps));
  EXPECT_THAT(deps, ::testing::Contains(::testing::HasSubstr("/libc.so.")));
}

}  // namespace devtools_goma
