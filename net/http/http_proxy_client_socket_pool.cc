// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_proxy_client_socket_pool.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_network_session.h"
#include "net/http/http_proxy_client_socket_wrapper.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/transport_client_socket_pool.h"
#include "net/socket/transport_connect_job.h"
#include "net/spdy/spdy_proxy_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_stream.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "url/gurl.h"

namespace net {

namespace {

// HttpProxyConnectJobs will time out after this many seconds.  Note this is in
// addition to the timeout for the transport socket.
#if defined(OS_ANDROID) || defined(OS_IOS)
static const int kHttpProxyConnectJobTimeoutInSeconds = 10;
#else
static const int kHttpProxyConnectJobTimeoutInSeconds = 30;
#endif

class HttpProxyTimeoutExperiments {
 public:
  HttpProxyTimeoutExperiments() { Init(); }

  ~HttpProxyTimeoutExperiments() = default;

  void Init() {
#if defined(OS_ANDROID) || defined(OS_IOS)
    min_proxy_connection_timeout_ = base::TimeDelta::FromSeconds(
        GetInt32Param("min_proxy_connection_timeout_seconds", 8));
    max_proxy_connection_timeout_ = base::TimeDelta::FromSeconds(
        GetInt32Param("max_proxy_connection_timeout_seconds", 30));
#else
    min_proxy_connection_timeout_ = base::TimeDelta::FromSeconds(
        GetInt32Param("min_proxy_connection_timeout_seconds", 30));
    max_proxy_connection_timeout_ = base::TimeDelta::FromSeconds(
        GetInt32Param("max_proxy_connection_timeout_seconds", 60));
#endif
    ssl_http_rtt_multiplier_ = GetInt32Param("ssl_http_rtt_multiplier", 10);
    non_ssl_http_rtt_multiplier_ =
        GetInt32Param("non_ssl_http_rtt_multiplier", 5);

    DCHECK_LT(0, ssl_http_rtt_multiplier_);
    DCHECK_LT(0, non_ssl_http_rtt_multiplier_);
    DCHECK_LE(base::TimeDelta(), min_proxy_connection_timeout_);
    DCHECK_LE(base::TimeDelta(), max_proxy_connection_timeout_);
    DCHECK_LE(min_proxy_connection_timeout_, max_proxy_connection_timeout_);
  }

  base::TimeDelta min_proxy_connection_timeout() const {
    return min_proxy_connection_timeout_;
  }
  base::TimeDelta max_proxy_connection_timeout() const {
    return max_proxy_connection_timeout_;
  }
  int32_t ssl_http_rtt_multiplier() const { return ssl_http_rtt_multiplier_; }
  int32_t non_ssl_http_rtt_multiplier() const {
    return non_ssl_http_rtt_multiplier_;
  }

 private:
  // Return the value of the parameter |param_name| for the field trial
  // "NetAdaptiveProxyConnectionTimeout". If the value of the parameter is
  // unavailable, then |default_value| is available.
  static int32_t GetInt32Param(const std::string& param_name,
                               int32_t default_value) {
    int32_t param;
    if (!base::StringToInt(base::GetFieldTrialParamValue(
                               "NetAdaptiveProxyConnectionTimeout", param_name),
                           &param)) {
      return default_value;
    }
    return param;
  }

  // For secure proxies, the connection timeout is set to
  // |ssl_http_rtt_multiplier_| times the HTTP RTT estimate. For insecure
  // proxies, the connection timeout is set to |non_ssl_http_rtt_multiplier_|
  // times the HTTP RTT estimate. In either case, the connection timeout
  // is clamped to be between |min_proxy_connection_timeout_| and
  // |max_proxy_connection_timeout_|.
  base::TimeDelta min_proxy_connection_timeout_;
  base::TimeDelta max_proxy_connection_timeout_;
  int32_t ssl_http_rtt_multiplier_;
  int32_t non_ssl_http_rtt_multiplier_;
};

HttpProxyTimeoutExperiments* GetProxyTimeoutExperiments() {
  static base::NoDestructor<HttpProxyTimeoutExperiments>
      proxy_timeout_experiments;
  return proxy_timeout_experiments.get();
}

}  // namespace

HttpProxySocketParams::HttpProxySocketParams(
    const scoped_refptr<TransportSocketParams>& transport_params,
    const scoped_refptr<SSLSocketParams>& ssl_params,
    quic::QuicTransportVersion quic_version,
    const std::string& user_agent,
    const HostPortPair& endpoint,
    HttpAuthCache* http_auth_cache,
    HttpAuthHandlerFactory* http_auth_handler_factory,
    SpdySessionPool* spdy_session_pool,
    QuicStreamFactory* quic_stream_factory,
    bool is_trusted_proxy,
    bool tunnel,
    const NetworkTrafficAnnotationTag traffic_annotation)
    : transport_params_(transport_params),
      ssl_params_(ssl_params),
      quic_version_(quic_version),
      spdy_session_pool_(spdy_session_pool),
      quic_stream_factory_(quic_stream_factory),
      user_agent_(user_agent),
      endpoint_(endpoint),
      http_auth_cache_(tunnel ? http_auth_cache : NULL),
      http_auth_handler_factory_(tunnel ? http_auth_handler_factory : NULL),
      is_trusted_proxy_(is_trusted_proxy),
      tunnel_(tunnel),
      traffic_annotation_(traffic_annotation) {
  // If doing a QUIC proxy, |quic_version| must not be
  // quic::QUIC_VERSION_UNSUPPORTED, and |ssl_params| must be valid while
  // |transport_params| is null. Otherwise, |quic_version| must be
  // quic::QUIC_VERSION_UNSUPPORTED, and exactly one of |transport_params| or
  // |ssl_params| must be set.
  DCHECK(quic_version_ == quic::QUIC_VERSION_UNSUPPORTED
             ? (bool)transport_params != (bool)ssl_params
             : !transport_params && ssl_params);
  // Exactly one of |transport_params_| and |ssl_params_| must be non-null.
  DCHECK(transport_params_ || ssl_params_);
  DCHECK(!transport_params_ || !ssl_params_);
}

HttpProxySocketParams::~HttpProxySocketParams() = default;

HttpProxyConnectJob::HttpProxyConnectJob(
    const std::string& group_name,
    RequestPriority priority,
    const SocketTag& socket_tag,
    ClientSocketPool::RespectLimits respect_limits,
    const scoped_refptr<HttpProxySocketParams>& params,
    ProxyDelegate* proxy_delegate,
    TransportClientSocketPool* transport_pool,
    TransportClientSocketPool* ssl_pool,
    NetworkQualityEstimator* network_quality_estimator,
    Delegate* delegate,
    NetLog* net_log)
    : ConnectJob(
          group_name,
          base::TimeDelta() /* The socket takes care of timeouts */,
          priority,
          socket_tag,
          respect_limits == ClientSocketPool::RespectLimits::ENABLED,
          delegate,
          NetLogWithSource::Make(net_log,
                                 NetLogSourceType::HTTP_PROXY_CONNECT_JOB)),
      client_socket_(new HttpProxyClientSocketWrapper(
          group_name,
          priority,
          socket_tag,
          respect_limits,
          ConnectionTimeout(*params, network_quality_estimator),
          base::TimeDelta::FromSeconds(kHttpProxyConnectJobTimeoutInSeconds),
          transport_pool,
          ssl_pool,
          params->transport_params(),
          params->ssl_params(),
          params->quic_version(),
          params->user_agent(),
          params->endpoint(),
          params->http_auth_cache(),
          params->http_auth_handler_factory(),
          params->spdy_session_pool(),
          params->quic_stream_factory(),
          params->is_trusted_proxy(),
          params->tunnel(),
          proxy_delegate,
          params->traffic_annotation(),
          this->net_log())) {}

HttpProxyConnectJob::~HttpProxyConnectJob() = default;

LoadState HttpProxyConnectJob::GetLoadState() const {
  return client_socket_->GetConnectLoadState();
}

bool HttpProxyConnectJob::HasEstablishedConnection() const {
  // TODO(mmenke): Implement this, as nested pools are removed.
  return false;
}

void HttpProxyConnectJob::GetAdditionalErrorState(ClientSocketHandle * handle) {
  if (error_response_info_) {
    handle->set_ssl_error_response_info(*error_response_info_);
    handle->set_is_ssl_error(true);
  }
}

base::TimeDelta HttpProxyConnectJob::ConnectionTimeout(
    const HttpProxySocketParams& params,
    const NetworkQualityEstimator* network_quality_estimator) {
  bool is_https = params.ssl_params() != nullptr;
  // HTTP proxy connections can't be on top of proxy connections.
  DCHECK(!is_https ||
         params.ssl_params()->GetConnectionType() == SSLSocketParams::DIRECT);

  if (network_quality_estimator) {
    base::Optional<base::TimeDelta> http_rtt_estimate =
        network_quality_estimator->GetHttpRTT();
    if (http_rtt_estimate) {
      int32_t multiplier =
          is_https
              ? GetProxyTimeoutExperiments()->ssl_http_rtt_multiplier()
              : GetProxyTimeoutExperiments()->non_ssl_http_rtt_multiplier();
      base::TimeDelta timeout = base::TimeDelta::FromMicroseconds(
          multiplier * http_rtt_estimate.value().InMicroseconds());
      // Ensure that connection timeout is between
      // |min_proxy_connection_timeout_| and |max_proxy_connection_timeout_|.
      return base::ClampToRange(
          timeout, GetProxyTimeoutExperiments()->min_proxy_connection_timeout(),
          GetProxyTimeoutExperiments()->max_proxy_connection_timeout());
    }
  }

  // Return the default proxy connection timeout.
  base::TimeDelta nested_job_timeout;
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  if (is_https) {
    nested_job_timeout = SSLConnectJob::ConnectionTimeout(
        *params.ssl_params(), network_quality_estimator);
  } else {
    nested_job_timeout = TransportConnectJob::ConnectionTimeout();
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

  return nested_job_timeout +
         base::TimeDelta::FromSeconds(kHttpProxyConnectJobTimeoutInSeconds);
}

void HttpProxyConnectJob::UpdateFieldTrialParametersForTesting() {
  GetProxyTimeoutExperiments()->Init();
}

int HttpProxyConnectJob::ConnectInternal() {
  int result = client_socket_->Connect(base::Bind(
      &HttpProxyConnectJob::OnConnectComplete, base::Unretained(this)));
  return HandleConnectResult(result);
}

void HttpProxyConnectJob::ChangePriorityInternal(RequestPriority priority) {
  if (client_socket_)
    client_socket_->SetPriority(priority);
}

void HttpProxyConnectJob::OnConnectComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);
  result = HandleConnectResult(result);
  NotifyDelegateOfCompletion(result);
  // |this| will have been deleted at this point.
}

int HttpProxyConnectJob::HandleConnectResult(int result) {
  if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED)
    error_response_info_ = client_socket_->GetAdditionalErrorState();

  if (result == OK || result == ERR_PROXY_AUTH_REQUESTED ||
      result == ERR_HTTPS_PROXY_TUNNEL_RESPONSE) {
    SetSocket(std::move(client_socket_));
  }
  return result;
}

HttpProxyClientSocketPool::HttpProxyConnectJobFactory::
    HttpProxyConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        TransportClientSocketPool* ssl_pool,
        ProxyDelegate* proxy_delegate,
        NetworkQualityEstimator* network_quality_estimator,
        NetLog* net_log)
    : transport_pool_(transport_pool),
      ssl_pool_(ssl_pool),
      proxy_delegate_(proxy_delegate),
      network_quality_estimator_(network_quality_estimator),
      net_log_(net_log) {}

std::unique_ptr<ConnectJob>
HttpProxyClientSocketPool::HttpProxyConnectJobFactory::NewConnectJob(
    const std::string& group_name,
    const PoolBase::Request& request,
    ConnectJob::Delegate* delegate) const {
  return std::make_unique<HttpProxyConnectJob>(
      group_name, request.priority(), request.socket_tag(),
      request.respect_limits(), request.params(), proxy_delegate_,
      transport_pool_, ssl_pool_, network_quality_estimator_, delegate,
      net_log_);
}

HttpProxyClientSocketPool::HttpProxyClientSocketPool(
    int max_sockets,
    int max_sockets_per_group,
    TransportClientSocketPool* transport_pool,
    TransportClientSocketPool* ssl_pool,
    ProxyDelegate* proxy_delegate,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log)
    : transport_pool_(transport_pool),
      ssl_pool_(ssl_pool),
      base_(this,
            max_sockets,
            max_sockets_per_group,
            ClientSocketPool::unused_idle_socket_timeout(),
            ClientSocketPool::used_idle_socket_timeout(),
            new HttpProxyConnectJobFactory(transport_pool,
                                           ssl_pool,
                                           proxy_delegate,
                                           network_quality_estimator,
                                           net_log)) {
  // We should always have a |transport_pool_| except in unit tests.
  if (transport_pool_)
    base_.AddLowerLayeredPool(transport_pool_);
  if (ssl_pool_)
    base_.AddLowerLayeredPool(ssl_pool_);
}

HttpProxyClientSocketPool::~HttpProxyClientSocketPool() = default;

int HttpProxyClientSocketPool::RequestSocket(const std::string& group_name,
                                             const void* socket_params,
                                             RequestPriority priority,
                                             const SocketTag& socket_tag,
                                             RespectLimits respect_limits,
                                             ClientSocketHandle* handle,
                                             CompletionOnceCallback callback,
                                             const NetLogWithSource& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_socket_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(socket_params);

  return base_.RequestSocket(group_name, *casted_socket_params, priority,
                             socket_tag, respect_limits, handle,
                             std::move(callback), net_log);
}

void HttpProxyClientSocketPool::RequestSockets(
    const std::string& group_name,
    const void* params,
    int num_sockets,
    const NetLogWithSource& net_log) {
  const scoped_refptr<HttpProxySocketParams>* casted_params =
      static_cast<const scoped_refptr<HttpProxySocketParams>*>(params);

  base_.RequestSockets(group_name, *casted_params, num_sockets, net_log);
}

void HttpProxyClientSocketPool::CancelRequest(
    const std::string& group_name,
    ClientSocketHandle* handle) {
  base_.CancelRequest(group_name, handle);
}

void HttpProxyClientSocketPool::SetPriority(const std::string& group_name,
                                            ClientSocketHandle* handle,
                                            RequestPriority priority) {
  base_.SetPriority(group_name, handle, priority);
}

void HttpProxyClientSocketPool::ReleaseSocket(
    const std::string& group_name,
    std::unique_ptr<StreamSocket> socket,
    int id) {
  base_.ReleaseSocket(group_name, std::move(socket), id);
}

void HttpProxyClientSocketPool::FlushWithError(int error) {
  base_.FlushWithError(error);
}

void HttpProxyClientSocketPool::CloseIdleSockets() {
  base_.CloseIdleSockets();
}

void HttpProxyClientSocketPool::CloseIdleSocketsInGroup(
    const std::string& group_name) {
  base_.CloseIdleSocketsInGroup(group_name);
}

int HttpProxyClientSocketPool::IdleSocketCount() const {
  return base_.idle_socket_count();
}

int HttpProxyClientSocketPool::IdleSocketCountInGroup(
    const std::string& group_name) const {
  return base_.IdleSocketCountInGroup(group_name);
}

LoadState HttpProxyClientSocketPool::GetLoadState(
    const std::string& group_name, const ClientSocketHandle* handle) const {
  return base_.GetLoadState(group_name, handle);
}

std::unique_ptr<base::DictionaryValue>
HttpProxyClientSocketPool::GetInfoAsValue(const std::string& name,
                                          const std::string& type,
                                          bool include_nested_pools) const {
  std::unique_ptr<base::DictionaryValue> dict(base_.GetInfoAsValue(name, type));
  if (include_nested_pools) {
    auto list = std::make_unique<base::ListValue>();
    if (transport_pool_) {
      list->Append(transport_pool_->GetInfoAsValue("transport_socket_pool",
                                                   "transport_socket_pool",
                                                   true));
    }
    if (ssl_pool_) {
      list->Append(ssl_pool_->GetInfoAsValue("ssl_socket_pool",
                                             "ssl_socket_pool",
                                             true));
    }
    dict->Set("nested_pools", std::move(list));
  }
  return dict;
}

bool HttpProxyClientSocketPool::IsStalled() const {
  return base_.IsStalled();
}

void HttpProxyClientSocketPool::AddHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.AddHigherLayeredPool(higher_pool);
}

void HttpProxyClientSocketPool::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  base_.RemoveHigherLayeredPool(higher_pool);
}

bool HttpProxyClientSocketPool::CloseOneIdleConnection() {
  if (base_.CloseOneIdleSocket())
    return true;
  return base_.CloseOneIdleConnectionInHigherLayeredPool();
}

}  // namespace net
