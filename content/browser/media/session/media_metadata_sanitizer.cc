// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_metadata_sanitizer.h"

#include <algorithm>
#include <string>

#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace content {

namespace {

// Maximum length for all the strings inside the MediaMetadata when it is sent
// over IPC. The renderer process should truncate the strings before sending
// the MediaMetadata and the browser process must do the same when receiving
// it.
const size_t kMaxIPCStringLength = 4 * 1024;

// Maximum type length of MediaImage, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxMediaImageTypeLength = 2 * 127 + 1;

// Maximum number of MediaImages inside the MediaMetadata.
const size_t kMaxNumberOfMediaImages = 10;

// Maximum of sizes in a MediaImage.
const size_t kMaxNumberOfMediaImageSizes = 10;

bool CheckMediaImageSrcSanity(const GURL& src) {
  if (!src.is_valid())
    return false;
  if (!src.SchemeIsHTTPOrHTTPS() &&
      !src.SchemeIs(url::kDataScheme) &&
      !src.SchemeIs(url::kBlobScheme))
    return false;
  if (src.spec().size() > url::kMaxURLChars)
    return false;

  return true;
}

bool CheckMediaImageSanity(const media_session::MediaImage& image) {
  if (!CheckMediaImageSrcSanity(image.src))
    return false;
  if (image.type.size() > kMaxMediaImageTypeLength)
    return false;
  if (image.sizes.size() > kMaxNumberOfMediaImageSizes)
    return false;

  return true;
}

}  // anonymous namespace

bool MediaMetadataSanitizer::SanitizeAndConvert(
    const blink::mojom::SpecMediaMetadataPtr& metadata,
    media_session::MediaMetadata* metadata_out) {
  if (metadata->title.size() > kMaxIPCStringLength)
    return false;
  if (metadata->artist.size() > kMaxIPCStringLength)
    return false;
  if (metadata->album.size() > kMaxIPCStringLength)
    return false;
  if (metadata->artwork.size() > kMaxNumberOfMediaImages)
    return false;

  metadata_out->title = metadata->title;
  metadata_out->artist = metadata->artist;
  metadata_out->album = metadata->album;

  for (const auto& image : metadata->artwork) {
    if (!CheckMediaImageSanity(image))
      return false;

    metadata_out->artwork.push_back(image);
  }

  return true;
}

}  // namespace content
