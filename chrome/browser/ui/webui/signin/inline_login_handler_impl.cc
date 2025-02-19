// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_impl.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper_delegate_impl.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/about_signin_internals.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_investigator.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"

#endif  // defined(OS_WIN)

namespace {

// Specific implementation of DiceTurnSyncOnHelper::Delegate for forced signin
// flows. Some confirmation prompts are skipped.
class ForcedSigninDiceTurnSyncOnHelperDelegate
    : public DiceTurnSyncOnHelperDelegateImpl {
 public:
  explicit ForcedSigninDiceTurnSyncOnHelperDelegate(Browser* browser)
      : DiceTurnSyncOnHelperDelegateImpl(browser) {}

 private:
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    NOTREACHED();
  }

  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override {
    std::move(callback).Run(
        DiceTurnSyncOnHelper ::SigninChoice::SIGNIN_CHOICE_CONTINUE);
  }
};

#if defined(OS_WIN)

// Returns a list of valid signin domains that were passed in
// |email_domains_parameter| as an argument to the gcpw signin dialog.

std::vector<std::string> GetEmailDomainsFromParameter(
    const std::string& email_domains_parameter) {
  return base::SplitString(base::ToLowerASCII(email_domains_parameter),
                           credential_provider::kEmailDomainsSeparator,
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Validates that the |signin_gaia_id| that the user signed in with matches
// the |gaia_id_parameter| passed to the gcpw signin dialog. Also ensures
// that the |signin_email| is in a domain listed in |email_domains_parameter|.
// Returns kUiecSuccess on success.
// Returns the appropriate error code on failure.
credential_provider::UiExitCodes ValidateSigninEmail(
    const std::string& gaia_id_parameter,
    const std::string& email_domains_parameter,
    const std::string& signin_email,
    const std::string& signin_gaia_id) {
  if (!gaia_id_parameter.empty() &&
      !base::LowerCaseEqualsASCII(gaia_id_parameter, signin_gaia_id)) {
    return credential_provider::kUiecEMailMissmatch;
  }

  if (email_domains_parameter.empty())
    return credential_provider::kUiecSuccess;

  std::vector<std::string> all_email_domains =
      GetEmailDomainsFromParameter(email_domains_parameter);
  std::string email_domain = gaia::ExtractDomainName(signin_email);

  return std::find(all_email_domains.begin(), all_email_domains.end(),
                   email_domain) != all_email_domains.end()
             ? credential_provider::kUiecSuccess
             : credential_provider::kUiecInvalidEmailDomain;
}

#endif

void LogHistogramValue(signin_metrics::AccessPointAction action) {
  UMA_HISTOGRAM_ENUMERATION("Signin.AllAccessPointActions", action,
                            signin_metrics::HISTOGRAM_MAX);
}

// Returns true if |profile| is a system profile or created from one.
bool IsSystemProfile(Profile* profile) {
  return profile->GetOriginalProfile()->IsSystemProfile();
}

void RedirectToNtpOrAppsPage(content::WebContents* contents,
                             signin_metrics::AccessPoint access_point) {
  // Do nothing if a navigation is pending, since this call can be triggered
  // from DidStartLoading. This avoids deleting the pending entry while we are
  // still navigating to it. See crbug/346632.
  if (contents->GetController().GetPendingEntry())
    return;

  VLOG(1) << "RedirectToNtpOrAppsPage";
  // Redirect to NTP/Apps page and display a confirmation bubble
  GURL url(access_point ==
                   signin_metrics::AccessPoint::ACCESS_POINT_APPS_PAGE_LINK
               ? chrome::kChromeUIAppsURL
               : chrome::kChromeUINewTabURL);
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
  contents->OpenURL(params);
}

void RedirectToNtpOrAppsPageIfNecessary(
    content::WebContents* contents,
    signin_metrics::AccessPoint access_point) {
  if (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS)
    RedirectToNtpOrAppsPage(contents, access_point);
}

void CloseModalSigninIfNeeded(InlineLoginHandlerImpl* handler) {
  if (handler) {
    Browser* browser = handler->GetDesktopBrowser();
    if (browser)
      browser->signin_view_controller()->CloseModalSignin();
  }
}

void SetProfileLocked(const base::FilePath profile_path, bool locked) {
  if (!profile_path.empty()) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    if (profile_manager) {
      ProfileAttributesEntry* entry;
      if (profile_manager->GetProfileAttributesStorage()
              .GetProfileAttributesWithPath(profile_path, &entry)) {
        if (locked)
          entry->LockForceSigninProfile(true);
        else
          entry->SetIsSigninRequired(false);
      }
    }
  }
}

void UnlockProfileAndHideLoginUI(const base::FilePath profile_path,
                                 InlineLoginHandlerImpl* handler) {
  SetProfileLocked(profile_path, false);
  if (handler)
    handler->CloseDialogFromJavascript();
  UserManager::Hide();
}

void LockProfileAndShowUserManager(const base::FilePath& profile_path) {
  SetProfileLocked(profile_path, true);
  UserManager::Show(profile_path,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
}

// Returns true if the showAccountManagement parameter in the given url is set
// to true.
bool ShouldShowAccountManagement(const GURL& url, bool is_mirror_enabled) {
  if (!is_mirror_enabled)
    return false;

  std::string value;
  if (net::GetValueForKeyInQuery(url, kSignInPromoQueryKeyShowAccountManagement,
                                 &value)) {
    int enabled = 0;
    if (base::StringToInt(value, &enabled) && enabled == 1)
      return true;
  }
  return false;
}

// Callback for DiceTurnOnSyncHelper.
void OnSyncSetupComplete(Profile* profile,
                         base::WeakPtr<InlineLoginHandlerImpl> handler,
                         const std::string& username,
                         const std::string& password) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool has_primary_account = identity_manager->HasPrimaryAccount();
  if (has_primary_account && !password.empty()) {
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(profile,
                                            ServiceAccessType::EXPLICIT_ACCESS);
    password_store->SaveGaiaPasswordHash(
        username, base::UTF8ToUTF16(password),
        password_manager::metrics_util::SyncPasswordHashChange::
            SAVED_ON_CHROME_SIGNIN);

    if (profiles::IsLockAvailable(profile))
      LocalAuth::SetLocalAuthCredentials(profile, password);
  }

  if (handler) {
    handler->SyncStarterCallback(has_primary_account);
  } else if (signin_util::IsForceSigninEnabled() && !has_primary_account) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile, base::Bind(&LockProfileAndShowUserManager),
        // Cannot be called because skip_beforeunload is true.
        BrowserList::CloseCallback(),
        /*skip_beforeunload=*/true);
  }
}

}  // namespace

InlineSigninHelper::InlineSigninHelper(
    base::WeakPtr<InlineLoginHandlerImpl> handler,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    Profile::CreateStatus create_status,
    const GURL& current_url,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    const std::string& signin_scoped_device_id,
    bool confirm_untrusted_signin,
    bool is_force_sign_in_with_usermanager)
    : gaia_auth_fetcher_(this, gaia::GaiaSource::kChrome, url_loader_factory),
      handler_(handler),
      profile_(profile),
      create_status_(create_status),
      current_url_(current_url),
      email_(email),
      gaia_id_(gaia_id),
      password_(password),
      auth_code_(auth_code),
      confirm_untrusted_signin_(confirm_untrusted_signin),
      is_force_sign_in_with_usermanager_(is_force_sign_in_with_usermanager) {
  DCHECK(profile_);
  DCHECK(!email_.empty());
  DCHECK(!auth_code_.empty());

  gaia_auth_fetcher_.StartAuthCodeForOAuth2TokenExchangeWithDeviceId(
      auth_code_, signin_scoped_device_id);
}

InlineSigninHelper::~InlineSigninHelper() {}

void InlineSigninHelper::OnClientOAuthSuccess(const ClientOAuthResult& result) {
  if (is_force_sign_in_with_usermanager_) {
    // If user sign in in UserManager with force sign in enabled, the browser
    // window won't be opened until now.
    UnlockProfileAndHideLoginUI(profile_->GetPath(), handler_.get());
    profiles::OpenBrowserWindowForProfile(
        base::Bind(&InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened,
                   base::Unretained(this), result),
        true, false, true, profile_, create_status_);
  } else {
    OnClientOAuthSuccessAndBrowserOpened(result, profile_, create_status_);
  }
}

void InlineSigninHelper::OnClientOAuthSuccessAndBrowserOpened(
    const ClientOAuthResult& result,
    Profile* profile,
    Profile::CreateStatus status) {
  Browser* browser = nullptr;
  if (handler_)
    browser = handler_->GetDesktopBrowser();

  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url_);
  if (reason == signin_metrics::Reason::REASON_FETCH_LST_ONLY) {
// Constants are only available on Windows for the Google Credential
// Provider for Windows. Other platforms will just close the dialog here.
#if defined(OS_WIN)
    std::string json_retval;
    base::Value args(base::Value::Type::DICTIONARY);
    args.SetKey(credential_provider::kKeyEmail, base::Value(email_));
    args.SetKey(credential_provider::kKeyPassword, base::Value(password_));
    args.SetKey(credential_provider::kKeyId, base::Value(gaia_id_));
    args.SetKey(credential_provider::kKeyRefreshToken,
                base::Value(result.refresh_token));
    args.SetKey(credential_provider::kKeyAccessToken,
                base::Value(result.access_token));

    handler_->SendLSTFetchResultsMessage(args);
#else
    if (handler_)
      handler_->CloseDialogFromJavascript();
#endif  // defined(OS_WIN)
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile_);
  about_signin_internals->OnRefreshTokenReceived("Successful");

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  // Seed the account with this combination of gaia id/display email.
  AccountInfo account_info;
  account_info.gaia = gaia_id_;
  account_info.email = email_;
  identity_manager->LegacySeedAccountInfo(account_info);

  std::string primary_email = identity_manager->GetPrimaryAccountInfo().email;
  if (gaia::AreEmailsSame(email_, primary_email) &&
      (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
       reason == signin_metrics::Reason::REASON_UNLOCK) &&
      !password_.empty() && profiles::IsLockAvailable(profile_)) {
    LocalAuth::SetLocalAuthCredentials(profile_, password_);
  }

#if defined(SYNC_PASSWORD_REUSE_DETECTION_ENABLED)
  if (!password_.empty()) {
    scoped_refptr<password_manager::PasswordStore> password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS);
    if (password_store && !primary_email.empty()) {
      password_store->SaveGaiaPasswordHash(
          primary_email, base::UTF8ToUTF16(password_),
          password_manager::metrics_util::SyncPasswordHashChange::
              SAVED_ON_CHROME_SIGNIN);
    }
  }
#endif

  if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
      reason == signin_metrics::Reason::REASON_UNLOCK ||
      reason == signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT) {
    identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
        gaia_id_, email_, result.refresh_token,
        result.is_under_advanced_protection,
        signin_metrics::SourceForRefreshTokenOperation::
            kInlineLoginHandler_Signin);

    if (signin::IsAutoCloseEnabledInEmbeddedURL(current_url_)) {
      // Close the gaia sign in tab via a task to make sure we aren't in the
      // middle of any webui handler code.
      bool show_account_management = ShouldShowAccountManagement(
          current_url_,
          AccountConsistencyModeManager::IsMirrorEnabledForProfile(profile_));
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&InlineLoginHandlerImpl::CloseTab, handler_,
                                    show_account_management));
    }

    if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION ||
        reason == signin_metrics::Reason::REASON_UNLOCK) {
      identity_manager->GetPrimaryAccountMutator()
          ->LegacyMergeSigninCredentialIntoCookieJar();
    }
    LogSigninReason(reason);
  } else {
    if (confirm_untrusted_signin_) {
      // Display a confirmation dialog to the user.
      base::RecordAction(
          base::UserMetricsAction("Signin_Show_UntrustedSigninPrompt"));
      if (!browser)
        browser = chrome::FindLastActiveWithProfile(profile_);
      browser->window()->ShowOneClickSigninConfirmation(
          base::UTF8ToUTF16(email_),
          base::BindOnce(&InlineSigninHelper::UntrustedSigninConfirmed,
                         base::Unretained(this), result.refresh_token));
      return;
    }
    CreateSyncStarter(browser, current_url_, result.refresh_token);
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }
}

void InlineSigninHelper::UntrustedSigninConfirmed(
    const std::string& refresh_token,
    bool confirmed) {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  if (confirmed) {
    CreateSyncStarter(nullptr, current_url_, refresh_token);
    return;
  }

  base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
  if (handler_) {
    handler_->SyncStarterCallback(false);
  } else if (signin_util::IsForceSigninEnabled()) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile_, base::Bind(&LockProfileAndShowUserManager),
        // Cannot be called because  skip_beforeunload is true.
        BrowserList::CloseCallback(),
        /*skip_beforeunload=*/true);
  }
}

void InlineSigninHelper::CreateSyncStarter(Browser* browser,
                                           const GURL& current_url,
                                           const std::string& refresh_token) {
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  if (identity_manager->HasPrimaryAccount()) {
    // Already signed in, nothing to do.
    if (handler_)
      handler_->SyncStarterCallback(true);
    return;
  }

  if (!browser)
    browser = chrome::OpenEmptyWindow(profile_);

  std::string account_id =
      identity_manager->GetAccountsMutator()->AddOrUpdateAccount(
          gaia_id_, email_, refresh_token,
          /*is_under_advanced_protection=*/false,
          signin_metrics::SourceForRefreshTokenOperation::
              kInlineLoginHandler_Signin);

  std::unique_ptr<DiceTurnSyncOnHelper::Delegate> delegate =
      signin_util::IsForceSigninEnabled()
          ? std::make_unique<ForcedSigninDiceTurnSyncOnHelperDelegate>(browser)
          : std::make_unique<DiceTurnSyncOnHelperDelegateImpl>(browser);

  new DiceTurnSyncOnHelper(
      profile_, signin::GetAccessPointForEmbeddedPromoURL(current_url),
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin::GetSigninReasonForEmbeddedPromoURL(current_url), account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT,
      std::move(delegate),
      base::BindOnce(&OnSyncSetupComplete, profile_, handler_, email_,
                     password_));
}

void InlineSigninHelper::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  if (handler_)
    handler_->HandleLoginError(error.ToString(), base::string16());

  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url_);
  if (reason != signin_metrics::Reason::REASON_FETCH_LST_ONLY) {
    AboutSigninInternals* about_signin_internals =
        AboutSigninInternalsFactory::GetForProfile(profile_);
    about_signin_internals->OnRefreshTokenReceived("Failure");
  }

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

InlineLoginHandlerImpl::InlineLoginHandlerImpl()
    : confirm_untrusted_signin_(false), weak_factory_(this) {}

InlineLoginHandlerImpl::~InlineLoginHandlerImpl() {}

// This method is not called with webview sign in enabled.
void InlineLoginHandlerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!web_contents() || !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  // Returns early if this is not a gaia webview navigation.
  content::RenderFrameHost* gaia_frame =
      signin::GetAuthFrame(web_contents(), "signin-frame");
  if (navigation_handle->GetRenderFrameHost() != gaia_frame)
    return;

  // Loading any untrusted (e.g., HTTP) URLs in the privileged sign-in process
  // will require confirmation before the sign in takes effect.
  const GURL kGaiaExtOrigin(
      GaiaUrls::GetInstance()->signin_completed_continue_url().GetOrigin());
  if (!navigation_handle->GetURL().is_empty()) {
    GURL origin(navigation_handle->GetURL().GetOrigin());
    if (navigation_handle->GetURL().spec() != url::kAboutBlankURL &&
        origin != kGaiaExtOrigin && !gaia::IsGaiaSignonRealm(origin)) {
      confirm_untrusted_signin_ = true;
    }
  }
}

// static
void InlineLoginHandlerImpl::SetExtraInitParams(base::DictionaryValue& params) {
  params.SetString("service", "chromiumsync");

  // If this was called from the user manager to reauthenticate the profile,
  // make sure the webui is aware.
  Profile* profile = Profile::FromWebUI(web_ui());
  if (IsSystemProfile(profile))
    params.SetBoolean("dontResizeNonEmbeddedPages", true);

  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url);

  const GURL& url = GaiaUrls::GetInstance()->embedded_signin_url();
  params.SetBoolean("isNewGaiaFlow", true);
  params.SetString("clientId",
                   GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  params.SetString("gaiaPath", url.path().substr(1));

#if defined(OS_WIN)
  if (reason == signin_metrics::Reason::REASON_FETCH_LST_ONLY) {
    std::string email_domains;
    if (net::GetValueForKeyInQuery(
            current_url, credential_provider::kEmailDomainsSigninPromoParameter,
            &email_domains)) {
      std::vector<std::string> all_email_domains =
          GetEmailDomainsFromParameter(email_domains);
      if (all_email_domains.size() == 1)
        params.SetString("emailDomain", all_email_domains[0]);
    }

    // Prevent opening a new window if the embedded page fails to load.
    // This will keep the user from being able to access a fully functional
    // Chrome window in incognito mode.
    params.SetBoolean("dontResizeNonEmbeddedPages", true);

    GURL windows_url = GaiaUrls::GetInstance()->embedded_setup_windows_url();
    // Redirect to specified gaia endpoint path for GCPW:
    std::string windows_endpoint_path = windows_url.path().substr(1);
    // Redirect to specified gaia endpoint path for GCPW:
    std::string gcpw_endpoint_path;
    if (net::GetValueForKeyInQuery(
            current_url, credential_provider::kGcpwEndpointPathPromoParameter,
            &gcpw_endpoint_path)) {
      windows_endpoint_path = gcpw_endpoint_path;
    }
    params.SetString("gaiaPath", windows_endpoint_path);
  }
#endif

  std::string flow;
  switch (reason) {
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
      flow = "addaccount";
      break;
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNLOCK:
      flow = "reauth";
      break;
    case signin_metrics::Reason::REASON_FORCED_SIGNIN_PRIMARY_ACCOUNT:
      flow = "enterprisefsi";
      break;
    default:
      flow = "signin";
      break;
  }
  params.SetString("flow", flow);

  content::WebContentsObserver::Observe(contents);
  LogHistogramValue(signin_metrics::HISTOGRAM_SHOWN);
}

void InlineLoginHandlerImpl::CompleteLogin(const std::string& email,
                                           const std::string& password,
                                           const std::string& gaia_id,
                                           const std::string& auth_code,
                                           bool skip_for_now,
                                           bool trusted,
                                           bool trusted_found,
                                           bool choose_what_to_sync) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();

  if (skip_for_now) {
    signin::SetUserSkippedPromo(Profile::FromWebUI(web_ui()));
    SyncStarterCallback(false);
    return;
  }

  // This value exists only for webview sign in.
  if (trusted_found)
    confirm_untrusted_signin_ = !trusted;

  DCHECK(!email.empty());
  DCHECK(!gaia_id.empty());
  DCHECK(!auth_code.empty());

  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(
          contents->GetBrowserContext(), signin::GetSigninPartitionURL());

  // If this was called from the user manager to reauthenticate the profile,
  // the current profile is the system profile.  In this case, use the email to
  // find the right profile to reauthenticate.  Otherwise the profile can be
  // taken from web_ui().
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url);

  Profile* profile = Profile::FromWebUI(web_ui());
  if (reason != signin_metrics::Reason::REASON_FETCH_LST_ONLY &&
      IsSystemProfile(profile)) {
    ProfileManager* manager = g_browser_process->profile_manager();
    base::FilePath path = profiles::GetPathOfProfileWithEmail(manager, email);
    if (path.empty()) {
      path = UserManager::GetSigninProfilePath();
    }
    if (!path.empty()) {
      // If we are only reauthenticating a profile in the user manager (and not
      // unlocking it), load the profile and finish the login.
      if (reason == signin_metrics::Reason::REASON_REAUTHENTICATION) {
        FinishCompleteLoginParams params(
            this, partition, current_url, base::FilePath(),
            confirm_untrusted_signin_, email, gaia_id, password, auth_code,
            choose_what_to_sync, false);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        profiles::LoadProfileAsync(path, callback);
      } else {
        // Otherwise, switch to the profile and finish the login. Pass the
        // profile path so it can be marked as unlocked. Don't pass a handler
        // pointer since it will be destroyed before the callback runs.
        bool is_force_signin_enabled = signin_util::IsForceSigninEnabled();
        InlineLoginHandlerImpl* handler = nullptr;
        if (is_force_signin_enabled)
          handler = this;
        FinishCompleteLoginParams params(
            handler, partition, current_url, path, confirm_untrusted_signin_,
            email, gaia_id, password, auth_code, choose_what_to_sync,
            is_force_signin_enabled);
        ProfileManager::CreateCallback callback =
            base::Bind(&InlineLoginHandlerImpl::FinishCompleteLogin, params);
        if (is_force_signin_enabled) {
          // Browser window will be opened after ClientOAuthSuccess.
          profiles::LoadProfileAsync(path, callback);
        } else {
          profiles::SwitchToProfile(path, true, callback,
                                    ProfileMetrics::SWITCH_PROFILE_UNLOCK);
        }
      }
    }
  } else {
    FinishCompleteLogin(FinishCompleteLoginParams(
                            this, partition, current_url, base::FilePath(),
                            confirm_untrusted_signin_, email, gaia_id, password,
                            auth_code, choose_what_to_sync, false),
                        profile, Profile::CREATE_STATUS_CREATED);
  }
}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    InlineLoginHandlerImpl* handler,
    content::StoragePartition* partition,
    const GURL& url,
    const base::FilePath& profile_path,
    bool confirm_untrusted_signin,
    const std::string& email,
    const std::string& gaia_id,
    const std::string& password,
    const std::string& auth_code,
    bool choose_what_to_sync,
    bool is_force_sign_in_with_usermanager)
    : handler(handler),
      partition(partition),
      url(url),
      profile_path(profile_path),
      confirm_untrusted_signin(confirm_untrusted_signin),
      email(email),
      gaia_id(gaia_id),
      password(password),
      auth_code(auth_code),
      choose_what_to_sync(choose_what_to_sync),
      is_force_sign_in_with_usermanager(is_force_sign_in_with_usermanager) {}

InlineLoginHandlerImpl::FinishCompleteLoginParams::FinishCompleteLoginParams(
    const FinishCompleteLoginParams& other) = default;

InlineLoginHandlerImpl::FinishCompleteLoginParams::
    ~FinishCompleteLoginParams() {}

// static
void InlineLoginHandlerImpl::FinishCompleteLogin(
    const FinishCompleteLoginParams& params,
    Profile* profile,
    Profile::CreateStatus status) {
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(params.url);

  std::string default_email;
  net::GetValueForKeyInQuery(params.url, "email", &default_email);
  std::string validate_email;
  net::GetValueForKeyInQuery(params.url, "validateEmail", &validate_email);

#if defined(OS_WIN)
  if (reason == signin_metrics::Reason::REASON_FETCH_LST_ONLY) {
    std::string validate_gaia_id;
    net::GetValueForKeyInQuery(
        params.url, credential_provider::kValidateGaiaIdSigninPromoParameter,
        &validate_gaia_id);
    std::string email_domains;
    net::GetValueForKeyInQuery(
        params.url, credential_provider::kEmailDomainsSigninPromoParameter,
        &email_domains);
    credential_provider::UiExitCodes exit_code = ValidateSigninEmail(
        validate_gaia_id, email_domains, params.email, params.gaia_id);
    if (exit_code != credential_provider::kUiecSuccess) {
      if (params.handler) {
        params.handler->HandleLoginError(base::NumberToString((int)exit_code),
                                         base::UTF8ToUTF16(params.email));
      }
      return;
    } else {
      // Validation has already been done for GCPW, so clear the validate
      // argument so it doesn't validate again. GCPW validation allows the
      // signin email to not match the email given in the request url if the
      // gaia id of the signin email matches the one given in the request url.
      validate_email.clear();
    }
  }
#endif

  // When doing a SAML sign in, this email check may result in a false
  // positive.  This happens when the user types one email address in the
  // gaia sign in page, but signs in to a different account in the SAML sign in
  // page.
  if (validate_email == "1" && !default_email.empty()) {
    if (!gaia::AreEmailsSame(params.email, default_email)) {
      if (params.handler) {
        params.handler->HandleLoginError(
            l10n_util::GetStringFUTF8(IDS_SYNC_WRONG_EMAIL,
                                      base::UTF8ToUTF16(default_email)),
            base::UTF8ToUTF16(params.email));
      }
      return;
    }
  }

  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForEmbeddedPromoURL(params.url);
  LogHistogramValue(signin_metrics::HISTOGRAM_ACCEPTED);
  bool switch_to_advanced =
      params.choose_what_to_sync &&
      (access_point != signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS);
  LogHistogramValue(switch_to_advanced
                        ? signin_metrics::HISTOGRAM_WITH_ADVANCED
                        : signin_metrics::HISTOGRAM_WITH_DEFAULTS);

  CanOfferSigninType can_offer_for = CAN_OFFER_SIGNIN_FOR_ALL_ACCOUNTS;
  switch (reason) {
    case signin_metrics::Reason::REASON_ADD_SECONDARY_ACCOUNT:
      can_offer_for = CAN_OFFER_SIGNIN_FOR_SECONDARY_ACCOUNT;
      break;
    case signin_metrics::Reason::REASON_REAUTHENTICATION:
    case signin_metrics::Reason::REASON_UNLOCK: {
      std::string primary_username =
          IdentityManagerFactory::GetForProfile(profile)
              ->GetPrimaryAccountInfo()
              .email;
      if (!gaia::AreEmailsSame(default_email, primary_username))
        can_offer_for = CAN_OFFER_SIGNIN_FOR_SECONDARY_ACCOUNT;
      break;
    }
    default:
      // No need to change |can_offer_for|.
      break;
  }

  std::string error_msg;
  bool can_offer = reason == signin_metrics::Reason::REASON_FETCH_LST_ONLY ||
                   CanOfferSignin(profile, can_offer_for, params.gaia_id,
                                  params.email, &error_msg);
  if (!can_offer) {
    if (params.handler) {
      params.handler->HandleLoginError(error_msg,
                                       base::UTF8ToUTF16(params.email));
    }
    return;
  }

  AboutSigninInternals* about_signin_internals =
      AboutSigninInternalsFactory::GetForProfile(profile);
  if (about_signin_internals)
    about_signin_internals->OnAuthenticationResultReceived("Successful");

  std::string signin_scoped_device_id =
      GetSigninScopedDeviceIdForProfile(profile);
  base::WeakPtr<InlineLoginHandlerImpl> handler_weak_ptr;
  if (params.handler)
    handler_weak_ptr = params.handler->GetWeakPtr();

  // InlineSigninHelper will delete itself.
  new InlineSigninHelper(
      handler_weak_ptr,
      params.partition->GetURLLoaderFactoryForBrowserProcess(), profile, status,
      params.url, params.email, params.gaia_id, params.password,
      params.auth_code, signin_scoped_device_id,
      params.confirm_untrusted_signin,
      params.is_force_sign_in_with_usermanager);

  // If opened from user manager to unlock a profile, make sure the user manager
  // is closed and that the profile is marked as unlocked.
  if (reason != signin_metrics::Reason::REASON_FETCH_LST_ONLY &&
      !params.is_force_sign_in_with_usermanager) {
    UnlockProfileAndHideLoginUI(params.profile_path, params.handler);
  }
}

void InlineLoginHandlerImpl::HandleLoginError(const std::string& error_msg,
                                              const base::string16& email) {
  content::WebContents* contents = web_ui()->GetWebContents();
  const GURL& current_url = contents->GetURL();
  signin_metrics::Reason reason =
      signin::GetSigninReasonForEmbeddedPromoURL(current_url);

  if (reason == signin_metrics::Reason::REASON_FETCH_LST_ONLY) {
    base::Value error_value(base::Value::Type::DICTIONARY);
#if defined(OS_WIN)
    // If the message is an integer error code, send it as part
    // of the result.
    int exit_code = 0;
    if (base::StringToInt(error_msg, &exit_code)) {
      error_value.SetKey(credential_provider::kKeyExitCode,
                         base::Value(exit_code));
    }
#endif
    SendLSTFetchResultsMessage(error_value);
    return;
  }
  SyncStarterCallback(false);
  Browser* browser = GetDesktopBrowser();
  Profile* profile = Profile::FromWebUI(web_ui());

  if (IsSystemProfile(profile))
    profile = g_browser_process->profile_manager()->GetProfileByPath(
        UserManager::GetSigninProfilePath());
  CloseModalSigninIfNeeded(this);
  if (!error_msg.empty()) {
    LoginUIServiceFactory::GetForProfile(profile)->DisplayLoginResult(
        browser, base::UTF8ToUTF16(error_msg), email);
  }
}

void InlineLoginHandlerImpl::SendLSTFetchResultsMessage(
    const base::Value& arg) {
  if (IsJavascriptAllowed())
    CallJavascriptFunction("inline.login.sendLSTFetchResults", arg);
}

Browser* InlineLoginHandlerImpl::GetDesktopBrowser() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  if (!browser)
    browser = chrome::FindLastActiveWithProfile(Profile::FromWebUI(web_ui()));
  return browser;
}

void InlineLoginHandlerImpl::SyncStarterCallback(bool sync_setup_success) {
  content::WebContents* contents = web_ui()->GetWebContents();

  if (contents->GetController().GetPendingEntry()) {
    // Do nothing if a navigation is pending, since this call can be triggered
    // from DidStartLoading. This avoids deleting the pending entry while we are
    // still navigating to it. See crbug/346632.
    return;
  }

  const GURL& current_url = contents->GetLastCommittedURL();
  signin_metrics::AccessPoint access_point =
      signin::GetAccessPointForEmbeddedPromoURL(current_url);
  bool auto_close = signin::IsAutoCloseEnabledInEmbeddedURL(current_url);

  if (!sync_setup_success) {
    RedirectToNtpOrAppsPage(contents, access_point);
  } else if (auto_close) {
    bool show_account_management = ShouldShowAccountManagement(
        current_url, AccountConsistencyModeManager::IsMirrorEnabledForProfile(
                         Profile::FromWebUI(web_ui())));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&InlineLoginHandlerImpl::CloseTab,
                       weak_factory_.GetWeakPtr(), show_account_management));
  } else {
    RedirectToNtpOrAppsPageIfNecessary(contents, access_point);
  }
}

void InlineLoginHandlerImpl::CloseTab(bool show_account_management) {
  content::WebContents* tab = web_ui()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(tab);
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    if (tab_strip_model) {
      int index = tab_strip_model->GetIndexOfWebContents(tab);
      if (index != TabStripModel::kNoTab) {
        tab_strip_model->ExecuteContextMenuCommand(
            index, TabStripModel::CommandCloseTab);
      }
    }

    if (show_account_management) {
      browser->window()->ShowAvatarBubbleFromAvatarButton(
          BrowserWindow::AVATAR_BUBBLE_MODE_ACCOUNT_MANAGEMENT,
          signin::ManageAccountsParams(),
          signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN,
          false);
    }
  }
}
