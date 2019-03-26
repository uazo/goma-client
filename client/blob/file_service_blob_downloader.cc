// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "file_service_blob_downloader.h"

#include <utility>

#include "file_data_output.h"
#include "goma_file_http.h"

namespace devtools_goma {

FileServiceBlobDownloader::FileServiceBlobDownloader(
    std::unique_ptr<FileServiceHttpClient> file_service)
    : file_service_(std::move(file_service)) {}

bool FileServiceBlobDownloader::Download(const ExecResult_Output& output,
                                         OutputFileInfo* info) {
  // TODO: Previously, this code passed |output.filename()| to
  // FileDataOutput::NewStringOutput(), but the StringOutput class only uses the
  // filename for debug purposes, in ToString().
  //
  // In the current version of the code, |output.filename()| is not passed in.
  // If we want to use this debug string, then we should store
  // |output.filename()| in |info->filename| before calling NewFileDataOutput().
  auto file_data_output = info->NewFileDataOutput();
  return file_service_->OutputFileBlob(output.blob(), file_data_output.get());
}

}  // namespace devtools_goma
