// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"

namespace chromeos {

MockAutoEnrollmentCheckScreen::MockAutoEnrollmentCheckScreen(
    BaseScreenDelegate* base_screen_delegate,
    AutoEnrollmentCheckScreenView* view)
    : AutoEnrollmentCheckScreen(base_screen_delegate, view) {}

MockAutoEnrollmentCheckScreen::~MockAutoEnrollmentCheckScreen() {}

void MockAutoEnrollmentCheckScreen::RealShow() {
  AutoEnrollmentCheckScreen::Show();
}

MockAutoEnrollmentCheckScreenView::MockAutoEnrollmentCheckScreenView() =
    default;

MockAutoEnrollmentCheckScreenView::~MockAutoEnrollmentCheckScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockAutoEnrollmentCheckScreenView::SetDelegate(Delegate* screen) {
  screen_ = screen;
  MockSetDelegate(screen);
}

}  // namespace chromeos
