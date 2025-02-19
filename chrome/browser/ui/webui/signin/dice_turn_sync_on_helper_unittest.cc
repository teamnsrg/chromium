// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;
using namespace unified_consent;

class DiceTurnSyncOnHelperTestBase;

namespace {

const char kEmail[] = "foo@gmail.com";
const char kPreviousEmail[] = "notme@bar.com";
const char kEnterpriseEmail[] = "enterprise@managed.com";

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER;
const signin_metrics::PromoAction kSigninPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;
const signin_metrics::Reason kSigninReason =
    signin_metrics::Reason::REASON_REAUTHENTICATION;

// Builds a testing profile with the right setup for this test.
std::unique_ptr<TestingProfile> BuildTestingProfile(
    const base::FilePath& path,
    Profile::Delegate* delegate);

// Dummy delegate forwarding all the calls the test fixture.
// Owned by the DiceTurnOnSyncHelper.
class TestDiceTurnSyncOnHelperDelegate : public DiceTurnSyncOnHelper::Delegate {
 public:
  explicit TestDiceTurnSyncOnHelperDelegate(
      DiceTurnSyncOnHelperTestBase* test_fixture);
  ~TestDiceTurnSyncOnHelperDelegate() override;

 private:
  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  DiceTurnSyncOnHelperTestBase* test_fixture_;
};

// Simple ProfileManager creating testing profiles.
class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  Profile* CreateProfileHelper(const base::FilePath& file_path) override {
    if (!base::PathExists(file_path) && !base::CreateDirectory(file_path))
      return nullptr;
    return BuildTestingProfile(file_path, /*delegate=*/nullptr).release();
  }

  Profile* CreateProfileAsyncHelper(const base::FilePath& path,
                                    Delegate* delegate) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));
    return BuildTestingProfile(path, this).release();
  }
};

// Fake user policy signin service immediately invoking the callbacks.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
  // Static method to use with BrowserContextKeyedServiceFactory.
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<FakeUserPolicySigninService>(
        profile, IdentityManagerFactory::GetForProfile(profile));
  }

  FakeUserPolicySigninService(Profile* profile,
                              identity::IdentityManager* identity_manager)
      : UserPolicySigninService(profile,
                                nullptr,
                                nullptr,
                                nullptr,
                                identity_manager,
                                nullptr) {}

  void set_dm_token(const std::string& dm_token) { dm_token_ = dm_token; }
  void set_client_id(const std::string& client_id) { client_id_ = client_id; }
  void set_account(const std::string& account_id, const std::string& email) {
    account_id_ = account_id;
    email_ = email;
  }

  // policy::UserPolicySigninService:
  void RegisterForPolicyWithAccountId(
      const std::string& username,
      const std::string& account_id,
      const PolicyRegistrationCallback& callback) override {
    EXPECT_EQ(email_, username);
    EXPECT_EQ(account_id_, account_id);
    callback.Run(dm_token_, client_id_);
  }

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      const PolicyFetchCallback& callback) override {
    callback.Run(true);
  }

 private:
  std::string dm_token_;
  std::string client_id_;
  std::string account_id_;
  std::string email_;
};

std::unique_ptr<TestingProfile> BuildTestingProfile(
    const base::FilePath& path,
    Profile::Delegate* delegate) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&signin::BuildTestSigninClient));
  profile_builder.AddTestingFactory(
      ProfileSyncServiceFactory::GetInstance(),
      base::BindRepeating(&BuildMockProfileSyncService));
  profile_builder.AddTestingFactory(
      policy::UserPolicySigninServiceFactory::GetInstance(),
      base::BindRepeating(&FakeUserPolicySigninService::Build));
  profile_builder.SetDelegate(delegate);
  profile_builder.SetPath(path);
  return IdentityTestEnvironmentProfileAdaptor::
      CreateProfileForIdentityTestEnvironment(profile_builder);
}

}  // namespace

class DiceTurnSyncOnHelperTestBase : public testing::Test {
 public:
  DiceTurnSyncOnHelperTestBase()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.GetPath()));

    profile_ = BuildTestingProfile(base::FilePath(), /*delegate=*/nullptr);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    account_id_ = identity_test_env()->MakeAccountAvailable(kEmail).account_id;
    user_policy_signin_service_ = static_cast<FakeUserPolicySigninService*>(
        policy::UserPolicySigninServiceFactory::GetForProfile(profile()));
    user_policy_signin_service_->set_account(account_id_, kEmail);
    EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_));
    initial_device_id_ = GetSigninScopedDeviceIdForProfile(profile());
    EXPECT_FALSE(initial_device_id_.empty());
  }

  ~DiceTurnSyncOnHelperTestBase() override {
    DCHECK(delegate_destroyed_);
    // Destroy extra profiles.
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  // Basic accessors.
  Profile* profile() { return profile_.get(); }
  identity::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }
  identity::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }
  const std::string& account_id() { return account_id_; }
  FakeUserPolicySigninService* user_policy_signin_service() {
    return user_policy_signin_service_;
  }
  const std::string initial_device_id() { return initial_device_id_; }
  bool delegate_destroyed() const { return delegate_destroyed_; }
  std::string enterprise_confirmation_email() const {
    return enterprise_confirmation_email_;
  }

  void ClearProfile() {
    identity_test_env_profile_adaptor_.reset();
    profile_.reset();
    user_policy_signin_service_ = nullptr;
  }

  // Gets the ProfileSyncServiceMock.
  browser_sync::ProfileSyncServiceMock* GetProfileSyncServiceMock() {
    return static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetForProfile(profile()));
  }

  DiceTurnSyncOnHelper* CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode mode) {
    return new DiceTurnSyncOnHelper(
        profile(), kAccessPoint, kSigninPromoAction, kSigninReason, account_id_,
        mode, std::make_unique<TestDiceTurnSyncOnHelperDelegate>(this),
        base::DoNothing());
  }

  void UseEnterpriseAccount() {
    account_id_ =
        identity_test_env()->MakeAccountAvailable(kEnterpriseEmail).account_id;
    user_policy_signin_service_->set_account(account_id_, kEnterpriseEmail);
  }

  void UseInvalidAccount() { account_id_ = "invalid_account"; }

  void SetExpectationsForSyncStartupCompleted() {
    browser_sync::ProfileSyncServiceMock* sync_service_mock =
        GetProfileSyncServiceMock();
    EXPECT_CALL(*sync_service_mock, GetSetupInProgressHandle()).Times(1);
    ON_CALL(*sync_service_mock, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*sync_service_mock, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void SetExpectationsForSyncStartupPending() {
    browser_sync::ProfileSyncServiceMock* sync_service_mock =
        GetProfileSyncServiceMock();
    EXPECT_CALL(*sync_service_mock, GetSetupInProgressHandle()).Times(1);
    ON_CALL(*sync_service_mock, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*sync_service_mock, GetTransportState())
        .WillByDefault(
            Return(syncer::SyncService::TransportState::INITIALIZING));
    ON_CALL(*sync_service_mock, GetAuthError())
        .WillByDefault(ReturnRef(kNoAuthError));
  }

  void CheckDelegateCalls() {
    EXPECT_EQ(expected_login_error_email_, login_error_email_);
    EXPECT_EQ(expected_login_error_message_, login_error_message_);
    EXPECT_EQ(expected_merge_data_previous_email_, merge_data_previous_email_);
    EXPECT_EQ(expected_merge_data_new_email_, merge_data_new_email_);
    EXPECT_EQ(expected_enterprise_confirmation_email_,
              enterprise_confirmation_email_);
    EXPECT_EQ(expected_switched_to_new_profile_, switched_to_new_profile_);
    EXPECT_EQ(expected_sync_confirmation_shown_, sync_confirmation_shown_);
    EXPECT_EQ(expected_sync_settings_shown_, sync_settings_shown_);
  }

  // Functions called by the DiceTurnSyncOnHelper::Delegate:
  void OnShowLoginError(const std::string& email,
                        const std::string& error_message) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(email.empty());
    EXPECT_TRUE(login_error_email_.empty())
        << "Login error should be shown only once.";
    login_error_email_ = email;
    login_error_message_ = error_message;  // May be empty.
  }

  void OnShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(previous_email.empty());
    EXPECT_FALSE(new_email.empty());
    EXPECT_TRUE(merge_data_previous_email_.empty())
        << "Merge data confirmation should be shown only once";
    EXPECT_TRUE(merge_data_new_email_.empty())
        << "Merge data confirmation should be shown only once";
    merge_data_previous_email_ = previous_email;
    merge_data_new_email_ = new_email;
    if (run_delegate_callbacks_)
      std::move(callback).Run(merge_data_choice_);
  }

  void OnShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(email.empty());
    EXPECT_TRUE(enterprise_confirmation_email_.empty())
        << "Enterprise confirmation should be shown only once.";
    enterprise_confirmation_email_ = email;
    if (run_delegate_callbacks_)
      std::move(callback).Run(enterprise_choice_);
  }

  void OnShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    EXPECT_FALSE(sync_confirmation_shown_)
        << "Sync confirmation should be shown only once.";
    sync_confirmation_shown_ = true;
    if (run_delegate_callbacks_)
      std::move(callback).Run(sync_confirmation_result_);
  }

  void OnShowSyncSettings() {
    EXPECT_TRUE(sync_confirmation_shown_)
        << "Must show sync confirmation first";
    EXPECT_FALSE(sync_settings_shown_);
    sync_settings_shown_ = true;
  }

  void SwitchToProfile(Profile* new_profile) {
    EXPECT_TRUE(new_profile);
    EXPECT_NE(profile(), new_profile)
        << "new_profile should not be the existing profile";
    EXPECT_FALSE(switched_to_new_profile_)
        << "Flow should only be restarted once";
    // The token has been transferred to the new token service, regardless of
    // SigninAbortedMode.
    EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_));
    EXPECT_TRUE(IdentityManagerFactory::GetForProfile(new_profile)
                    ->HasAccountWithRefreshToken(account_id_));
    // The initial device ID is no longer used by any profile.
    EXPECT_NE(initial_device_id(),
              GetSigninScopedDeviceIdForProfile(profile()));
    EXPECT_NE(initial_device_id(),
              GetSigninScopedDeviceIdForProfile(new_profile));

    switched_to_new_profile_ = true;
  }

  void OnDelegateDestroyed() { delegate_destroyed_ = true; }

 protected:
  // Delegate behavior.
  DiceTurnSyncOnHelper::SigninChoice merge_data_choice_ =
      DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL;
  DiceTurnSyncOnHelper::SigninChoice enterprise_choice_ =
      DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL;
  LoginUIService::SyncConfirmationUIClosedResult sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SIGNIN;
  bool run_delegate_callbacks_ = true;

  // Expected delegate calls.
  std::string expected_login_error_email_;
  std::string expected_login_error_message_;
  std::string expected_enterprise_confirmation_email_;
  std::string expected_merge_data_previous_email_;
  std::string expected_merge_data_new_email_;
  bool expected_switched_to_new_profile_ = false;
  bool expected_sync_confirmation_shown_ = false;
  bool expected_sync_settings_shown_ = false;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;
  std::string account_id_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  FakeUserPolicySigninService* user_policy_signin_service_ = nullptr;
  std::string initial_device_id_;

  // State of the delegate calls.
  bool delegate_destroyed_ = false;
  std::string login_error_email_;
  std::string login_error_message_;
  std::string enterprise_confirmation_email_;
  std::string merge_data_previous_email_;
  std::string merge_data_new_email_;
  bool switched_to_new_profile_ = false;
  bool sync_confirmation_shown_ = false;
  bool sync_settings_shown_ = false;

  // Note: This needs to be a member variable for testing::ReturnRef.
  const GoogleServiceAuthError kNoAuthError =
      GoogleServiceAuthError::AuthErrorNone();
};

// Test class with only DiceMigration enabled.
class DiceTurnSyncOnHelperTest : public DiceTurnSyncOnHelperTestBase {
 public:
  DiceTurnSyncOnHelperTest() = default;

 private:
  ScopedAccountConsistencyDiceMigration scoped_dice_;
};

// Test class with Dice and UnifiedConsent enabled.
class DiceTurnSyncOnHelperTestWithUnifiedConsent
    : public DiceTurnSyncOnHelperTestBase {
 public:
  DiceTurnSyncOnHelperTestWithUnifiedConsent()
      : scoped_unified_consent_(UnifiedConsentFeatureState::kEnabled) {}
  ~DiceTurnSyncOnHelperTestWithUnifiedConsent() override {}

 private:
  ScopedAccountConsistencyDice scoped_dice_;
  ScopedUnifiedConsent scoped_unified_consent_;
};

// TestDiceTurnSyncOnHelperDelegate implementation.

TestDiceTurnSyncOnHelperDelegate::TestDiceTurnSyncOnHelperDelegate(
    DiceTurnSyncOnHelperTestBase* test_fixture)
    : test_fixture_(test_fixture) {}

TestDiceTurnSyncOnHelperDelegate::~TestDiceTurnSyncOnHelperDelegate() {
  test_fixture_->OnDelegateDestroyed();
}

void TestDiceTurnSyncOnHelperDelegate::ShowLoginError(
    const std::string& email,
    const std::string& error_message) {
  test_fixture_->OnShowLoginError(email, error_message);
}

void TestDiceTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  test_fixture_->OnShowMergeSyncDataConfirmation(previous_email, new_email,
                                                 std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
    const std::string& email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  test_fixture_->OnShowEnterpriseAccountConfirmation(email,
                                                     std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncConfirmation(std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncSettings() {
  test_fixture_->OnShowSyncSettings();
}

void TestDiceTurnSyncOnHelperDelegate::SwitchToProfile(Profile* new_profile) {
  test_fixture_->SwitchToProfile(new_profile);
}

// Check that the invalid account is supported.
TEST_F(DiceTurnSyncOnHelperTest, InvalidAccount) {
  UseInvalidAccount();
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  CheckDelegateCalls();
}

// Tests that the login error is displayed and that the account is kept.
TEST_F(DiceTurnSyncOnHelperTest, CanOfferSigninErrorKeepAccount) {
  // Set expectations.
  expected_login_error_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Tests that the login error is displayed and that the account is removed.
TEST_F(DiceTurnSyncOnHelperTest, CanOfferSigninErrorRemoveAccount) {
  // Set expectations.
  expected_login_error_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Aborts the flow after the cross account dialog.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountAbort) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Merge data after the cross account dialog.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountContinue) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  merge_data_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Create a new profile after the cross account dialog and show the signin page.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountNewProfile) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  merge_data_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  // Check expectations.
  base::RunLoop().RunUntilIdle();  // Profile creation is asynchronous.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  // The token has been removed from the source profile even though
  // KEEP_ACCOUNT was used.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Abort after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationAbort) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Continue after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationContinue) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE;
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Continue with a new profile after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationNewProfile) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE;
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  base::RunLoop().RunUntilIdle();  // Profile creation is asynchronous.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Tests that the sync confirmation is shown and the user can abort.
TEST_F(DiceTurnSyncOnHelperTest, UndoSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(0);

  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Tests that the sync settings page is shown.
TEST_F(DiceTurnSyncOnHelperTest, ConfigureSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  expected_sync_settings_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(0);

  // Configure the test.
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::CONFIGURE_SYNC_FIRST;
  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
}

// Tests that the user is signed in and Sync configuration is complete.
TEST_F(DiceTurnSyncOnHelperTest, StartSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(1);
  // Configure the test.
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId());
  CheckDelegateCalls();
}

// Tests that the user is signed in and Sync configuration is complete.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest, ShowSyncDialogForEndConsumerAccount) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(1);

  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId());
  CheckDelegateCalls();
}

// Tests that the user enabled unified consent,
TEST_F(DiceTurnSyncOnHelperTestWithUnifiedConsent,
       ShowSyncDialogForEndConsumerAccount_UnifiedConsentEnabled) {
  ASSERT_TRUE(IsUnifiedConsentFeatureEnabled());
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(1);
  using Service = UnifiedConsentServiceClient::Service;
  using ServiceState = UnifiedConsentServiceClient::ServiceState;
  PrefService* pref_service = profile()->GetPrefs();
  ChromeUnifiedConsentServiceClient consent_service_client(pref_service);
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
      url_keyed_collection_helper = UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(
              pref_service,
              ProfileSyncServiceFactory::GetForProfile(profile()));
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (consent_service_client.IsServiceSupported(service)) {
      consent_service_client.SetServiceEnabled(service, false);
      EXPECT_EQ(ServiceState::kDisabled,
                consent_service_client.GetServiceState(service));
    }
  }
  EXPECT_FALSE(url_keyed_collection_helper->IsEnabled());

  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId());
  CheckDelegateCalls();
  for (int i = 0; i <= static_cast<int>(Service::kLast); ++i) {
    Service service = static_cast<Service>(i);
    if (consent_service_client.IsServiceSupported(service)) {
      EXPECT_EQ(ServiceState::kEnabled,
                consent_service_client.GetServiceState(service));
    }
  }
  EXPECT_TRUE(url_keyed_collection_helper->IsEnabled());
}

// For enterprise user, tests that the user is signed in only after Sync engine
// starts.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest,
       ShowSyncDialogBlockedUntilSyncStartupCompletedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending();

  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  DiceTurnSyncOnHelper* dice_sync_starter = CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check that the primary account was set with IdentityManager, but the sync
  // confirmation dialog was not yet shown.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId());
  CheckDelegateCalls();

  // Simulate that sync startup has completed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(1);
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  dice_sync_starter->SyncStartupCompleted();
  CheckDelegateCalls();
}

// For enterprise user, tests that the user is signed in only after Sync engine
// fails to start.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest,
       ShowSyncDialogBlockedUntilSyncStartupFailedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending();

  // Signin flow.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  DiceTurnSyncOnHelper* dice_sync_starter = CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check that the primary account was added to the token service and in the
  // sign-in manager.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId());
  CheckDelegateCalls();

  // Simulate that sync startup has failed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetProfileSyncServiceMock()->GetUserSettingsMock(),
              SetFirstSetupComplete())
      .Times(1);
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  dice_sync_starter->SyncStartupFailed();
  CheckDelegateCalls();
}

// Checks that the profile can be deleted in the middle of the flow.
TEST_F(DiceTurnSyncOnHelperTest, ProfileDeletion) {
  run_delegate_callbacks_ = false;  // Delegate is hanging.

  // Show the enterprise confirmation dialog.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE;
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Delegate is now hanging at the enterprise confirmation dialog.
  // Dialog has been shown.
  EXPECT_EQ(kEmail, enterprise_confirmation_email());
  // But signin is not finished.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());

  // Delete the profile.
  ClearProfile();

  // DiceTurnSyncOnHelper was destroyed.
  EXPECT_TRUE(delegate_destroyed());
}
