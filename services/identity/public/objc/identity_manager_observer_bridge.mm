// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "services/identity/public/objc/identity_manager_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace identity {

IdentityManagerObserverBridge::IdentityManagerObserverBridge(
    IdentityManager* identity_manager,
    id<IdentityManagerObserverBridgeDelegate> delegate)
    : identity_manager_(identity_manager), delegate_(delegate) {
  identity_manager_->AddObserver(this);
}

IdentityManagerObserverBridge::~IdentityManagerObserverBridge() {
  identity_manager_->RemoveObserver(this);
}

void IdentityManagerObserverBridge::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  if ([delegate_ respondsToSelector:@selector(onPrimaryAccountSet:)]) {
    [delegate_ onPrimaryAccountSet:primary_account_info];
  }
}

void IdentityManagerObserverBridge::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  if ([delegate_ respondsToSelector:@selector(onPrimaryAccountCleared:)]) {
    [delegate_ onPrimaryAccountCleared:previous_primary_account_info];
  }
}

void IdentityManagerObserverBridge::OnPrimaryAccountSigninFailed(
    const GoogleServiceAuthError& error) {
  if ([delegate_ respondsToSelector:@selector(onPrimaryAccountSigninFailed:)]) {
    [delegate_ onPrimaryAccountSigninFailed:error];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info) {
  if ([delegate_
          respondsToSelector:@selector(onRefreshTokenUpdatedForAccount:)]) {
    [delegate_ onRefreshTokenUpdatedForAccount:account_info];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokenRemovedForAccount(
    const std::string& account_id) {
  if ([delegate_
          respondsToSelector:@selector(onRefreshTokenRemovedForAccount:)]) {
    [delegate_ onRefreshTokenRemovedForAccount:account_id];
  }
}

void IdentityManagerObserverBridge::OnRefreshTokensLoaded() {
  if ([delegate_ respondsToSelector:@selector(onRefreshTokensLoaded)]) {
    [delegate_ onRefreshTokensLoaded];
  }
}

void IdentityManagerObserverBridge::OnAccountsInCookieUpdated(
    const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
    const GoogleServiceAuthError& error) {
  if ([delegate_ respondsToSelector:@selector(onAccountsInCookieUpdated:
                                                                  error:)]) {
    [delegate_ onAccountsInCookieUpdated:accounts_in_cookie_jar_info
                                   error:error];
  }
}

void IdentityManagerObserverBridge::OnEndBatchOfRefreshTokenStateChanges() {
  if ([delegate_
          respondsToSelector:@selector(onEndBatchOfRefreshTokenStateChanges)]) {
    [delegate_ onEndBatchOfRefreshTokenStateChanges];
  }
}

}  // namespace identity
