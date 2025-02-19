// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_

#import <Foundation/Foundation.h>

#include "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ui/chrome_load_params.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class Browser;
class UrlLoadingNotifier;

@class OpenNewTabCommand;

// TODO(crbug.com/907527): normalize all parameters to open a url in
// UrlLoadingService and URLLoadingServiceDelegate.

// Objective-C delegate for UrlLoadingService.
@protocol URLLoadingServiceDelegate

// Implementing delegate must switch to a tab that matches |params| or open in a
// new tab.
- (void)switchToTabWithParams:
    (const web::NavigationManager::WebLoadParams&)params;

// Implementing delegate must open the url in |command| in a new tab.
- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command;

@end

// Observer used to update listeners of change of state in url loading.
class UrlLoadingService : public KeyedService {
 public:
  UrlLoadingService(UrlLoadingNotifier* notifier);

  void SetDelegate(id<URLLoadingServiceDelegate> delegate);
  void SetBrowser(Browser* browser);

  // Opens a url based on |chrome_params|.
  void LoadUrlInCurrentTab(const ChromeLoadParams& chrome_params);

  // Switches to a tab that matches |web_params| or opens in a new tab.
  void SwitchToTab(const web::NavigationManager::WebLoadParams& web_params);

  // Opens a url based on |command| in a new tab.
  void OpenUrlInNewTab(OpenNewTabCommand* command);

 private:
  __weak id<URLLoadingServiceDelegate> delegate_;
  Browser* browser_;
  UrlLoadingNotifier* notifier_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_SERVICE_H_
