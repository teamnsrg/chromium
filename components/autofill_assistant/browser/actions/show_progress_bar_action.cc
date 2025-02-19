// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ShowProgressBarAction::ShowProgressBarAction(const ActionProto& proto)
    : Action(proto) {
  DCHECK(proto_.has_show_progress_bar());
}

ShowProgressBarAction::~ShowProgressBarAction() {}

void ShowProgressBarAction::InternalProcessAction(
    ActionDelegate* delegate,
    ProcessActionCallback callback) {
  if (proto_.show_progress_bar().progress() == 0) {
    // Old script might still contain a ShowProgressBar action that clears the
    // progress. Ignore these.
    return;
  }
  if (!proto_.show_progress_bar().message().empty()) {
    delegate->SetStatusMessage(proto_.show_progress_bar().message());
  }
  int progress =
      std::min(100, std::max(0, proto_.show_progress_bar().progress()));
  delegate->SetProgress(progress);

  UpdateProcessedAction(ACTION_APPLIED);
  std::move(callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
