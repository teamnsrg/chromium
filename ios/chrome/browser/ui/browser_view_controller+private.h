// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_PRIVATE_H_

#import "ios/chrome/browser/ui/browser_view_controller.h"

#import "base/ios/block_types.h"

// This is a private category that is intended to only be imported in
// browser_coordinator.mm.
@interface BrowserViewController (Private)

// Activates/deactivates the object. This will enable/disable the ability for
// this object to browse, and to have live UIWebViews associated with it. While
// not active, the UI will not react to changes in the tab model, so generally
// an inactive BVC should not be visible.
@property(nonatomic, assign, getter=isActive) BOOL active;

// Dismisses all presented views, excluding the omnibox if |dismissOmnibox| is
// NO, then calls |completion|.
- (void)clearPresentedStateWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Switch to the tab best represented by the given |params|.
- (void)switchToTabWithParams:
    (const web::NavigationManager::WebLoadParams&)params;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_PRIVATE_H_
