// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_mouse_driver.h"

#include "content/browser/renderer_host/input/synthetic_gesture_target.h"

namespace content {

SyntheticMouseDriver::SyntheticMouseDriver() : last_modifiers_(0) {
  mouse_event_.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
}

SyntheticMouseDriver::~SyntheticMouseDriver() {}

void SyntheticMouseDriver::DispatchEvent(SyntheticGestureTarget* target,
                                         const base::TimeTicks& timestamp) {
  mouse_event_.SetTimeStamp(timestamp);
  if (mouse_event_.GetType() != blink::WebInputEvent::kUndefined) {
    target->DispatchInputEventToPlatform(mouse_event_);
    mouse_event_.SetType(blink::WebInputEvent::kUndefined);
  }
}

void SyntheticMouseDriver::Press(float x,
                                 float y,
                                 int index,
                                 SyntheticPointerActionParams::Button button,
                                 int key_modifiers,
                                 float width,
                                 float height,
                                 float rotation_angle,
                                 float force) {
  DCHECK_EQ(index, 0);
  int modifiers =
      SyntheticPointerActionParams::GetWebMouseEventModifier(button);
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseDown, x, y,
      modifiers | key_modifiers | last_modifiers_, mouse_event_.pointer_type);
  mouse_event_.click_count = 1;
  mouse_event_.button =
      SyntheticPointerActionParams::GetWebMouseEventButton(button);
  last_modifiers_ = modifiers | last_modifiers_;
}

void SyntheticMouseDriver::Move(float x,
                                float y,
                                int index,
                                int key_modifiers,
                                float width,
                                float height,
                                float rotation_angle,
                                float force) {
  DCHECK_EQ(index, 0);
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseMove, x, y, key_modifiers | last_modifiers_,
      mouse_event_.pointer_type);
  mouse_event_.button =
      SyntheticPointerActionParams::GetWebMouseEventButtonFromModifier(
          last_modifiers_);
  mouse_event_.click_count = 0;
}

void SyntheticMouseDriver::Release(int index,
                                   SyntheticPointerActionParams::Button button,
                                   int key_modifiers) {
  DCHECK_EQ(index, 0);
  mouse_event_ = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseUp, mouse_event_.PositionInWidget().x,
      mouse_event_.PositionInWidget().y, key_modifiers | last_modifiers_,
      mouse_event_.pointer_type);
  mouse_event_.click_count = 1;
  mouse_event_.button =
      SyntheticPointerActionParams::GetWebMouseEventButton(button);
  last_modifiers_ =
      last_modifiers_ &
      (~SyntheticPointerActionParams::GetWebMouseEventModifier(button));
}

void SyntheticMouseDriver::Cancel(int index) {
  NOTIMPLEMENTED();
}

void SyntheticMouseDriver::Leave(int index) {
  NOTIMPLEMENTED();
}

bool SyntheticMouseDriver::UserInputCheck(
    const SyntheticPointerActionParams& params) const {
  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED) {
    return false;
  }

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::PRESS) {
    int modifiers =
        SyntheticPointerActionParams::GetWebMouseEventModifier(params.button());
    if (last_modifiers_ & modifiers)
      return false;
  }

  if (params.pointer_action_type() ==
      SyntheticPointerActionParams::PointerActionType::RELEASE) {
    int modifiers =
        SyntheticPointerActionParams::GetWebMouseEventModifier(params.button());
    if (!(last_modifiers_ & modifiers))
      return false;
  }

  return true;
}

}  // namespace content