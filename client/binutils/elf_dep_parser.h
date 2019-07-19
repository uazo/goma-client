// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_DEP_PARSER_H_
#define DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_DEP_PARSER_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "gtest/gtest_prod.h"

namespace devtools_goma {

class ElfDepParser {
 public:
  ElfDepParser(std::string cwd,
               std::vector<std::string> default_search_paths,
               bool ignore_rpath)
      : cwd_(std::move(cwd)),
        default_search_paths_(std::move(default_search_paths)),
        ignore_rpath_(ignore_rpath) {}

  virtual ~ElfDepParser() {}

  ElfDepParser(ElfDepParser&&) = delete;
  ElfDepParser(const ElfDepParser&) = delete;
  ElfDepParser& operator=(const ElfDepParser&) = delete;
  ElfDepParser& operator=(ElfDepParser&&) = delete;

  // List up all library dependencies for |cmd_or_lib| and stored to |deps|.
  // Stored paths will be a relative paths from |cwd_| if there is no
  // absolute paths in RPATH.
  // The function returns true on success.
  bool GetDeps(const absl::string_view cmd_or_lib,
               absl::flat_hash_set<std::string>* deps);

 private:
  // ELF headers used for detecting dependencies.
  // Fields that does not affect detecting dependencies are ommited, and it
  // cannot be used for general purpose ELF Header.  That is why this structure
  // is private.
  // Also, for that purpose, we do not need to distinguish ELF32 and ELF64,
  // and that is why we use the following strucutre instead of structure in
  // elf.h.
  struct ElfHeader {
    uint8_t ei_class;
    uint8_t ei_data;
    uint8_t ei_osabi;
    uint16_t e_machine;

    std::string DebugString() const;

    bool operator==(const ElfHeader& other) const {
      return ei_class == other.ei_class && ei_data == other.ei_data &&
             e_machine == other.e_machine &&
             (ei_osabi == other.ei_osabi ||
              // On Linux, both System V and Linux are used.
              (ei_osabi == 0x03 && other.ei_osabi == 0x00) ||
              (ei_osabi == 0x00 && other.ei_osabi == 0x03));
    }

    bool operator!=(const ElfHeader& other) const { return !(*this == other); }
  };
  FRIEND_TEST(ElfDepParserTest, GetElfHeader);

  // Returns relative library path name if succeeds.
  // Otherwise, empty string will be returned.
  std::string FindLib(const absl::string_view lib_filename,
                      const absl::string_view origin,
                      const std::vector<std::string>& search_paths,
                      const ElfHeader& src_elf_header) const;

  std::string FindLibInDir(absl::string_view dir,
                           const absl::string_view lib_filename,
                           const absl::string_view origin,
                           const ElfHeader& src_elf_header) const;

  static absl::optional<ElfHeader> GetElfHeader(
      const std::string& abs_cmd_or_lib);

  const std::string cwd_;
  const std::vector<std::string> default_search_paths_;
  const bool ignore_rpath_;
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_DEP_PARSER_H_
