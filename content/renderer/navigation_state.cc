// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_state.h"

#include <utility>

#include "content/common/frame_messages.h"
#include "content/public/common/navigation_policy.h"
#include "content/renderer/internal_document_state_data.h"
#include "third_party/blink/public/web/commit_result.mojom.h"

namespace content {

NavigationState::~NavigationState() {
  RunCommitNavigationCallback(blink::mojom::CommitResult::Aborted);
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateBrowserInitiated(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    base::TimeTicks time_commit_requested,
    mojom::FrameNavigationControl::CommitNavigationCallback callback,
    mojom::NavigationClient::CommitNavigationCallback
        per_navigation_mojo_interface_callback,
    std::unique_ptr<NavigationClient> navigation_client) {
  return base::WrapUnique(new NavigationState(
      common_params, commit_params, time_commit_requested, false,
      std::move(callback), std::move(per_navigation_mojo_interface_callback),
      std::move(navigation_client)));
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateContentInitiated() {
  return base::WrapUnique(new NavigationState(
      CommonNavigationParams(), CommitNavigationParams(), base::TimeTicks(),
      true, content::mojom::FrameNavigationControl::CommitNavigationCallback(),
      content::mojom::NavigationClient::CommitNavigationCallback(), nullptr));
}

// static
NavigationState* NavigationState::FromDocumentLoader(
    blink::WebDocumentLoader* document_loader) {
  return InternalDocumentStateData::FromDocumentLoader(document_loader)
      ->navigation_state();
}

bool NavigationState::WasWithinSameDocument() {
  return was_within_same_document_;
}

bool NavigationState::IsContentInitiated() {
  return is_content_initiated_;
}

void NavigationState::RunCommitNavigationCallback(
    blink::mojom::CommitResult result) {
  if (commit_callback_)
    std::move(commit_callback_).Run(result);
}

void NavigationState::RunPerNavigationInterfaceCommitNavigationCallback(
    std::unique_ptr<::FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  DCHECK(IsPerNavigationMojoInterfaceEnabled());
  if (per_navigation_mojo_interface_commit_callback_)
    std::move(per_navigation_mojo_interface_commit_callback_)
        .Run(std::move(params), std::move(interface_params));
  navigation_client_.reset();
}

NavigationState::NavigationState(
    const CommonNavigationParams& common_params,
    const CommitNavigationParams& commit_params,
    base::TimeTicks time_commit_requested,
    bool is_content_initiated,
    mojom::FrameNavigationControl::CommitNavigationCallback callback,
    mojom::NavigationClient::CommitNavigationCallback
        per_navigation_mojo_interface_commit_callback,
    std::unique_ptr<NavigationClient> navigation_client)
    : request_committed_(false),
      was_within_same_document_(false),
      is_content_initiated_(is_content_initiated),
      common_params_(common_params),
      commit_params_(commit_params),
      time_commit_requested_(time_commit_requested),
      navigation_client_(std::move(navigation_client)),
      commit_callback_(std::move(callback)),
      per_navigation_mojo_interface_commit_callback_(
          std::move(per_navigation_mojo_interface_commit_callback)) {}
}  // namespace content
