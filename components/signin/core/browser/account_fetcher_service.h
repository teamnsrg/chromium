// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_FETCHER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_FETCHER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "ui/gfx/image/image.h"

class AccountInfoFetcher;
class AccountTrackerService;
class OAuth2TokenService;
class PrefRegistrySimple;
class SigninClient;

#if defined(OS_ANDROID)
class ChildAccountInfoFetcherAndroid;
#endif

namespace base {
class DictionaryValue;
}

namespace image_fetcher {
struct RequestMetadata;
class ImageDecoder;
class ImageFetcherImpl;
}  // namespace image_fetcher

class AccountFetcherService : public KeyedService,
                              public OAuth2TokenService::Observer {
 public:
  // Name of the preference that tracks the int64_t representation of the last
  // time the AccountTrackerService was updated.
  static const char kLastUpdatePref[];

  // Size used for downloading account pictures. Exposed for tests.
  static const int kAccountImageDownloadSize;

  AccountFetcherService();
  ~AccountFetcherService() override;

  // Registers the preferences used by AccountFetcherService.
  static void RegisterPrefs(PrefRegistrySimple* user_prefs);

  void Initialize(SigninClient* signin_client,
                  OAuth2TokenService* token_service,
                  AccountTrackerService* account_tracker_service,
                  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder);

  // KeyedService implementation
  void Shutdown() override;

  // Indicates if all user information has been fetched. If the result is false,
  // there are still unfininshed fetchers.
  virtual bool IsAllUserInfoFetched() const;

  void FetchUserInfoBeforeSignin(const std::string& account_id);

  AccountTrackerService* account_tracker_service() const {
    return account_tracker_service_;
  }

  // It is important that network fetches are not enabled until the profile is
  // loaded. See http://crbug.com/441399 for more context.
  void OnProfileLoaded();

  void EnableNetworkFetchesForTest();

#if defined(OS_ANDROID)
  // Called by ChildAccountInfoFetcherAndroid.
  void SetIsChildAccount(const std::string& account_id, bool is_child_account);
#endif

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

 private:
  friend class AccountInfoFetcher;

  void RefreshAllAccountInfo(bool only_fetch_if_invalid);
  void RefreshAllAccountsAndScheduleNext();
  void ScheduleNextRefresh();

#if defined(OS_ANDROID)
  // Called on all account state changes. Decides whether to fetch new child
  // status information or reset old values that aren't valid now.
  void UpdateChildInfo();
#endif

  void MaybeEnableNetworkFetches();

  // Virtual so that tests can override the network fetching behaviour.
  // Further the two fetches are managed by a different refresh logic and
  // thus, can not be combined.
  virtual void StartFetchingUserInfo(const std::string& account_id);
#if defined(OS_ANDROID)
  virtual void StartFetchingChildInfo(const std::string& account_id);

  // If there is more than one account in a profile, we forcibly reset the
  // child status for an account to be false.
  void ResetChildInfo();
#endif

  // Refreshes the AccountInfo associated with |account_id|.
  void RefreshAccountInfo(const std::string& account_id,
                          bool only_fetch_if_invalid);

  // Called by AccountInfoFetcher.
  void OnUserInfoFetchSuccess(const std::string& account_id,
                              std::unique_ptr<base::DictionaryValue> user_info);
  void OnUserInfoFetchFailure(const std::string& account_id);

  image_fetcher::ImageFetcherImpl* GetOrCreateImageFetcher();

  // Called in |OnUserInfoFetchSuccess| after the account info has been fetched.
  void FetchAccountImage(const std::string& account_id);

  void OnImageFetched(const std::string& id,
                      const gfx::Image& image,
                      const image_fetcher::RequestMetadata& image_metadata);

  AccountTrackerService* account_tracker_service_ = nullptr;  // Not owned.
  OAuth2TokenService* token_service_ = nullptr;               // Not owned.
  SigninClient* signin_client_ = nullptr;                     // Not owned.
  bool network_fetches_enabled_ = false;
  bool profile_loaded_ = false;
  bool refresh_tokens_loaded_ = false;
  bool shutdown_called_ = false;
  base::Time last_updated_;
  base::OneShotTimer timer_;

#if defined(OS_ANDROID)
  std::string child_request_account_id_;
  std::unique_ptr<ChildAccountInfoFetcherAndroid> child_info_request_;
#endif

  // Holds references to account info fetchers keyed by account_id.
  std::unordered_map<std::string, std::unique_ptr<AccountInfoFetcher>>
      user_info_requests_;

  // Used for fetching the account images.
  std::unique_ptr<image_fetcher::ImageFetcherImpl> image_fetcher_;
  std::unique_ptr<image_fetcher::ImageDecoder> image_decoder_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AccountFetcherService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_FETCHER_SERVICE_H_
