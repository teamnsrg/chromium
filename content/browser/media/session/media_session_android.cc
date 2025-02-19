// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_android.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/time/time.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/media_session.h"
#include "jni/MediaSessionImpl_jni.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace content {

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

struct MediaSessionAndroid::JavaObjectGetter {
  static ScopedJavaLocalRef<jobject> GetJavaObject(
      MediaSessionAndroid* session_android) {
    return session_android->GetJavaObject();
  }
};

MediaSessionAndroid::MediaSessionAndroid(MediaSessionImpl* session)
    : media_session_(session) {
  DCHECK(media_session_);

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_media_session =
      Java_MediaSessionImpl_create(env, reinterpret_cast<intptr_t>(this));
  j_media_session_ = JavaObjectWeakGlobalRef(env, j_media_session);

  WebContentsAndroid* contents_android = GetWebContentsAndroid();
  if (contents_android)
    contents_android->SetMediaSession(j_media_session);

  media_session::mojom::MediaSessionObserverPtr observer;
  observer_binding_.Bind(mojo::MakeRequest(&observer));
  session->AddObserver(std::move(observer));
}

MediaSessionAndroid::~MediaSessionAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_local_session = j_media_session_.get(env);

  // The Java object will tear down after this call.
  if (!j_local_session.is_null())
    Java_MediaSessionImpl_mediaSessionDestroyed(env, j_local_session);

  j_media_session_.reset();

  WebContentsAndroid* contents_android = GetWebContentsAndroid();
  if (contents_android)
    contents_android->SetMediaSession(nullptr);
}

// static
ScopedJavaLocalRef<jobject> JNI_MediaSessionImpl_GetMediaSessionFromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_contents_android) {
  WebContents* contents = WebContents::FromJavaWebContents(j_contents_android);
  if (!contents)
    return ScopedJavaLocalRef<jobject>();

  MediaSessionImpl* session = MediaSessionImpl::Get(contents);
  DCHECK(session);
  return MediaSessionAndroid::JavaObjectGetter::GetJavaObject(
      session->session_android());
}

void MediaSessionAndroid::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr session_info) {
  ScopedJavaLocalRef<jobject> j_local_session = GetJavaObject();
  if (j_local_session.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaSessionImpl_mediaSessionStateChanged(
      env, j_local_session, session_info->is_controllable,
      session_info->playback_state ==
          media_session::mojom::MediaPlaybackState::kPaused);
}

void MediaSessionAndroid::MediaSessionMetadataChanged(
    const base::Optional<media_session::MediaMetadata>& metadata) {
  ScopedJavaLocalRef<jobject> j_local_session = GetJavaObject();
  if (j_local_session.is_null())
    return;

  JNIEnv* env = base::android::AttachCurrentThread();

  // Avoid translating metadata through JNI if there is no Java observer.
  if (!Java_MediaSessionImpl_hasObservers(env, j_local_session))
    return;

  ScopedJavaLocalRef<jobject> j_metadata;
  if (metadata.has_value())
    j_metadata = metadata.value().CreateJavaObject(env);
  Java_MediaSessionImpl_mediaSessionMetadataChanged(env, j_local_session,
                                                    j_metadata);
}

void MediaSessionAndroid::MediaSessionActionsChanged(
    const std::vector<media_session::mojom::MediaSessionAction>& actions) {
  ScopedJavaLocalRef<jobject> j_local_session = GetJavaObject();
  if (j_local_session.is_null())
    return;

  std::vector<int> actions_vec;
  for (auto action : actions)
    actions_vec.push_back(static_cast<int>(action));

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaSessionImpl_mediaSessionActionsChanged(
      env, j_local_session, base::android::ToJavaIntArray(env, actions_vec));
}

void MediaSessionAndroid::Resume(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_obj) {
  DCHECK(media_session_);
  media_session_->Resume(MediaSession::SuspendType::kUI);
}

void MediaSessionAndroid::Suspend(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_obj) {
  DCHECK(media_session_);
  media_session_->Suspend(MediaSession::SuspendType::kUI);
}

void MediaSessionAndroid::Stop(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_obj) {
  DCHECK(media_session_);
  media_session_->Stop(MediaSession::SuspendType::kUI);
}

void MediaSessionAndroid::Seek(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_obj,
    const jlong millis) {
  DCHECK(media_session_);
  DCHECK_NE(millis, 0)
      << "Attempted to seek by a missing number of milliseconds";
  media_session_->Seek(base::TimeDelta::FromMilliseconds(millis));
}

void MediaSessionAndroid::DidReceiveAction(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           int action) {
  media_session_->DidReceiveAction(
      static_cast<media_session::mojom::MediaSessionAction>(action));
}

void MediaSessionAndroid::RequestSystemAudioFocus(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_obj) {
  DCHECK(media_session_);
  media_session_->RequestSystemAudioFocus(
      media_session::mojom::AudioFocusType::kGain);
}

WebContentsAndroid* MediaSessionAndroid::GetWebContentsAndroid() {
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(media_session_->web_contents());
  if (!contents)
    return nullptr;
  return contents->GetWebContentsAndroid();
}

ScopedJavaLocalRef<jobject> MediaSessionAndroid::GetJavaObject() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return j_media_session_.get(env);
}

}  // namespace content
