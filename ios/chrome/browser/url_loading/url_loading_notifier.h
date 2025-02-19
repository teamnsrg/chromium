// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_
#define IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class UrlLoadingObserverBridge;

// A class containing static functions to notify observers of url loading
// state change.
class UrlLoadingNotifier : public KeyedService {
 public:
  // Creates a UrlLoadingNotifier.
  explicit UrlLoadingNotifier();
  ~UrlLoadingNotifier() override;

  // Adds |observer| to the list of observers.
  void AddObserver(UrlLoadingObserverBridge* observer);

  // Removes |observer| from the list of observers.
  void RemoveObserver(UrlLoadingObserverBridge* observer);

  // The loader will open |url| in the current tab. Next state will be
  // one of: TabFailedToOpenUrl, TabDidPrerenderUrl,
  // TabDidReloadUrl or TabDidOpenUrl.
  void TabWillOpenUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader didn't succeed opening the requested |url|. Reason
  // can, for example be an incognito mismatch or an induced crash.
  // It is possible that the url was opened, but in another tab.
  void TabFailedToOpenUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader replaced the load with a prerendering.
  void TabDidPrerenderUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader reloaded the |url| in the current tab.
  void TabDidReloadUrl(const GURL& url, ui::PageTransition transition_type);

  // The loader initiated the |url| loading successfully.
  void TabDidOpenUrl(const GURL& url, ui::PageTransition transition_type);

 private:
  base::ObserverList<UrlLoadingObserverBridge>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(UrlLoadingNotifier);
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_URL_LOADING_NOTIFIER_H_
