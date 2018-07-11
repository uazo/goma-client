// Copyright 2014 The Goma Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef DEVTOOLS_GOMA_CLIENT_HTTP_H_
#define DEVTOOLS_GOMA_CLIENT_HTTP_H_

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#else
#include "socket_helper_win.h"
#endif

#include <json/json.h>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "basictypes.h"
#include "gtest/gtest_prod.h"
#include "lockhelper.h"
#include "luci_context.h"
#include "oauth2.h"
#include "tls_engine.h"
#include "worker_thread_manager.h"

using std::string;

namespace google {
namespace protobuf {
namespace io {
class ZeroCopyInputStream;
}  // namespace io
}  // namespace protobuf
}  // namespace google

namespace devtools_goma {

class Descriptor;
class Histogram;
class HttpRequest;
class HttpResponse;
class HttpRPCStats;
class OAuth2AccessTokenRefreshTask;
class OneshotClosure;
class SocketFactory;

// HttpClient is a HTTP client.  It sends HttpRequest on Descriptor
// generated by SocketFactory and TLSEngineFactory, and receives
// the response in HttpResponse.
class HttpClient {
 public:
  struct Options {
    Options();
    string dest_host_name;
    int dest_port;
    string proxy_host_name;
    int proxy_port;
    string extra_params;
    string authorization;
    string cookie;
    bool capture_response_header;
    string url_path_prefix;
    string http_host_name;
    bool use_ssl;
    string ssl_extra_cert;
    string ssl_extra_cert_data;
    int ssl_crl_max_valid_duration;
    double socket_read_timeout_sec;
    int min_retry_backoff_ms;
    int max_retry_backoff_ms;

    OAuth2Config oauth2_config;
    string gce_service_account;
    string service_account_json_filename;
    LuciContextAuth luci_context_auth;

    bool fail_fast;
    int network_error_margin;
    int network_error_threshold_percent;

    // Allows throttling if this is true.
    bool allow_throttle;

    bool reuse_connection;

    bool InitFromURL(absl::string_view url);

    string SocketHost() const;
    int SocketPort() const;
    string RequestURL(absl::string_view path) const;
    string Host() const;

    string DebugString() const;
    void ClearAuthConfig();
  };

  // Status is used for each HTTP transaction.
  // Caller can specify
  //  - timeout_should_be_http_error
  //  - timeouts.
  // The other fields are filled by HttpClient.
  struct Status {
    enum State {
      // Running state. If failed in some step, State would be kept as-is.
      // Then, caller of HttpClient can know where HttpClient failed.
      INIT,
      PENDING,
      SENDING_REQUEST,
      REQUEST_SENT,
      RECEIVING_RESPONSE,
      RESPONSE_RECEIVED,
    };
    Status();

    // HACK: to provide copy constructor of std::atomic<bool>.
    struct AtomicBool {
      std::atomic<bool> value;

      AtomicBool(bool b) : value(b) {}  // NOLINT
      AtomicBool(const AtomicBool& b) : value(b.value.load()) {}
      AtomicBool& operator=(const AtomicBool& b) {
        value = b.value.load();
        return *this;
      }
      AtomicBool& operator=(bool b) {
        value = b;
        return *this;
      }
      operator bool() const {
        return value.load();
      }
    };

    State state;

    // If true, timeout is treated as http error (default).
    bool timeout_should_be_http_error;
    std::deque<int> timeout_secs;

    // Whether connect() was successful for this request.
    bool connect_success;

    // Whether RPC was finished or not.
    AtomicBool finished;

    // Result of RPC for CallWithAsync. OK=success, or error code.
    int err;
    string err_message;

    // Become false if http is disabled with failnow().
    bool enabled;

    int http_return_code;
    string response_header;

    // size of message on http (maybe compressed).
    size_t req_size;
    size_t resp_size;

    // size of serialized message (not compressed).
    // for now, it only for proto messages on HttpRPC.
    // TODO: set this for compressed test message or so.
    size_t raw_req_size;
    size_t raw_resp_size;

    // in milliseconds.
    int throttle_time;
    int pending_time;
    int req_build_time;
    int req_send_time;
    int wait_time;
    int resp_recv_time;
    int resp_parse_time;

    int num_retry;
    int num_throttled;
    int num_connect_failed;

    string trace_id;
    string master_trace_id;  // master request in multi http rpc.

    string DebugString() const;
  };

  enum ConnectionCloseState {
    NO_CLOSE,
    NORMAL_CLOSE,
    ERROR_CLOSE,
  };

  // NetworkErrorMonitor can be attached to HttpClient.
  // When network error is detected, or network is recovered,
  // corresponding method will be called.
  // These methods will be called with under mu_ is locked
  // to be called in serial.
  class NetworkErrorMonitor {
   public:
    virtual ~NetworkErrorMonitor() {}
    // Called when http request was not succeeded.
    virtual void OnNetworkErrorDetected() = 0;
    // Called when http request was succeeded after network error started.
    virtual void OnNetworkRecovered() = 0;
  };

  // Request is a request of HTTP transaction.
  class Request {
   public:
    Request();
    virtual ~Request();

    void Init(const string& method, const string& path,
              const Options& options);

    void SetMethod(const string& method);
    void SetRequestPath(const string& path);
    const string& request_path() const { return request_path_; }
    void SetHost(const string& host);
    void SetContentType(const string& content_type);
    void SetAuthorization(const string& authorization);
    void SetCookie(const string& cookie);
    void AddHeader(const string& key, const string& value);

    // CreateMessage returns HTTP request message.
    virtual string CreateMessage() const = 0;

    // Clone returns clone of this Request.
    virtual std::unique_ptr<Request> Clone() const = 0;

   protected:
    // CreateHeader creates a header line.
    static string CreateHeader(const string& key, const string& value);

    // BuildMessage creates HTTP request message with additional headers
    // and body.
    string BuildMessage(const std::vector<string>& headers,
                        absl::string_view body) const;

   private:
    string method_;
    string request_path_;
    string host_;
    string content_type_;
    string authorization_;
    string cookie_;
    std::vector<string> headers_;

    DISALLOW_ASSIGN(Request);
  };

  // Response is a response of HTTP transaction.
  class Response {
   public:
    Response();
    virtual ~Response();

    bool HasHeader() const;
    absl::string_view Header() const;

    // HttpClient will use the following methods to receive HTTP response.
    void SetRequestPath(const string& path);
    void SetTraceId(const string& trace_id);
    void Reset();

    // Buffer returns a buffer pointer and buffer's size.
    // Received data should be filled in buf[0..buf_size), and call
    // Recv with number data received in the buffer.
    void Buffer(char** buf, int* buf_size);

    // Recv receives r bytes in the buffer specified by Buffer().
    // Returns true if all HTTP response is received so ready to parse.
    // Returns false if more data is needed to parse response.
    bool Recv(int r);

    // Parse parses a HTTP response message.
    void Parse();

    // Number of bytes already received.
    size_t len() const { return len_; }

    // Maximum buffer size at the moment.
    // HttpResponse grows buffer size in Buffer if necessary.
    size_t buffer_size() const { return buffer_.size(); }

    // Remaining bytes for complete responses.
    // remaining might be zero, if it is not yet known.
    // Use Recv to check complete response has been received or not.
    size_t remaining() const { return remaining_; }

    // status_code reports HTTP status code.
    int status_code() const { return status_code_; }

    // result reports transaction results. OK or FAIL.
    int result() const { return result_; }
    const string& err_message() const { return err_message_; }

    // represents whether response has 'Connection: close' header.
    bool HasConnectionClose() const;

   protected:
    // ParseBody parses body.
    // if error occured, updates result_, err_message_.
    virtual void ParseBody(
        google::protobuf::io::ZeroCopyInputStream* input) = 0;

    int result_;
    string err_message_;
    string trace_id_;

   private:
    string request_path_;

    string buffer_;  // whole buffer
    size_t len_;     // received length in buffer_
    size_t body_offset_;  // position to start response body in buffer_
    size_t content_length_;  // content length specified in http response header
    bool is_chunked_;  // chunked transfer encoding?
    size_t remaining_;  // remaining bytes for full response.
    std::vector<absl::string_view> chunks_;

    int status_code_;

    DISALLOW_COPY_AND_ASSIGN(Response);
  };


  static std::unique_ptr<SocketFactory> NewSocketFactoryFromOptions(
      const Options& options);
  static std::unique_ptr<TLSEngineFactory> NewTLSEngineFactoryFromOptions(
      const Options& options);

  // HttpClient is a http client to a specific server.
  // Takes ownership of socket_factory and tls_engine_factory.
  // It doesn't take ownership of wm.
  HttpClient(std::unique_ptr<SocketFactory> socket_factory,
             std::unique_ptr<TLSEngineFactory> tls_engine_factory,
             const Options& options,
             WorkerThreadManager* wm);
  ~HttpClient();

  // Initializes Request for method and path.
  void InitHttpRequest(
      Request* req, const string& method, const string& path) const;

  // Do performs a HTTP transaction.
  // Caller have ownership of req, resp and status.
  // This is synchronous call.
  void Do(const Request* req, Response* resp, Status* status);

  // DoAsync initiates a HTTP transaction.
  // Caller have ownership of req, resp and status, until callback is called
  // (if callback is not NULL) or status->finished becomes true (if callback
  // is NULL).
  void DoAsync(const Request* req, Response* resp,
               Status* status,
               OneshotClosure* callback);

  // Wait waits for a HTTP transaction initiated by DoAsync with callback=NULL.
  void Wait(Status* status);

  // Shutdown the client. all on-the-fly requests will fail.
  void Shutdown();
  bool shutting_down() const;

  // ramp_up return [0, 100].
  // ramp_up == 0 means 0% of requests could be sent.
  // ramp_up == 100 means 100% of requests could be sent.
  // when !enabled(), it returns 0.
  // when enabled_from_ == 0, it returns 100.
  int ramp_up() const;

  // IsHealthyRecently returns false if more than given percentage
  // (via options_.network_error_threshold_percent) of http requests in
  // last 3 seconds having status code other than 200.
  bool IsHealthyRecently();
  string GetHealthStatusMessage() const;
  bool IsHealthy() const;

  // Get email address to login with oauth2.
  string GetAccount();
  bool GetOAuth2Config(OAuth2Config* config) const;
  bool SetOAuth2Config(const OAuth2Config& config);

  string DebugString() const;

  void DumpToJson(Json::Value* json) const;
  void DumpStatsToProto(HttpRPCStats* stats) const;

  // options used to construct this client.
  // Note that oauth2_config might have been updated and differ from this one.
  // Use GetOAuth2Config above.
  const Options& options() const { return options_; }

  // Calculate next backoff msec.
  // prev_backoff_msec must be positive.
  static int BackoffMsec(
      const Options& option, int prev_backoff_msec, bool in_error);

  // public for HttpRPC ping.
  void IncNumActive();
  void DecNumActive();
  // Provided for test that checks socket_pool status.
  // A test should wait all in-flight tasks land.
  void WaitNoActive();

  int UpdateHealthStatusMessageForPing(
      const Status& status, int round_trip_time);

  // NetworkErrorStartedTime return a time network error started.
  // Returns 0 if no error occurred recently.
  // The time will be set on fatal http error (302, 401, 403) and when
  // no socket in socket pool is available to connect to the host.
  // The time will be cleared when HttpClient get 2xx response.
  time_t NetworkErrorStartedTime() const;

  // Takes the ownership.
  void SetMonitor(std::unique_ptr<NetworkErrorMonitor> monitor);

  static const char kGomaLength[];

 private:
  class Task;
  friend class Task;

  struct TrafficStat {
    TrafficStat();
    int read_byte;
    int write_byte;
    int query;
    int http_err;
  };
  typedef std::deque<TrafficStat> TrafficHistory;

  // NetworkErrorStatus checks the network error is continued
  // from the previous error or not.
  // Thread-unsafe, must be guarded by mutex.
  class NetworkErrorStatus {
   public:
    explicit NetworkErrorStatus(int margin)
        : error_recover_margin_(margin),
          error_started_time_(0),
          error_until_(0) {}

    // Returns the network error started time.
    // 0 if network is not in the error state.
    time_t NetworkErrorStartedTime() const { return error_started_time_; }
    time_t NetworkErrorUntil() const { return error_until_; }

    // Call this when the network access was error.
    // Returns true if a new network error is detected.
    // This will convert level trigger to edge trigger.
    bool OnNetworkErrorDetected(time_t now);

    // Call this when network access was not error.
    // Even this called, we keep the error until |error_until_|.
    // Returns true if the network is really recovered.
    // This will convert level trigger to edge trigger.
    bool OnNetworkRecovered(time_t now);

   private:
    const int error_recover_margin_;
    // 0 if network is not in the error state. Otherwise, time when the network
    // error has started.
    time_t error_started_time_;
    // Even we get the 2xx http status, we consider the network is still
    // in the error state until this time.
    time_t error_until_;
  };

  // |may_retry| is provided for initial ping.
  Descriptor* NewDescriptor();
  void ReleaseDescriptor(Descriptor* d, ConnectionCloseState close_state);

  double EstimatedRecvTime(size_t bytes);

  string GetOAuth2Authorization() const;
  bool ShouldRefreshOAuth2AccessToken() const;
  void RunAfterOAuth2AccessTokenGetReady(
      WorkerThreadManager::ThreadId thread_id,
      OneshotClosure* callback);

  void UpdateBackoffUnlocked(bool in_error) EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Returns time to wait in the queue. If returns 0, no need to wait.
  int TryStart();

  void IncNumPending();
  void DecNumPending();

  // Returns milliseconds time to wait in the queue on error.
  int GetRandomizeBackoffTimeInMs();

  // return true if shutting_down or disabled.
  bool failnow() const;

  void IncReadByte(int n);
  void IncWriteByte(int n);

  void UpdateStats(const Status& status);

  void UpdateTrafficHistory();

  void NetworkErrorDetectedUnlocked() EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void NetworkRecoveredUnlocked() EXCLUSIVE_LOCKS_REQUIRED(mu_);

  void UpdateStatusCodeHistoryUnlocked() EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void AddStatusCodeHistoryUnlocked(int status_code)
      EXCLUSIVE_LOCKS_REQUIRED(mu_);

  const Options options_;
  std::unique_ptr<TLSEngineFactory> tls_engine_factory_;
  std::unique_ptr<SocketFactory> socket_pool_;
  std::unique_ptr<OAuth2AccessTokenRefreshTask> oauth_refresh_task_;

  WorkerThreadManager* wm_;

  mutable Lock mu_;
  ConditionVariable cond_;  // signaled when num_active_ is 0.
  string health_status_ GUARDED_BY(mu_);
  bool shutting_down_ GUARDED_BY(mu_);
  std::deque<std::pair<time_t, int>> recent_http_status_code_ GUARDED_BY(mu_);
  size_t bad_status_num_in_recent_http_ GUARDED_BY(mu_);

  std::unique_ptr<NetworkErrorMonitor> monitor_ GUARDED_BY(mu_);
  // Checking network error state. When we get fatal http error
  // defined in IsFatalNetworkErrorCode(), or when no socket in socket pool is
  // available to connect to the host, we consider the network error.
  // When we get 2xx HTTP responses for specified duration, we consider
  // the network is recovered.
  // For the other error, this does not care.
  NetworkErrorStatus network_error_status_ GUARDED_BY(mu_);;

  int num_query_ GUARDED_BY(mu_);
  int num_active_ GUARDED_BY(mu_);
  int total_pending_ GUARDED_BY(mu_);
  int peak_pending_  GUARDED_BY(mu_);
  int num_pending_ GUARDED_BY(mu_);
  int num_http_retry_ GUARDED_BY(mu_);
  int num_http_timeout_ GUARDED_BY(mu_);
  int num_http_error_ GUARDED_BY(mu_);

  size_t total_write_byte_ GUARDED_BY(mu_);
  size_t total_read_byte_ GUARDED_BY(mu_);
  size_t num_writable_ GUARDED_BY(mu_);
  size_t num_readable_ GUARDED_BY(mu_);
  std::unique_ptr<Histogram> read_size_ GUARDED_BY(mu_);
  std::unique_ptr<Histogram> write_size_ GUARDED_BY(mu_);

  size_t total_resp_byte_ GUARDED_BY(mu_);
  long total_resp_time_ GUARDED_BY(mu_);  // msec.

  int ping_http_return_code_ GUARDED_BY(mu_);
  int ping_round_trip_time_ms_ GUARDED_BY(mu_);

  std::map<int, int> num_http_status_code_ GUARDED_BY(mu_);
  TrafficHistory traffic_history_ GUARDED_BY(mu_);
  PeriodicClosureId traffic_history_closure_id_ GUARDED_BY(mu_);
  int retry_backoff_ms_;
  // if enabled_from_ > 0,
  //   t < enabled_from, then it will be disabled,
  //   enabled_from <= t, then it is in ramp up period
  // where t=time().
  // if enabled_from_ == 0, it is enabled (without checking time()).
  time_t enabled_from_ GUARDED_BY(mu_);

  int num_network_error_ GUARDED_BY(mu_);
  int num_network_recovered_ GUARDED_BY(mu_);

  FRIEND_TEST(NetworkErrorStatus, BasicTest);
  DISALLOW_COPY_AND_ASSIGN(HttpClient);
};

// HttpRequest is a request of HTTP transaction.
class HttpRequest : public HttpClient::Request {
 public:
  HttpRequest();
  ~HttpRequest() override;

  void SetBody(const string& body);

  string CreateMessage() const override;

  std::unique_ptr<HttpClient::Request> Clone() const override {
    return std::unique_ptr<HttpClient::Request>(new HttpRequest(*this));
  }

 private:
  string body_;

  DISALLOW_ASSIGN(HttpRequest);
};

// HttpResponse is a response of HTTP transaction.
class HttpResponse : public HttpClient::Response {
 public:
  HttpResponse();
  ~HttpResponse() override;

  const string& Body() const;

 protected:
  // ParseBody parses body.
  // if error occured, updates result_, err_message_.
  void ParseBody(google::protobuf::io::ZeroCopyInputStream* input) override;

 private:
  string parsed_body_;  // dechunked and uncompressed

  DISALLOW_COPY_AND_ASSIGN(HttpResponse);
};

}  // namespace devtools_goma

#endif  // DEVTOOLS_GOMA_CLIENT_HTTP_H_
