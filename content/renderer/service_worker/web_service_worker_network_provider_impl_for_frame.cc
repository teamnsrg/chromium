// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/web_service_worker_network_provider_impl_for_frame.h"

#include <utility>

#include "content/common/navigation_params.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/origin_util.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

namespace {
// Returns whether it's possible for a document whose frame is a descendant of
// |frame| to be a secure context, not considering scheme exceptions (since any
// document can be a secure context if it has a scheme exception). See
// Document::isSecureContextImpl for more details.
bool IsFrameSecure(blink::WebFrame* frame) {
  while (frame) {
    if (!frame->GetSecurityOrigin().IsPotentiallyTrustworthy())
      return false;
    frame = frame->Parent();
  }
  return true;
}
}  // namespace

class WebServiceWorkerNetworkProviderImplForFrame::NewDocumentObserver
    : public RenderFrameObserver {
 public:
  NewDocumentObserver(WebServiceWorkerNetworkProviderImplForFrame* owner,
                      RenderFrameImpl* frame)
      : RenderFrameObserver(frame), owner_(owner) {}

  void DidCreateNewDocument() override {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    blink::WebDocumentLoader* web_loader =
        render_frame()->GetWebFrame()->GetDocumentLoader();
    DCHECK_EQ(owner_, web_loader->GetServiceWorkerNetworkProvider());

    if (web_frame->GetSecurityOrigin().IsOpaque()) {
      // At navigation commit we thought the document was eligible to use
      // service workers so created the network provider, but it turns out it is
      // not eligible because it is CSP sandboxed.
      web_loader->SetServiceWorkerNetworkProvider(
          WebServiceWorkerNetworkProviderImplForFrame::CreateInvalidInstance());
      // |this| and its owner are destroyed.
      return;
    }

    owner_->NotifyExecutionReady();
  }

  void OnDestruct() override {
    // Deletes |this|.
    owner_->observer_.reset();
  }

 private:
  WebServiceWorkerNetworkProviderImplForFrame* owner_;
};

// static
std::unique_ptr<WebServiceWorkerNetworkProviderImplForFrame>
WebServiceWorkerNetworkProviderImplForFrame::Create(
    RenderFrameImpl* frame,
    const CommitNavigationParams* commit_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory) {
  blink::WebLocalFrame* web_frame = frame->GetWebFrame();
  // Determine if a provider should be created and properly initialized for the
  // navigation. A default provider will always be created since it is expected
  // in a certain number of places, however it will have an invalid id.
  bool should_create_provider = false;
  int provider_id = kInvalidServiceWorkerProviderId;
  if (commit_params) {
    should_create_provider = commit_params->should_create_service_worker;
    provider_id = commit_params->service_worker_provider_id;
  } else {
    // It'd be convenient to check web_frame->GetSecurityOrigin().IsOpaque()
    // here instead of just looking at the sandbox flags, but
    // GetSecurityOrigin() crashes because the frame does not yet have a
    // security context.
    should_create_provider =
        ((web_frame->EffectiveSandboxFlags() &
          blink::WebSandboxFlags::kOrigin) != blink::WebSandboxFlags::kOrigin);
  }

  // If we shouldn't create a real provider, return one with an invalid id.
  if (!should_create_provider) {
    return CreateInvalidInstance();
  }

  // Otherwise, create the provider.

  // Ideally Document::IsSecureContext would be called here, but the document is
  // not created yet, and due to redirects the URL may change. So pass
  // is_parent_frame_secure to the browser process, so it can determine the
  // context security when deciding whether to allow a service worker to control
  // the document.
  const bool is_parent_frame_secure = IsFrameSecure(web_frame->Parent());

  // If the browser process did not assign a provider id already, assign one
  // now (see class comments for content::ServiceWorkerProviderHost).
  DCHECK(ServiceWorkerUtils::IsBrowserAssignedProviderId(provider_id) ||
         provider_id == kInvalidServiceWorkerProviderId);
  if (provider_id == kInvalidServiceWorkerProviderId)
    provider_id = ServiceWorkerProviderContext::GetNextId();

  auto provider =
      base::WrapUnique(new WebServiceWorkerNetworkProviderImplForFrame(frame));

  auto host_info = blink::mojom::ServiceWorkerProviderHostInfo::New(
      provider_id, frame->GetRoutingID(),
      blink::mojom::ServiceWorkerProviderType::kForWindow,
      is_parent_frame_secure, nullptr /* host_request */,
      nullptr /* client_ptr_info */);
  blink::mojom::ServiceWorkerContainerAssociatedRequest client_request =
      mojo::MakeRequest(&host_info->client_ptr_info);
  blink::mojom::ServiceWorkerContainerHostAssociatedPtrInfo host_ptr_info;
  host_info->host_request = mojo::MakeRequest(&host_ptr_info);

  provider->context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
      provider_id, blink::mojom::ServiceWorkerProviderType::kForWindow,
      std::move(client_request), std::move(host_ptr_info),
      std::move(controller_info), std::move(fallback_loader_factory));

  if (ChildThreadImpl::current()) {
    ChildThreadImpl::current()->channel()->GetRemoteAssociatedInterface(
        &provider->dispatcher_host_);
    provider->dispatcher_host_->OnProviderCreated(std::move(host_info));
  } else {
    // current() may be null in tests. Silently drop messages sent over
    // ServiceWorkerContainerHost since we couldn't set it up correctly due to
    // this test limitation. This way we don't crash when the associated
    // interface ptr is used.
    //
    // TODO(falken): Just give ServiceWorkerProviderContext a null interface ptr
    // and make the callsites deal with it. They are supposed to anyway because
    // OnNetworkProviderDestroyed() can reset the ptr to null at any time.
    mojo::AssociateWithDisconnectedPipe(host_info->host_request.PassHandle());
  }
  return provider;
}

// static
std::unique_ptr<WebServiceWorkerNetworkProviderImplForFrame>
WebServiceWorkerNetworkProviderImplForFrame::CreateInvalidInstance() {
  return base::WrapUnique(
      new WebServiceWorkerNetworkProviderImplForFrame(nullptr));
}

WebServiceWorkerNetworkProviderImplForFrame::
    WebServiceWorkerNetworkProviderImplForFrame(RenderFrameImpl* frame) {
  if (frame)
    observer_ = std::make_unique<NewDocumentObserver>(this, frame);
}

WebServiceWorkerNetworkProviderImplForFrame::
    ~WebServiceWorkerNetworkProviderImplForFrame() {
  if (context())
    context()->OnNetworkProviderDestroyed();
}

void WebServiceWorkerNetworkProviderImplForFrame::WillSendRequest(
    blink::WebURLRequest& request) {
  if (!request.GetExtraData())
    request.SetExtraData(std::make_unique<RequestExtraData>());
  auto* extra_data = static_cast<RequestExtraData*>(request.GetExtraData());
  extra_data->set_service_worker_provider_id(provider_id());

  // If the provider does not have a controller at this point, the renderer
  // expects the request to never be handled by a service worker, so call
  // SetSkipServiceWorker() with true to skip service workers here. Otherwise,
  // a service worker that is in the process of becoming the controller (i.e.,
  // via claim()) on the browser-side could handle the request and break the
  // assumptions of the renderer.
  if (request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kTopLevel &&
      request.GetFrameType() !=
          network::mojom::RequestContextFrameType::kNested &&
      IsControlledByServiceWorker() ==
          blink::mojom::ControllerServiceWorkerMode::kNoController) {
    request.SetSkipServiceWorker(true);
  }

  // Inject this frame's fetch window id into the request. This is really only
  // needed for subresource requests in S13nServiceWorker. For main resource
  // requests or non-S13nSW case, the browser process sets the id on the
  // request when dispatching the fetch event to the service worker. But it
  // doesn't hurt to set it always.
  if (context())
    request.SetFetchWindowId(context()->fetch_request_window_id());
}

blink::mojom::ControllerServiceWorkerMode
WebServiceWorkerNetworkProviderImplForFrame::IsControlledByServiceWorker() {
  if (!context())
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return context()->IsControlledByServiceWorker();
}

int64_t
WebServiceWorkerNetworkProviderImplForFrame::ControllerServiceWorkerID() {
  if (!context())
    return blink::mojom::kInvalidServiceWorkerVersionId;
  return context()->GetControllerVersionId();
}

std::unique_ptr<blink::WebURLLoader>
WebServiceWorkerNetworkProviderImplForFrame::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  // RenderThreadImpl is nullptr in some tests.
  if (!RenderThreadImpl::current())
    return nullptr;

  // S13nServiceWorker:
  // We only install our own URLLoader if Servicification is enabled.
  if (!blink::ServiceWorkerUtils::IsServicificationEnabled())
    return nullptr;

  // We need SubresourceLoaderFactory populated in order to create our own
  // URLLoader for subresource loading.
  if (!context() || !context()->GetSubresourceLoaderFactory())
    return nullptr;

  // If the URL is not http(s) or otherwise whitelisted, do not intercept the
  // request. Schemes like 'blob' and 'file' are not eligible to be intercepted
  // by service workers.
  // TODO(falken): Let ServiceWorkerSubresourceLoaderFactory handle the request
  // and move this check there (i.e., for such URLs, it should use its fallback
  // factory).
  const GURL gurl(request.Url());
  if (!gurl.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(gurl))
    return nullptr;

  // If GetSkipServiceWorker() returns true, do not intercept the request.
  if (request.GetSkipServiceWorker())
    return nullptr;

  // Create our own SubresourceLoader to route the request to the controller
  // ServiceWorker.
  // TODO(crbug.com/796425): Temporarily wrap the raw mojom::URLLoaderFactory
  // pointer into SharedURLLoaderFactory.
  return std::make_unique<WebURLLoaderImpl>(
      RenderThreadImpl::current()->resource_dispatcher(),
      std::move(task_runner_handle),
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          context()->GetSubresourceLoaderFactory()));
}

void WebServiceWorkerNetworkProviderImplForFrame::DispatchNetworkQuiet() {
  if (!context())
    return;
  context()->DispatchNetworkQuiet();
}

int WebServiceWorkerNetworkProviderImplForFrame::provider_id() const {
  if (!context_)
    return kInvalidServiceWorkerProviderId;
  return context_->provider_id();
}

void WebServiceWorkerNetworkProviderImplForFrame::NotifyExecutionReady() {
  if (context())
    context()->NotifyExecutionReady();
}

}  // namespace content
