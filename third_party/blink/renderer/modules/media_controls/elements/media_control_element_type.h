// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ELEMENT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ELEMENT_TYPE_H_

// Classes sub-classing MediaControlElementBase will all have a defined type
// from this list. It is used by code that need to know what type of media
// control element it is interacting with.
enum MediaControlElementType {
  kMediaSlider,
  kMediaSliderThumb,
  kMediaTimelineContainer,
  kMediaOverflowButton,
  kMediaOverflowList,
  kMediaIgnore
};

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_ELEMENTS_MEDIA_CONTROL_ELEMENT_TYPE_H_
