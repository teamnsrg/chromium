// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace content {

class WebContents;

// MediaSession manages the media session and audio focus for a given
// WebContents. There is only one MediaSession per WebContents.
//
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class MediaSession : public media_session::mojom::MediaSession {
 public:
  // Returns the MediaSession associated to this WebContents. Creates one if
  // none is currently available.
  CONTENT_EXPORT static MediaSession* Get(WebContents* contents);

  ~MediaSession() override = default;

  // Tell the media session a user action has performed.
  virtual void DidReceiveAction(
      media_session::mojom::MediaSessionAction action) = 0;

  // Set the volume multiplier applied during ducking.
  virtual void SetDuckingVolumeMultiplier(double multiplier) = 0;

  // Set the audio focus group id for this media session. Sessions in the same
  // group can share audio focus. Setting this to null will use the browser
  // default value. This will only have any effect if audio focus grouping is
  // supported.
  virtual void SetAudioFocusGroupId(const base::UnguessableToken& group_id) = 0;

  // media_session.mojom.MediaSession overrides -------------------------------

  // Suspend the media session.
  // |type| represents the origin of the request.
  void Suspend(SuspendType suspend_type) override = 0;

  // Resume the media session.
  // |type| represents the origin of the request.
  void Resume(SuspendType suspend_type) override = 0;

  // Let the media session start ducking such that the volume multiplier is
  // reduced.
  void StartDucking() override = 0;

  // Let the media session stop ducking such that the volume multiplier is
  // recovered.
  void StopDucking() override = 0;

  // Returns information about the MediaSession.
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override = 0;

  // Returns debug information about the MediaSession.
  void GetDebugInfo(GetDebugInfoCallback callback) override = 0;

  // Adds an observer to listen to events related to this MediaSession.
  void AddObserver(
      media_session::mojom::MediaSessionObserverPtr observer) override = 0;

  // Skip to the previous track. If there is no previous track then this will be
  // a no-op.
  void PreviousTrack() override = 0;

  // Skip to the next track. If there is no next track then this will be a
  // no-op.
  void NextTrack() override = 0;

  // Skip ad.
  void SkipAd() override = 0;

  // Seek the media session. If the media cannot seek then this will be a no-op.
  // The |seek_time| is the time delta that the media will seek by and supports
  // both positive and negative values.
  void Seek(base::TimeDelta seek_time) override = 0;

  // Stop the media session.
  // |type| represents the origin of the request.
  void Stop(SuspendType suspend_type) override = 0;

 protected:
  MediaSession() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SESSION_H_
