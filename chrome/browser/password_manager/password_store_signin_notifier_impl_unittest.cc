// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_signin_notifier_impl.h"

#include "base/bind.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace password_manager {
namespace {

class PasswordStoreSigninNotifierImplTest : public testing::Test {
 public:
  PasswordStoreSigninNotifierImplTest() {
    testing_profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            testing_profile_.get());
    store_ = new MockPasswordStore();
  }

  ~PasswordStoreSigninNotifierImplTest() override {
    store_->ShutdownOnUIThread();
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  scoped_refptr<MockPasswordStore> store_;
};

// Checks that if a notifier is subscribed on sign-in events, then
// a password store receives sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Subscribed) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com");
  testing::Mock::VerifyAndClearExpectations(store_.get());
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, Unsubscribed) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());
  notifier.UnsubscribeFromSigninEvents();
  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash()).Times(0);
  identity_test_env()->MakePrimaryAccountAvailable("test@example.com");
  identity_test_env()->ClearPrimaryAccount();
}

// Checks that if a notifier is unsubscribed on sign-in events, then
// a password store receives no sign-in notifications.
TEST_F(PasswordStoreSigninNotifierImplTest, SignOutContentArea) {
  PasswordStoreSigninNotifierImpl notifier(testing_profile_.get());
  notifier.SubscribeToSigninEvents(store_.get());

  identity_test_env()->MakePrimaryAccountAvailable("username");
  testing::Mock::VerifyAndClearExpectations(store_.get());
  EXPECT_CALL(*store_, ClearGaiaPasswordHash("username2"));
  auto* identity_manager = identity_test_env()->identity_manager();
  AccountFetcherService* account_fetcher_service =
      AccountFetcherServiceFactory::GetForProfile(testing_profile_.get());
  identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
      /*gaia_id=*/"secondary_account_id",
      /*email=*/"username2",
      /*refresh_token=*/"refresh_token",
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  // This call is necessary to ensure that the account removal is fully
  // processed in this testing context.
  account_fetcher_service->EnableNetworkFetchesForTest();
  identity_manager->GetAccountsMutator()->RemoveAccount(
      "secondary_account_id",
      signin_metrics::SourceForRefreshTokenOperation::kUserMenu_RemoveAccount);
  testing::Mock::VerifyAndClearExpectations(store_.get());

  EXPECT_CALL(*store_, ClearAllGaiaPasswordHash());
  identity_test_env()->ClearPrimaryAccount();
  notifier.UnsubscribeFromSigninEvents();
}

}  // namespace
}  // namespace password_manager
