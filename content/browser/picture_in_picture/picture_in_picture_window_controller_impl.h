// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class OverlaySurfaceEmbedder;
class WebContents;
class WebContentsImpl;
class MediaWebContentsObserver;

// TODO(thakis,mlamouri): PictureInPictureWindowControllerImpl isn't
// CONTENT_EXPORT'd because it creates complicated build issues with
// WebContentsUserData being a non-exported template. As a result, the class
// uses CONTENT_EXPORT for methods that are being used from tests.
// CONTENT_EXPORT should be moved back to the class when the Windows build will
// work with it. https://crbug.com/589840.
class PictureInPictureWindowControllerImpl
    : public PictureInPictureWindowController,
      public WebContentsUserData<PictureInPictureWindowControllerImpl> {
 public:
  // Gets a reference to the controller associated with |initiator| and creates
  // one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  CONTENT_EXPORT static PictureInPictureWindowControllerImpl*
  GetOrCreateForWebContents(WebContents* initiator);

  ~PictureInPictureWindowControllerImpl() override;

  // PictureInPictureWindowController:
  CONTENT_EXPORT gfx::Size Show() override;
  CONTENT_EXPORT void Close(bool should_pause_video,
                            bool should_reset_pip_player) override;
  CONTENT_EXPORT void CloseAndFocusInitiator() override;
  CONTENT_EXPORT void OnWindowDestroyed() override;
  CONTENT_EXPORT void SetPictureInPictureCustomControls(
      const std::vector<blink::PictureInPictureControlInfo>& controls) override;
  CONTENT_EXPORT void EmbedSurface(const viz::SurfaceId& surface_id,
                                   const gfx::Size& natural_size) override;
  CONTENT_EXPORT OverlayWindow* GetWindowForTesting() override;
  CONTENT_EXPORT void UpdateLayerBounds() override;
  CONTENT_EXPORT bool IsPlayerActive() override;
  CONTENT_EXPORT WebContents* GetInitiatorWebContents() override;
  CONTENT_EXPORT bool TogglePlayPause() override;
  CONTENT_EXPORT void CustomControlPressed(
      const std::string& control_id) override;
  CONTENT_EXPORT void UpdatePlaybackState(bool is_playing,
                                          bool reached_end_of_stream) override;
  CONTENT_EXPORT void SetAlwaysHidePlayPauseButton(bool is_visible) override;
  CONTENT_EXPORT void SkipAd() override;

  CONTENT_EXPORT void MediaSessionActionsChanged(
      const std::set<media_session::mojom::MediaSessionAction>& actions);

 private:
  friend class WebContentsUserData<PictureInPictureWindowControllerImpl>;

  // Use PictureInPictureWindowControllerImpl::GetOrCreateForWebContents() to
  // create an instance.
  CONTENT_EXPORT explicit PictureInPictureWindowControllerImpl(
      WebContents* initiator);

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  void OnLeavingPictureInPicture(bool should_pause_video,
                                 bool should_reset_pip_player);

  // Internal method to set the states after the window was closed, whether via
  // the system or Chromium.
  void CloseInternal(bool should_pause_video, bool should_reset_pip_player);

  // Creates a new window if the previous one was destroyed. It can happen
  // because of the system control of the window.
  void EnsureWindow();

  // Allow play/pause button to be visible if Media Session actions "play" and
  // "pause" are both handled by the website or if
  // always_hide_play_pause_button_ is false.
  void UpdatePlayPauseButtonVisibility();

  std::unique_ptr<OverlayWindow> window_;
  std::unique_ptr<OverlaySurfaceEmbedder> embedder_;
  WebContentsImpl* const initiator_;

  // Used to determine the state of the media player and route messages to
  // the corresponding media player with id |media_player_id_|.
  MediaWebContentsObserver* media_web_contents_observer_;
  base::Optional<WebContentsObserver::MediaPlayerId> media_player_id_;

  viz::SurfaceId surface_id_;

  // Used to show/hide some actions in Picture-in-Picture window. These are set
  // to true when website handles some Media Session actions.
  bool media_session_action_play_handled_ = false;
  bool media_session_action_pause_handled_ = false;
  bool media_session_action_skip_ad_handled_ = false;

  // Used to hide play/pause button if video is a MediaStream or has infinite
  // duration. Play/pause button visibility can be overridden by the Media
  // Session API in UpdatePlayPauseButtonVisibility().
  bool always_hide_play_pause_button_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
