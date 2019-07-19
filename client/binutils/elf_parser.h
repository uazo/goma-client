// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_PARSER_H_
#define DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_PARSER_H_

#include <memory>
#include <string>
#include <vector>

namespace devtools_goma {

class ElfParser {
 public:
  static std::unique_ptr<ElfParser> NewElfParser(const std::string& filename);
  virtual ~ElfParser() {}
  ElfParser(const ElfParser&) = delete;
  ElfParser& operator=(const ElfParser&) = delete;
  virtual void UseProgramHeader(bool use_program_header) = 0;
  virtual bool ReadDynamicNeeded(std::vector<std::string>* needed) = 0;
  virtual bool ReadDynamicNeededAndRpath(std::vector<std::string>* needed,
                                         std::vector<std::string>* rpath) = 0;

  static bool IsElf(const std::string& filename);

 protected:
  virtual bool valid() const = 0;
  ElfParser() {}
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_BINUTILS_ELF_PARSER_H_
