// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_helper.h"

#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/performance_manager/page_resource_coordinator.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/render_process_user_data.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace resource_coordinator {

ResourceCoordinatorTabHelper::ResourceCoordinatorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      performance_manager_(
          performance_manager::PerformanceManager::GetInstance()) {
  TabLoadTracker::Get()->StartTracking(web_contents);
  if (performance_manager_) {
    page_resource_coordinator_ =
        std::make_unique<performance_manager::PageResourceCoordinator>(
            performance_manager_);

    // Make sure to set the visibility property when we create
    // |page_resource_coordinator_|.
    const bool is_visible =
        web_contents->GetVisibility() != content::Visibility::HIDDEN;
    page_resource_coordinator_->SetVisibility(is_visible);

    if (auto* page_signal_receiver = GetPageSignalReceiver()) {
      // Gets CoordinationUnitID for this WebContents and adds it to
      // PageSignalReceiver.
      page_signal_receiver->AssociateCoordinationUnitIDWithWebContents(
          page_resource_coordinator_->id(), web_contents);
    }

    if (memory_instrumentation::MemoryInstrumentation::GetInstance()) {
      auto* rc_parts = g_browser_process->resource_coordinator_parts();
      DCHECK(rc_parts);
      rc_parts->tab_memory_metrics_reporter()->StartReporting(
          TabLoadTracker::Get());
    }
  }

#if !defined(OS_ANDROID)
  // Don't create the LocalSiteCharacteristicsWebContentsObserver for this tab
  // we don't have a page signal receiver as the data that this observer
  // records depend on it.
  if (base::FeatureList::IsEnabled(features::kSiteCharacteristicsDatabase) &&
      GetPageSignalReceiver()) {
    local_site_characteristics_wc_observer_ =
        std::make_unique<LocalSiteCharacteristicsWebContentsObserver>(
            web_contents);
  }
#endif

  // Dispatch creation notifications for any pre-existing frames.
  // This seems to occur only in tests, but dealing with this allows asserting
  // a strong invariant on the frames_ collection.
  std::vector<content::RenderFrameHost*> existing_frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : existing_frames) {
    // Only send notifications for live frames, the non-live ones will generate
    // creation notifications when animated.
    if (frame->IsRenderFrameLive())
      RenderFrameCreated(frame);
  }
}

ResourceCoordinatorTabHelper::~ResourceCoordinatorTabHelper() = default;

void ResourceCoordinatorTabHelper::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_NE(nullptr, render_frame_host);
  // This must not exist in the map yet.
  DCHECK(!base::ContainsKey(frames_, render_frame_host));

  if (!performance_manager_)
    return;

  std::unique_ptr<performance_manager::FrameResourceCoordinator> frame =
      std::make_unique<performance_manager::FrameResourceCoordinator>(
          performance_manager_);
  content::RenderFrameHost* parent = render_frame_host->GetParent();
  if (parent) {
    DCHECK(base::ContainsKey(frames_, parent));
    auto& parent_frame_node = frames_[parent];
    parent_frame_node->AddChildFrame(*frame.get());
  }

  RenderProcessUserData* user_data =
      RenderProcessUserData::GetForRenderProcessHost(
          render_frame_host->GetProcess());
  // In unittests the user data isn't populated as the relevant main parts
  // is not in play.
  // TODO(siggi): Figure out how to assert on this when the main parts are
  //     registered with the content browser client.
  if (user_data)
    frame->SetProcess(*user_data->process_resource_coordinator());

  frames_[render_frame_host] = std::move(frame);
}

void ResourceCoordinatorTabHelper::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!performance_manager_)
    return;

  // TODO(siggi): Ideally this would DCHECK that the deleted render frame host
  //     is known, e.g. that there was a creation notification for it. This is
  //     however not always the case. Notably these two unit_tests:
  //       - TabsApiUnitTest.TabsGoForwardAndBack
  //       - TabsApiUnitTest.TabsGoForwardAndBackWithoutTabId
  //     end up issuing deletion notifications for render frame hosts never seen
  //     before. It appears that the RenderFrameHostManager keeps a queue of
  //     pending deletions. If a frame is already in this queue at the time
  //     this tab helper is attached to a WebContents, the eventual deletion
  //     notification will be singular.
  // DCHECK(base::ContainsKey(frames_, render_frame_host));
  frames_.erase(render_frame_host);
}

void ResourceCoordinatorTabHelper::DidStartLoading() {
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetIsLoading(true);
  TabLoadTracker::Get()->DidStartLoading(web_contents());
}

void ResourceCoordinatorTabHelper::DidReceiveResponse() {
  TabLoadTracker::Get()->DidReceiveResponse(web_contents());
}

void ResourceCoordinatorTabHelper::DidStopLoading() {
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetIsLoading(false);
  TabLoadTracker::Get()->DidStopLoading(web_contents());
}

void ResourceCoordinatorTabHelper::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  TabLoadTracker::Get()->DidFailLoad(web_contents());
}

void ResourceCoordinatorTabHelper::RenderProcessGone(
    base::TerminationStatus status) {
  // TODO(siggi): Looks like this can be acquired in a more timely manner from
  //    the RenderProcessHostObserver.
  TabLoadTracker::Get()->RenderProcessGone(web_contents(), status);
}

void ResourceCoordinatorTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (page_resource_coordinator_) {
    // TODO(fdoray): An OCCLUDED tab should not be considered visible.
    const bool is_visible = visibility != content::Visibility::HIDDEN;
    page_resource_coordinator_->SetVisibility(is_visible);
  }
}

void ResourceCoordinatorTabHelper::WebContentsDestroyed() {
  if (page_resource_coordinator_) {
    if (auto* page_signal_receiver = GetPageSignalReceiver()) {
      // Gets CoordinationUnitID for this WebContents and removes it from
      // PageSignalReceiver.
      page_signal_receiver->RemoveCoordinationUnitID(
          page_resource_coordinator_->id());
    }
  }
  TabLoadTracker::Get()->StopTracking(web_contents());
}

void ResourceCoordinatorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (page_resource_coordinator_) {
    // Grab the current time up front, as this is as close as we'll get to the
    // original commit time.
    base::TimeTicks navigation_committed_time = base::TimeTicks::Now();

    content::RenderFrameHost* render_frame_host =
        navigation_handle->GetRenderFrameHost();
    // Make sure the hierarchical structure is constructed before sending signal
    // to Resource Coordinator.
    // TODO(siggi): Ideally this would be a DCHECK, but it seems it's possible
    //     to get a DidFinishNavigation notification for a deleted frame when
    //     with the network service.
    auto it = frames_.find(render_frame_host);
    if (it != frames_.end()) {
      // TODO(siggi): See whether this can be done in RenderFrameCreated.
      page_resource_coordinator_->AddFrame(*(it->second));
    }

    if (navigation_handle->IsInMainFrame()) {
      if (auto* page_signal_receiver = GetPageSignalReceiver()) {
        // Update the last observed navigation ID for this WebContents.
        page_signal_receiver->SetNavigationID(
            web_contents(), navigation_handle->GetNavigationId());
      }

      UpdateUkmRecorder(navigation_handle->GetNavigationId());
      ResetFlag();
      page_resource_coordinator_->OnMainFrameNavigationCommitted(
          navigation_committed_time, navigation_handle->GetNavigationId(),
          navigation_handle->GetURL().spec());
    }
  }
}

void ResourceCoordinatorTabHelper::TitleWasSet(
    content::NavigationEntry* entry) {
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }
  if (page_resource_coordinator_)
    page_resource_coordinator_->OnTitleUpdated();
}

void ResourceCoordinatorTabHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }
  if (page_resource_coordinator_)
    page_resource_coordinator_->OnFaviconUpdated();
}

void ResourceCoordinatorTabHelper::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* render_frame_host,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  if (interface_name != mojom::FrameCoordinationUnit::Name_)
    return;

  auto it = frames_.find(render_frame_host);
  DCHECK(it != frames_.end());
  it->second->AddBinding(
      mojom::FrameCoordinationUnitRequest(std::move(*interface_pipe)));
}

void ResourceCoordinatorTabHelper::UpdateUkmRecorder(int64_t navigation_id) {
  ukm_source_id_ =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  if (page_resource_coordinator_)
    page_resource_coordinator_->SetUKMSourceId(ukm_source_id_);
}

void ResourceCoordinatorTabHelper::ResetFlag() {
  first_time_title_set_ = false;
  first_time_favicon_set_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ResourceCoordinatorTabHelper)

}  // namespace resource_coordinator
