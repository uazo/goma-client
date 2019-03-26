// Copyright 2011 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LINKER_SCRIPT_PARSER_H_
#define DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LINKER_SCRIPT_PARSER_H_

#include <memory>
#include <string>
#include <vector>

#include "basictypes.h"
#include "content_cursor.h"

namespace devtools_goma {

// Linker script parser for Goma.
// It only supports commands dealing with files.
// http://sourceware.org/binutils/docs-2.17/ld/File-Commands.html#File-Commands
// Once Parse successfully done, it returns
//  searchdirs(): sarch directories
//  srartup(): startup object filename, if specified.
//  input(): input files in INPUT, GROUP or AS_NEEDED. may be "-lfile".
//  output(): output file, if specified.
class LinkerScriptParser {
 public:
  // Constructs a parser to read content.
  LinkerScriptParser(std::unique_ptr<Content> content,
                     std::string current_directory,
                     std::vector<std::string> searchdirs,
                     std::string sysroot);
  ~LinkerScriptParser();

  const std::vector<std::string>& searchdirs() const { return searchdirs_; }

  bool Parse();

  const std::string& startup() const { return startup_; }
  const std::vector<std::string>& inputs() const { return inputs_; }
  const std::string& output() const { return output_; }

 private:
  bool ParseUntil(const std::string& term_token);
  bool NextToken(std::string* token);
  bool GetToken(const std::string& token);
  bool ProcessFileList(bool accept_as_needed);
  bool ProcessFile(std::string* filename);
  bool ProcessInclude();
  bool ProcessInput();
  bool ProcessGroup();
  bool ProcessAsNeeded();
  bool ProcessOutput();
  bool ProcessSearchDir();
  bool ProcessStartup();
  bool FindFile(const std::string& filename, std::string* include_file);

  std::unique_ptr<ContentCursor> content_;
  const std::string current_directory_;
  std::vector<std::string> searchdirs_;
  const std::string sysroot_;

  std::string startup_;
  std::vector<std::string> inputs_;
  std::string output_;

  // provided for testing.
  static const char* fakeroot_;

  friend class LinkerScriptParserTest;

  DISALLOW_COPY_AND_ASSIGN(LinkerScriptParser);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_LINKER_LINKER_INPUT_PROCESSOR_LINKER_SCRIPT_PARSER_H_
