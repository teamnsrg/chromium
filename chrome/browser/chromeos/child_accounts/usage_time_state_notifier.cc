// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/usage_time_state_notifier.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

namespace {

UsageTimeStateNotifier::UsageTimeState GetCurrentState() {
  if (session_manager::SessionManager::Get()->session_state() ==
      session_manager::SessionState::ACTIVE) {
    return UsageTimeStateNotifier::UsageTimeState::ACTIVE;
  } else {
    return UsageTimeStateNotifier::UsageTimeState::INACTIVE;
  }
}

}  // namespace

UsageTimeStateNotifier::UsageTimeStateNotifier()
    : last_state_(GetCurrentState()) {}

UsageTimeStateNotifier::~UsageTimeStateNotifier() {}

// static
UsageTimeStateNotifier* UsageTimeStateNotifier::GetInstance() {
  static base::NoDestructor<UsageTimeStateNotifier> instance;
  return instance.get();
}

void UsageTimeStateNotifier::AddObserver(
    UsageTimeStateNotifier::Observer* observer) {
  DCHECK(observer);
  if (!observers_.might_have_observers()) {
    session_manager::SessionManager::Get()->AddObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
    last_state_ = GetCurrentState();
  }
  observers_.AddObserver(observer);
}

void UsageTimeStateNotifier::RemoveObserver(
    UsageTimeStateNotifier::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
  if (!observers_.might_have_observers()) {
    session_manager::SessionManager::Get()->RemoveObserver(this);
    DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  }
}

void UsageTimeStateNotifier::ChangeUsageTimeState(
    UsageTimeStateNotifier::UsageTimeState state) {
  if (state == last_state_)
    return;
  for (auto& observer : observers_) {
    observer.OnUsageTimeStateChange(state);
  }
  last_state_ = state;
}

void UsageTimeStateNotifier::OnSessionStateChanged() {
  ChangeUsageTimeState(GetCurrentState());
}

void UsageTimeStateNotifier::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& state) {
  // State should be inactive only if the screen is turned off.
  if (!state.off() && session_manager::SessionManager::Get()->session_state() ==
                          session_manager::SessionState::ACTIVE) {
    ChangeUsageTimeState(UsageTimeState::ACTIVE);
  } else {
    ChangeUsageTimeState(UsageTimeState::INACTIVE);
  }
}

void UsageTimeStateNotifier::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  ChangeUsageTimeState(UsageTimeState::INACTIVE);
}

void UsageTimeStateNotifier::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  ChangeUsageTimeState(GetCurrentState());
}

}  // namespace chromeos
