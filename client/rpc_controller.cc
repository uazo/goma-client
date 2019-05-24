// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rpc_controller.h"

#include "glog/logging.h"
#include "prototmp/goma_data.pb.h"
#include "worker_thread_manager.h"

#ifdef _WIN32
#include "callback.h"
#include "compiler_specific.h"
#endif

namespace devtools_goma {

namespace {

// Returns true if header looks like a request coming from browser.
// see also goma_ipc.cc:GomaIPC::SendRequest.
bool IsBrowserRequest(absl::string_view header) {
  // This logic is hard to read. It says:
  // - If `header` contains the string literal, then return false.
  // - If not, then return true.
  // TODO: Use absl::StrContains().
  if (header.find("\r\nHost: 0.0.0.0\r\n") != absl::string_view::npos) {
    return false;
  }
  // TODO: check it doesn't contain Origin header etc?
  return true;
}

}  // namespace

RpcController::RpcController(
    ThreadpoolHttpServer::HttpServerRequest* http_server_request)
    : http_server_request_(http_server_request),
      server_port_(http_server_request->server().port()),
      gomacc_req_size_(0) {
  DCHECK(http_server_request_ != nullptr);
}

RpcController::~RpcController() {
  DCHECK(http_server_request_ == nullptr);
}

bool RpcController::ParseRequest(ExecReq* req) {
  absl::string_view header = http_server_request_->header();
  if (http_server_request_->request_content_length() <= 0) {
    LOG(WARNING) << "Invalid request from client (no content-length):"
                 << header;
    return false;
  }
  // it won't protect request by using network communications API.
  // https://developer.chrome.com/apps/app_network
  if (IsBrowserRequest(header)) {
    LOG(WARNING) << "Unallowed request from browser:" << header;
    return false;
  }
  if (header.find("\r\nContent-Type: binary/x-protocol-buffer\r\n") ==
      absl::string_view::npos) {
    LOG(WARNING) << "Invalid request from client (invalid content-type):"
                 << header;
    return false;
  }

  gomacc_req_size_ = http_server_request_->request_content_length();
  return req->ParseFromArray(http_server_request_->request_content(),
                             http_server_request_->request_content_length());
}

void RpcController::SendReply(const ExecResp& resp) {
  CHECK(http_server_request_ != nullptr);

  size_t gomacc_resp_size = resp.ByteSize();
  std::ostringstream http_response_message;
  http_response_message << "HTTP/1.1 200 OK\r\n"
                        << "Content-Type: binary/x-protocol-buffer\r\n"
                        << "Content-Length: " << gomacc_resp_size << "\r\n\r\n";
  std::string response_string = http_response_message.str();
  int header_size = response_string.size();
  response_string.resize(header_size + gomacc_resp_size);
  resp.SerializeToArray(&response_string[header_size], gomacc_resp_size);
  http_server_request_->SendReply(response_string);
  http_server_request_ = nullptr;
}

void RpcController::NotifyWhenClosed(OneshotClosure* callback) {
  CHECK(http_server_request_ != nullptr);
  http_server_request_->NotifyWhenClosed(callback);
}

}  // namespace devtools_goma
