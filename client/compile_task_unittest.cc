// Copyright 2016 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "compile_task.h"

#include <memory>
#include <set>
#include <string>

#include <json/json.h>

#include "absl/memory/memory.h"
#include "absl/strings/ascii.h"
#include "callback.h"
#include "compile_stats.h"
#include "compiler_flags.h"
#include "compiler_flags_parser.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "json_util.h"
#include "prototmp/goma_data.pb.h"
#include "rpc_controller.h"
#include "threadpool_http_server.h"
#include "worker_thread_manager.h"

namespace devtools_goma {

namespace {

constexpr int kCompileTaskId = 1234;

std::unique_ptr<ExecReq> CreateExecReqForTest() {
  auto req = absl::make_unique<ExecReq>();
  req->add_arg("clang");
  req->add_arg("foo.cc");
  req->add_arg("-o");
  req->add_arg("foo");
  req->set_cwd("/home/user/code/chromium/src");

  auto input = req->add_input();
  input->set_filename("foo.cc");
  input->set_hash_key("abcdef");

  return req;
}

ScopedCompilerInfoState GetCompilerInfoStateForTest() {
  auto data = absl::make_unique<CompilerInfoData>();
  data->mutable_cxx();

  auto res1 = data->add_resource();
  res1->set_name("/usr/lib/gcc/x86_64-linux-gnu/8/crtbegin.o");
  res1->set_type(CompilerInfoData::CLANG_GCC_INSTALLATION_MARKER);

  auto res2 = data->add_resource();
  res2->set_name("../../third_party/llvm-build/Release+Asserts/bin/clang++");
  res2->set_type(CompilerInfoData::EXECUTABLE_BINARY);
  res2->set_symlink_path("clang");

  auto res3 = data->add_resource();
  res3->set_name("../../third_party/llvm-build/Release+Asserts/bin/clang");
  res3->set_type(CompilerInfoData::EXECUTABLE_BINARY);
  res3->set_is_executable(true);

  auto res4 = data->add_resource();
  res4->set_name(
      "../../third_party/llvm-build/Release+Asserts/bin/../lib/libstdc++.so.6");
  res4->set_type(CompilerInfoData::EXECUTABLE_BINARY);
  res4->set_is_executable(true);

  auto* compiler_info_state = new CompilerInfoState(std::move(data));
  return ScopedCompilerInfoState(compiler_info_state);
}

class DummyHttpHandler : public ThreadpoolHttpServer::HttpHandler {
 public:
  ~DummyHttpHandler() override = default;

  void HandleHttpRequest(
      ThreadpoolHttpServer::HttpServerRequest* http_server_request) override {}

  bool shutting_down() override { return true; }
};

class DummyHttpServerRequest : public ThreadpoolHttpServer::HttpServerRequest {
 public:
  class DummyMonitor : public ThreadpoolHttpServer::Monitor {
   public:
    ~DummyMonitor() override = default;

    void FinishHandle(const ThreadpoolHttpServer::Stat& stat) override {}
  };

  DummyHttpServerRequest(WorkerThreadManager* worker_thread_manager,
                         ThreadpoolHttpServer* http_server)
      : ThreadpoolHttpServer::HttpServerRequest(worker_thread_manager,
                                                http_server,
                                                stat_,
                                                &monitor_) {}

  bool CheckCredential() override { return true; }

  bool IsTrusted() override { return true; }

  void SendReply(const std::string& response) override {}

  void NotifyWhenClosed(OneshotClosure* callback) override { delete callback; }

 private:
  ThreadpoolHttpServer::Stat stat_;
  DummyMonitor monitor_;
};

// ExecServiceClient used for testing. Just returns the HTTP response code that
// is provided to the constructor.
class FakeExecServiceClient : public ExecServiceClient {
 public:
  explicit FakeExecServiceClient(int http_return_code)
      : ExecServiceClient(nullptr, ""),
        http_return_code_(http_return_code) {}
  ~FakeExecServiceClient() override = default;

  void ExecAsync(const ExecReq* req, ExecResp* resp,
                 HttpClient::Status* status,
                 OneshotClosure* callback) override {
    status->http_return_code = http_return_code_;
    callback->Run();
  }

 private:
  int http_return_code_;
};

// Unit tests that require a real instance of CompileTask should inherit from
// this class.
class CompileTaskTest : public ::testing::Test,
                        public CompileTask::DerefCleanupHandler {
 public:
  void SetUp() override {
    // This is required for ProcessCallExec() to pass.
    exec_request_.add_input();

    // Create a task that does not get executed. The values and objects that are
    // passed in are dummy values.
    worker_thread_manager_ = absl::make_unique<WorkerThreadManager>();
    http_server_ =
        absl::make_unique<ThreadpoolHttpServer>("LISTEN_ADDR", 8088, 1,
                                                worker_thread_manager_.get(), 1,
                                                &http_handler_, 3);
    compile_service_ =
        absl::make_unique<CompileService>(worker_thread_manager_.get(), 1);

    http_server_request_ = absl::make_unique<DummyHttpServerRequest>(
        worker_thread_manager_.get(), http_server_.get());
    rpc_controller_ =
        absl::make_unique<RpcController>(http_server_request_.get());

    compile_task_ = new CompileTask(compile_service_.get(), kCompileTaskId);
    compile_task_->SetDerefCleanupHandler(this);
    auto req = CreateExecReqForTest();
    compile_task_->Init(rpc_controller_.get(), std::move(req), &exec_response_,
                        nullptr);
  }

  // Notification callback to indicate that |compile_task_| was deallocated in
  // CompileTask::Deref().
  void OnCleanup(const CompileTask* task) override {
    DCHECK_EQ(task, compile_task_);
    compile_task_ = nullptr;
  }

  void TearDown() override {
    // Make sure all CHECKs pass by signaling the end of a CompileTask.
    rpc_controller_->SendReply(exec_response_);
    worker_thread_manager_->Finish();

    // Force all CompileTasks owned by |compile_service_| to be cleaned up.
    compile_service_.reset();
    if (compile_task_) {
      compile_task_->Deref();
      compile_task_ = nullptr;
    }
  }

  CompileTask* compile_task() const { return compile_task_; }
  const std::unique_ptr<CompileService>& compile_service() const {
    return compile_service_;
  }

 private:
  // These objects need to be initialized at the start of each test.
  std::unique_ptr<WorkerThreadManager> worker_thread_manager_;
  std::unique_ptr<ThreadpoolHttpServer> http_server_;
  std::unique_ptr<CompileService> compile_service_;
  std::unique_ptr<DummyHttpServerRequest> http_server_request_;
  std::unique_ptr<RpcController> rpc_controller_;

  // CompileTask's destructor is private. It must be a bare pointer. Call
  // Deref() to clean up.
  CompileTask* compile_task_ = nullptr;

  // These are objects whose contents are not used. They do not need to be
  // re-initialized at the start of each test.
  ExecReq exec_request_;
  ExecResp exec_response_;
  DummyHttpHandler http_handler_;
};

}  // anonymous namespace

TEST_F(CompileTaskTest, DumpToJsonWithoutRunning) {
  Json::Value json;
  compile_task()->DumpToJson(false, &json);

  EXPECT_EQ(5, json.getMemberNames().size()) << json.toStyledString();
  EXPECT_TRUE(json.isMember("command"));
  EXPECT_TRUE(json.isMember("elapsed"));
  EXPECT_TRUE(json.isMember("id"));
  EXPECT_TRUE(json.isMember("state"));
  EXPECT_TRUE(json.isMember("summaryOnly"));

  EXPECT_FALSE(json.isMember("replied"));

  std::string error_message;

  int id = -1;
  EXPECT_TRUE(GetIntFromJson(json, "id", &id, &error_message)) << error_message;
  EXPECT_EQ(kCompileTaskId, id);

  std::string state;
  EXPECT_TRUE(GetStringFromJson(json, "state", &state, &error_message))
      << error_message;
  EXPECT_EQ("INIT", state);
}

TEST_F(CompileTaskTest, DumpToJsonWithUnsuccessfulStart) {
  // Set the thread ID to something other than the curren thread to pass a
  // DCHECK().
  if (compile_task()->thread_id_ == GetCurrentThreadId()) {
    ++compile_task()->thread_id_;
  }
  // Running without having set proper compiler flags.
  compile_task()->Start();

  Json::Value json;
  compile_task()->DumpToJson(false, &json);
  EXPECT_FALSE(json.isMember("http_status"));
  EXPECT_TRUE(json.isMember("state"));

  std::string error_message;

  std::string state;
  EXPECT_TRUE(GetStringFromJson(json, "state", &state, &error_message))
      << error_message;
  EXPECT_EQ("FINISHED", state);

  // There should be no HTTP response code if there was no call to the server.
  int http_status = -1;
  EXPECT_FALSE(
      GetIntFromJson(json, "http_status", &http_status, &error_message))
      << error_message;
}

TEST_F(CompileTaskTest, DumpToJsonWithValidCallToServer) {
  // FakeExecServiceClient returns HTTP response code 200.
  compile_service()->SetExecServiceClient(
      absl::make_unique<FakeExecServiceClient>(200));
  // Force-set |state_| to enable ProcessCallExec() to run.
  compile_task()->state_ = CompileTask::FILE_REQ;
  compile_task()->ProcessCallExec();

  Json::Value json;
  compile_task()->DumpToJson(false, &json);
  EXPECT_TRUE(json.isMember("http_status"));
  EXPECT_TRUE(json.isMember("state"));

  std::string error_message;

  std::string state;
  EXPECT_TRUE(GetStringFromJson(json, "state", &state, &error_message))
      << error_message;
  EXPECT_EQ("FINISHED", state);

  int http_status = -1;
  EXPECT_TRUE(GetIntFromJson(json, "http_status", &http_status, &error_message))
      << error_message;
  EXPECT_EQ(200, http_status);
}

TEST_F(CompileTaskTest, DumpToJsonWithHTTPErrorCode) {
  // FakeExecServiceClient returns HTTP response code 403.
  compile_service()->SetExecServiceClient(
      absl::make_unique<FakeExecServiceClient>(403));
  // Force-set |state_| to enable ProcessCallExec() to run.
  compile_task()->state_ = CompileTask::FILE_REQ;
  compile_task()->ProcessCallExec();

  Json::Value json;
  compile_task()->DumpToJson(false, &json);

  std::string error_message;

  int http_status = -1;
  EXPECT_TRUE(GetIntFromJson(json, "http_status", &http_status, &error_message))
      << error_message;
  EXPECT_EQ(403, http_status);
}

TEST_F(CompileTaskTest, DumpToJsonWithDone) {
  compile_task()->state_ = CompileTask::FINISHED;
  compile_task()->rpc_ = nullptr;
  compile_task()->rpc_resp_ = nullptr;
  compile_task()->Done();

  Json::Value json;
  compile_task()->DumpToJson(false, &json);

  int replied = 0;
  std::string error_message;
  EXPECT_TRUE(GetIntFromJson(json, "replied", &replied, &error_message))
      << error_message;
  EXPECT_NE(0, replied);
}

TEST_F(CompileTaskTest, UpdateStatsFinished) {
  // Force-set |state_| to enable UpdateStats() to run.
  compile_task()->state_ = CompileTask::FINISHED;
  compile_task()->UpdateStats();

  ASSERT_TRUE(compile_task()->resp_);
  EXPECT_TRUE(compile_task()->resp_->compiler_proxy_goma_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_cache_hit());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_local_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_aborted());
}

TEST_F(CompileTaskTest, UpdateStatsFinishedCacheHit) {
  // Force-set |state_| to enable UpdateStats() to run.
  compile_task()->state_ = CompileTask::FINISHED;
  compile_task()->mutable_stats()->set_cache_hit(true);
  compile_task()->UpdateStats();

  ASSERT_TRUE(compile_task()->resp_);
  EXPECT_TRUE(compile_task()->resp_->compiler_proxy_goma_finished());
  EXPECT_TRUE(compile_task()->resp_->compiler_proxy_goma_cache_hit());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_local_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_aborted());
}

TEST_F(CompileTaskTest, UpdateStatsLocalFinished) {
  // Force-set |state_| to enable UpdateStats() to run.
  compile_task()->state_ = CompileTask::LOCAL_FINISHED;
  compile_task()->UpdateStats();

  ASSERT_TRUE(compile_task()->resp_);
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_cache_hit());
  EXPECT_TRUE(compile_task()->resp_->compiler_proxy_local_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_aborted());
}

TEST_F(CompileTaskTest, UpdateStatsAborted) {
  // Force-set |state_| to enable UpdateStats() to run.
  compile_task()->abort_ = true;
  compile_task()->UpdateStats();

  ASSERT_TRUE(compile_task()->resp_);
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_finished());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_goma_cache_hit());
  EXPECT_FALSE(compile_task()->resp_->compiler_proxy_local_finished());
  EXPECT_TRUE(compile_task()->resp_->compiler_proxy_goma_aborted());
}

TEST_F(CompileTaskTest, OmitDurationFromUserError) {
  // input, expected.
  std::vector<std::pair<std::string, std::string>> testcases = {
      {"compiler_proxy [173.736822ms]: reached max number of active fail "
       "fallbacks",
       "compiler_proxy <duration omitted>: reached max number of active fail "
       "fallbacks"},
  };

  for (const auto& tc : testcases) {
    EXPECT_EQ(tc.second, CompileTask::OmitDurationFromUserError(tc.first));
  }
}

TEST_F(CompileTaskTest, SetCompilerResourcesNoSendCompilerBinary) {
  compile_service()->SetSendCompilerBinaryAsInput(false);
  compile_task()->compiler_info_state_ = GetCompilerInfoStateForTest();

  compile_task()->SetCompilerResources();

  const ExecReq& req = *compile_task()->req_;

  // Only the object file is included as an input.
  ASSERT_EQ(2, req.input_size());
  EXPECT_EQ("foo.cc", req.input(0).filename());
  EXPECT_EQ("/usr/lib/gcc/x86_64-linux-gnu/8/crtbegin.o",
            req.input(1).filename());

  // No toolchain specs included.
  EXPECT_FALSE(req.toolchain_included());
  EXPECT_EQ(0, req.toolchain_specs_size());
}

TEST_F(CompileTaskTest, SetCompilerResourcesSendCompilerBinary) {
  compile_service()->SetSendCompilerBinaryAsInput(true);
  compile_task()->compiler_info_state_ = GetCompilerInfoStateForTest();

  compile_task()->SetCompilerResources();

  const ExecReq& req = *compile_task()->req_;

  // The toolchain files are included as inputs.
  ASSERT_EQ(4, req.input_size());
  EXPECT_EQ("foo.cc", req.input(0).filename());
  EXPECT_EQ("/usr/lib/gcc/x86_64-linux-gnu/8/crtbegin.o",
            req.input(1).filename());
  EXPECT_EQ("../../third_party/llvm-build/Release+Asserts/bin/clang",
            req.input(2).filename());
  EXPECT_EQ(
      "../../third_party/llvm-build/Release+Asserts/bin/../lib/libstdc++.so.6",
      req.input(3).filename());

  EXPECT_TRUE(req.toolchain_included());
  ASSERT_EQ(3, req.toolchain_specs_size());

  // The toolchain specs are included.
  EXPECT_EQ("../../third_party/llvm-build/Release+Asserts/bin/clang++",
            req.toolchain_specs(0).path());
  EXPECT_EQ("clang", req.toolchain_specs(0).symlink_path());

  EXPECT_EQ("../../third_party/llvm-build/Release+Asserts/bin/clang",
            req.toolchain_specs(1).path());
  EXPECT_TRUE(req.toolchain_specs(1).is_executable());

  EXPECT_EQ(
      "../../third_party/llvm-build/Release+Asserts/bin/../lib/libstdc++.so.6",
      req.toolchain_specs(2).path());
  EXPECT_TRUE(req.toolchain_specs(2).is_executable());
}

}  // namespace devtools_goma
