// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "elf_dep_parser.h"

#include "absl/base/macros.h"
#include "absl/strings/match.h"
#include "absl/strings/str_replace.h"
#include "base/path.h"
#include "client/binutils/elf_parser.h"
#include "glog/logging.h"
#include "glog/stl_logging.h"
#include "lib/path_resolver.h"
#include "lib/path_util.h"
#include "lib/scoped_fd.h"

namespace devtools_goma {

bool ElfDepParser::GetDeps(const absl::string_view cmd_or_lib,
                           absl::flat_hash_set<std::string>* deps) {
  const std::string abs_cmd_or_lib =
      file::JoinPathRespectAbsolute(cwd_, cmd_or_lib);
  std::unique_ptr<ElfParser> ep = ElfParser::NewElfParser(abs_cmd_or_lib);
  if (ep == nullptr) {
    LOG(ERROR) << "failed to open ELF file."
               << " abs_cmd_or_lib=" << abs_cmd_or_lib;
    return false;
  }
  std::vector<std::string> libs;
  std::vector<std::string> rpaths;
  if (!ep->ReadDynamicNeededAndRpath(&libs, &rpaths)) {
    LOG(ERROR) << "failed to get libs and rpaths."
               << " abs_cmd_or_lib=" << abs_cmd_or_lib;
    return false;
  }

  absl::optional<ElfDepParser::ElfHeader> elf_header =
      GetElfHeader(abs_cmd_or_lib);
  if (!elf_header.has_value()) {
    LOG(ERROR) << "failed to get elf header."
               << " abs_cmd_or_lib=" << abs_cmd_or_lib;
    return false;
  }

  // keep libs for bredth first search.
  std::vector<std::string> libs_to_search;
  for (const auto& lib : libs) {
    std::string lib_path =
        FindLib(lib, file::Dirname(cmd_or_lib), rpaths, *elf_header);
    if (lib_path.empty()) {
      LOG(ERROR) << "failed to find dependent library."
                 << " lib=" << lib << " rpaths=" << rpaths
                 << " default_search_path=" << default_search_paths_;
      return false;
    }
    // No need to see a known library.
    if (deps->contains(lib_path)) {
      continue;
    }
    CHECK(deps->insert(lib_path).second);
    libs_to_search.push_back(std::move(lib_path));
  }
  for (const auto& lib : libs_to_search) {
    if (!GetDeps(lib, deps)) {
      return false;
    }
  }

  return true;
}

std::string ElfDepParser::FindLib(
    const absl::string_view lib_filename,
    const absl::string_view origin,
    const std::vector<std::string>& search_paths,
    const ElfDepParser::ElfHeader& src_elf_header) const {
  // TODO: search DT_RUNPATH after LD_LIBRARY_PATH.
  // According to GNU ls.so manual, libraries are searched in following order:
  // 1. DT_RPATH (if --inhibit-cache is not empty string or ':' and no
  //    DT_RUNPATH)
  // 2. LD_LIBRARY_PATH (which can be overwritten by --library-path)
  //    The value should be passed via |default_search_path|.
  // 3. DT_RUNPATH (we searches RUNPATH in 1. in this impl. Weshould fix it.)
  // 4. path in ldconfig cache (we do not support this)
  // 5. trusted default paths (we do not support this)
  if (!ignore_rpath_) {
    for (const auto& dir : search_paths) {
      std::string lib = FindLibInDir(dir, lib_filename, origin, src_elf_header);
      if (!lib.empty()) {
        return lib;
      }
    }
  }
  for (const std::string& dir : default_search_paths_) {
    std::string lib = FindLibInDir(dir, lib_filename, origin, src_elf_header);
    if (!lib.empty()) {
      return lib;
    }
  }
  return std::string();
}

std::string ElfDepParser::FindLibInDir(
    absl::string_view dir,
    const absl::string_view lib_filename,
    const absl::string_view origin,
    const ElfDepParser::ElfHeader& src_elf_header) const {
  std::string new_dir = absl::StrReplaceAll(dir, {
                                                     {"$ORIGIN", origin},
                                                 });
  if (PathResolver::ResolvePath(new_dir) == PathResolver::ResolvePath(origin)) {
    dir = origin;
  } else {
    dir = new_dir;
  }
  if (absl::StrContains(dir, "$")) {
    LOG(ERROR) << "found non supported $ pattern."
               << " dir=" << dir;
    return std::string();
  }
  std::string path = file::JoinPathRespectAbsolute(dir, lib_filename);
  std::string abs_path = file::JoinPathRespectAbsolute(cwd_, path);
  if (access(abs_path.c_str(), R_OK) != 0) {
    return std::string();
  }

  absl::optional<ElfDepParser::ElfHeader> elf_header = GetElfHeader(abs_path);
  if (!elf_header.has_value()) {
    return std::string();
  }
  if (elf_header != src_elf_header) {
    LOG(INFO) << "file exists but header mismatches."
              << " path=" << path << " elf_header=" << elf_header->DebugString()
              << " src_elf_header=" << src_elf_header.DebugString();
    return std::string();
  }
  VLOG(2) << "origin:" << origin << " path:" << path;

  return path;
}

/* static */
absl::optional<ElfDepParser::ElfHeader> ElfDepParser::GetElfHeader(
    const std::string& abs_cmd_or_lib) {
  DCHECK(IsPosixAbsolutePath(abs_cmd_or_lib))
      << "not absolute path: " << abs_cmd_or_lib;
  ElfDepParser::ElfHeader elf_header;
  static constexpr char kELFMagic[] = {0x7f, 'E', 'L', 'F'};
  ScopedFd fd(ScopedFd::OpenForRead(abs_cmd_or_lib));
  if (!fd.valid()) {
    LOG(WARNING) << "failed to open " << abs_cmd_or_lib;
    return absl::nullopt;
  }

  // read common part of ELF32 and ELF64 headers.
  char buf[0x18];
  if (fd.Read(buf, sizeof(buf)) != sizeof(buf)) {
    LOG(WARNING) << "failed to read possibly ELF file."
                 << " abs_cmd_or_lib=" << abs_cmd_or_lib;
    return absl::nullopt;
  }
  if (memcmp(buf, kELFMagic, ABSL_ARRAYSIZE(kELFMagic)) != 0) {
    LOG(WARNING) << "file do not have ELF magic."
                 << " abs_cmd_or_lib=" << abs_cmd_or_lib;
    return absl::nullopt;
  }
  elf_header.ei_class = static_cast<uint8_t>(buf[0x04]);
  elf_header.ei_data = static_cast<uint8_t>(buf[0x05]);
  elf_header.ei_osabi = static_cast<uint8_t>(buf[0x07]);
  elf_header.e_machine = static_cast<uint16_t>(buf[0x12]) |
                         static_cast<uint16_t>(buf[0x13]) << 0x8;

  VLOG(1) << "elf header: abs_cmd_or_lib=" << abs_cmd_or_lib
          << " elf_header=" << elf_header.DebugString();
  return elf_header;
}

std::string ElfDepParser::ElfHeader::DebugString() const {
  return absl::StrCat("ei_class=", ei_class, ",", "ei_data=", ei_data, ",",
                      "ei_osabi=", ei_osabi, ",", "e_machine=", e_machine, ",");
}

}  // namespace devtools_goma
