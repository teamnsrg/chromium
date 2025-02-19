// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/appcache/appcache_frontend_impl.h"

#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/appcache/web_application_cache_host_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

using blink::WebApplicationCacheHost;

namespace content {

// Inline helper to keep the lines shorter and unwrapped.
inline WebApplicationCacheHostImpl* GetHost(int id) {
  return WebApplicationCacheHostImpl::FromId(id);
}

AppCacheFrontendImpl::AppCacheFrontendImpl() : binding_(this) {}
AppCacheFrontendImpl::~AppCacheFrontendImpl() = default;

void AppCacheFrontendImpl::Bind(blink::mojom::AppCacheFrontendRequest request) {
  binding_.Bind(std::move(request));
}

blink::mojom::AppCacheBackend* AppCacheFrontendImpl::backend_proxy() {
  if (!backend_ptr_) {
    RenderThread::Get()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, mojo::MakeRequest(&backend_ptr_));
  }
  return backend_ptr_.get();
}

void AppCacheFrontendImpl::CacheSelected(int32_t host_id,
                                         blink::mojom::AppCacheInfoPtr info) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnCacheSelected(*info);
}

void AppCacheFrontendImpl::StatusChanged(const std::vector<int32_t>& host_ids,
                                         blink::mojom::AppCacheStatus status) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnStatusChanged(status);
  }
}

void AppCacheFrontendImpl::EventRaised(const std::vector<int32_t>& host_ids,
                                       blink::mojom::AppCacheEventID event_id) {
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_PROGRESS_EVENT);  // See OnProgressEventRaised.
  DCHECK_NE(event_id,
            blink::mojom::AppCacheEventID::
                APPCACHE_ERROR_EVENT);  // See OnErrorEventRaised.
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnEventRaised(event_id);
  }
}

void AppCacheFrontendImpl::ProgressEventRaised(
    const std::vector<int32_t>& host_ids,
    const GURL& url,
    int32_t num_total,
    int32_t num_complete) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnProgressEventRaised(url, num_total, num_complete);
  }
}

void AppCacheFrontendImpl::ErrorEventRaised(
    const std::vector<int32_t>& host_ids,
    blink::mojom::AppCacheErrorDetailsPtr details) {
  for (auto i = host_ids.begin(); i != host_ids.end(); ++i) {
    WebApplicationCacheHostImpl* host = GetHost(*i);
    if (host)
      host->OnErrorEventRaised(*details);
  }
}

void AppCacheFrontendImpl::LogMessage(
    int32_t host_id,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::string& message) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->OnLogMessage(log_level, message);
}

void AppCacheFrontendImpl::SetSubresourceFactory(
    int32_t host_id,
    network::mojom::URLLoaderFactoryPtr url_loader_factory) {
  WebApplicationCacheHostImpl* host = GetHost(host_id);
  if (host)
    host->SetSubresourceFactory(std::move(url_loader_factory));
}

}  // namespace content
