// Copyright 2019 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVTOOLS_GOMA_CLIENT_RPC_CONTROLLER_H_
#define DEVTOOLS_GOMA_CLIENT_RPC_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "basictypes.h"
#include "lockhelper.h"
#include "threadpool_http_server.h"

namespace devtools_goma {

class ExecReq;
class ExecResp;
class OneshotClosure;
class WorkerThreadManager;

class RpcController {
 public:
  explicit RpcController(
      ThreadpoolHttpServer::HttpServerRequest* http_server_request);
  ~RpcController();

  bool ParseRequest(ExecReq* req);
  void SendReply(const ExecResp& resp);

  // Notifies callback when original request is closed.
  // Can be called from any thread.
  // callback will be called on the thread where this method is called.
  void NotifyWhenClosed(OneshotClosure* callback);

  int server_port() const { return server_port_; }

  size_t gomacc_req_size() const { return gomacc_req_size_; }

 private:
  friend class CompileService;
  ThreadpoolHttpServer::HttpServerRequest* http_server_request_;
  int server_port_;

  size_t gomacc_req_size_;

  DISALLOW_COPY_AND_ASSIGN(RpcController);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_RPC_CONTROLLER_H_
