// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/rectf.h"
#include "components/autofill_assistant/browser/state.h"

namespace autofill_assistant {

// UI delegate called for script executions.
class UiDelegate {
 public:
  virtual ~UiDelegate() = default;

  // Returns the current state of the controller.
  virtual AutofillAssistantState GetState() = 0;

  // Asks for updated coordinates for the touchable area. This is called to
  // speed up update of the touchable areas when there are good reasons to think
  // that the current coordinates are out of date, such as while scrolling.
  virtual void UpdateTouchableArea() = 0;

  // Called when user interaction within the allowed touchable area was
  // detected. This should cause rerun of preconditions check.
  virtual void OnUserInteractionInsideTouchableArea() = 0;

  // Returns a string describing the current execution context. This is useful
  // when analyzing feedback forms and for debugging in general.
  virtual std::string GetDebugContext() = 0;

  // Returns the current status message.
  virtual std::string GetStatusMessage() const = 0;

  // Returns the current contextual information. May be null if empty.
  virtual const Details* GetDetails() const = 0;

  // Returns the current progress; a percentage.
  virtual int GetProgress() const = 0;

  // Returns the current set of chips.
  virtual const std::vector<Chip>& GetChips() const = 0;

  // Selects a chip, from the set of chips returned by GetChips().
  virtual void SelectChip(int chip) = 0;

  // Returns the drop out reason for the last state transition to STOPPED.
  virtual Metrics::DropOutReason GetDropOutReason() const = 0;

  // Adds the rectangles that correspond to the current touchable area to the
  // given vector.
  //
  // |areas| is expressed in coordinates relative to the width or height of the
  // visible viewport, as a number between 0 and 1. It can be empty.
  //
  // Note that the vector is not cleared before rectangles are added.
  virtual void GetTouchableArea(std::vector<RectF>* area) const = 0;

 protected:
  UiDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
