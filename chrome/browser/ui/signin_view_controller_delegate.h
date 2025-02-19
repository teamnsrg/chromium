// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_

#include "base/macros.h"

class Browser;
class SigninViewController;

namespace signin_metrics {
enum class AccessPoint;
}

namespace content {
class WebContents;
}

// Interface to the platform-specific managers of the Signin and Sync
// confirmation tab-modal dialogs. This and its platform-specific
// implementations are responsible for actually creating and owning the dialogs,
// as well as managing the navigation inside them.
// Subclasses are responsible for deleting themselves when the window they're
// managing closes.
class SigninViewControllerDelegate {
 public:
  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the sync confirmation dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSyncConfirmationDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the modal sign in error dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSigninErrorDelegate(
      SigninViewController* signin_view_controller,
      Browser* browser);

  // Closes the sign-in dialog. Note that this method may destroy this object,
  // so the caller should no longer use this object after calling this method.
  virtual void CloseModalSignin() = 0;

  // Either navigates back in the signin flow if the history state allows it or
  // closes the flow otherwise. Note that if view is closed, this method may
  // destroy this object, so the caller should no longer use this object after
  // calling this method.
  virtual void PerformNavigation() = 0;

  // This will be called by the base class to request a resize of the native
  // view hosting the content to |height|. |height| is the total height of the
  // content, in pixels.
  virtual void ResizeNativeView(int height) = 0;

  // Returns the web contents of the modal dialog.
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  SigninViewControllerDelegate();
  virtual ~SigninViewControllerDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninViewControllerDelegate);
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
