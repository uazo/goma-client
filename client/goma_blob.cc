// Copyright 2018 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "goma_blob.h"

#include <stdio.h>

#include "absl/memory/memory.h"
#include "blob/file_blob_downloader.h"
#include "blob/file_service_blob_uploader.h"
MSVC_PUSH_DISABLE_WARNING_FOR_PROTO()
#include "prototmp/goma_data.pb.h"
MSVC_POP_WARNING()

namespace devtools_goma {

BlobClient::Uploader::Uploader(std::string filename)
    : filename_(std::move(filename)) {}

std::unique_ptr<FileDataOutput>
BlobClient::Downloader::OutputFileInfo::NewFileDataOutput() {
  if (this->tmp_filename.empty()) {
    return FileDataOutput::NewStringOutput(this->filename, &this->content);
  }
  const auto& filename = this->tmp_filename;
  // TODO: We might want to restrict paths this program may write?
  remove(filename.c_str());
  return FileDataOutput::NewFileOutput(filename, this->mode);
}

FileBlobClient::FileBlobClient(
    std::unique_ptr<FileServiceHttpClient> file_service_client)
    : BlobClient(), file_service_client_(std::move(file_service_client)) {}

std::unique_ptr<BlobClient::Uploader> FileBlobClient::NewUploader(
    std::string filename,
    const RequesterInfo& requester_info,
    std::string trace_id) {
  return absl::make_unique<FileServiceBlobUploader>(
      std::move(filename), file_service_client_->WithRequesterInfoAndTraceId(
                               requester_info, std::move(trace_id)));
}

std::unique_ptr<BlobClient::Downloader> FileBlobClient::NewDownloader(
    const RequesterInfo& requester_info,
    std::string trace_id) {
  return absl::make_unique<FileBlobDownloader>(
      absl::make_unique<FileServiceBlobDownloader>(
          file_service_client_->WithRequesterInfoAndTraceId(
              requester_info, std::move(trace_id))));
}

}  // namespace devtools_goma
