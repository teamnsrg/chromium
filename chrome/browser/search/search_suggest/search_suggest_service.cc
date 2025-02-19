// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

constexpr char kSuggestionHashRegex[] = "[a-z0-9]{1,4}";

std::string* ValidateHash(const uint8_t hash[4]) {
  const std::string hash_string = reinterpret_cast<const char*>(hash);

  std::string* trimmed_string = new std::string("");
  // The uint8_t array received via IPC ends in an EOT byte (\4), remove it.
  base::TrimString(hash_string, "\4", trimmed_string);

  if (!re2::RE2::FullMatch(*trimmed_string, kSuggestionHashRegex))
    return nullptr;
  return trimmed_string;
}

const char kFirstShownTimeMs[] = "first_shown_time_ms";
const char kImpressionCapExpireTimeMs[] = "impression_cap_expire_time_ms";
const char kImpressionsCount[] = "impressions_count";
const char kIsRequestFrozen[] = "is_request_frozen";
const char kMaxImpressions[] = "max_impressions";
const char kRequestFreezeTimeMs[] = "request_freeze_time_ms";
const char kRequestFrozenTimeMs[] = "request_frozen_time_ms";

// Default value for max_impressions specified by the VASCO team.
const int kDefaultMaxImpressions = 4;

std::unique_ptr<base::DictionaryValue> ImpressionDictDefaults() {
  std::unique_ptr<base::DictionaryValue> defaults =
      std::make_unique<base::DictionaryValue>();
  defaults->SetInteger(kFirstShownTimeMs, 0);
  defaults->SetInteger(kImpressionCapExpireTimeMs, 0);
  defaults->SetInteger(kImpressionsCount, 0);
  defaults->SetBoolean(kIsRequestFrozen, false);
  defaults->SetInteger(kMaxImpressions, kDefaultMaxImpressions);
  defaults->SetInteger(kRequestFreezeTimeMs, 0);
  defaults->SetInteger(kRequestFrozenTimeMs, 0);
  return defaults;
}

}  // namespace

class SearchSuggestService::SigninObserver
    : public identity::IdentityManager::Observer {
 public:
  using SigninStatusChangedCallback = base::RepeatingClosure;

  SigninObserver(identity::IdentityManager* identity_manager,
                 const SigninStatusChangedCallback& callback)
      : identity_manager_(identity_manager), callback_(callback) {
    identity_manager_->AddObserver(this);
  }

  ~SigninObserver() override { identity_manager_->RemoveObserver(this); }

  bool SignedIn() {
    return !identity_manager_->GetAccountsInCookieJar()
                .signed_in_accounts.empty();
  }

 private:
  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    callback_.Run();
  }

  identity::IdentityManager* const identity_manager_;
  SigninStatusChangedCallback callback_;
};

SearchSuggestService::SearchSuggestService(
    Profile* profile,
    identity::IdentityManager* identity_manager,
    std::unique_ptr<SearchSuggestLoader> loader)
    : loader_(std::move(loader)),
      signin_observer_(std::make_unique<SigninObserver>(
          identity_manager,
          base::BindRepeating(&SearchSuggestService::SigninStatusChanged,
                              base::Unretained(this)))),
      profile_(profile) {}

SearchSuggestService::~SearchSuggestService() = default;

void SearchSuggestService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnSearchSuggestServiceShuttingDown();
  }

  signin_observer_.reset();
  DCHECK(!observers_.might_have_observers());
}

void SearchSuggestService::Refresh() {
  const std::string blocklist = GetBlocklistAsString();
  MaybeLoadWithBlocklist(blocklist);
}

void SearchSuggestService::MaybeLoadWithBlocklist(
    const std::string& blocklist) {
  if (!signin_observer_->SignedIn()) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::SIGNED_OUT,
                            base::nullopt);
  } else if (profile_->GetPrefs()->GetBoolean(
                 prefs::kNtpSearchSuggestionsOptOut)) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::OPTED_OUT,
                            base::nullopt);
  } else if (RequestsFrozen()) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::REQUESTS_FROZEN,
                            base::nullopt);
  } else if (ImpressionCapReached()) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::IMPRESSION_CAP,
                            base::nullopt);
  } else {
    loader_->Load(blocklist,
                  base::BindOnce(&SearchSuggestService::SearchSuggestDataLoaded,
                                 base::Unretained(this)));
  }
}

void SearchSuggestService::AddObserver(SearchSuggestServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchSuggestService::RemoveObserver(
    SearchSuggestServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchSuggestService::SigninStatusChanged() {
  // If we have cached data, clear it.
  if (search_suggest_data_.has_value()) {
    search_suggest_data_ = base::nullopt;
  }
}

void SearchSuggestService::SearchSuggestDataLoaded(
    SearchSuggestLoader::Status status,
    const base::Optional<SearchSuggestData>& data) {
  // In case of transient errors, keep our cached data (if any), but still
  // notify observers of the finished load (attempt).
  if (status != SearchSuggestLoader::Status::TRANSIENT_ERROR) {
    search_suggest_data_ = data;
    search_suggest_status_ = status;

    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpSearchSuggestionsImpressions);

    if (data.has_value()) {
      base::DictionaryValue* dict = update.Get();
      dict->SetInteger(kMaxImpressions, data->max_impressions);
      dict->SetInteger(kImpressionCapExpireTimeMs,
                       data->impression_cap_expire_time_ms);
      dict->SetInteger(kRequestFreezeTimeMs, data->request_freeze_time_ms);
    } else if (status == SearchSuggestLoader::Status::FATAL_ERROR) {
      base::DictionaryValue* dict = update.Get();
      dict->SetBoolean(kIsRequestFrozen, true);
      dict->SetInteger(kRequestFrozenTimeMs, base::Time::Now().ToTimeT());
    }
  }
  NotifyObservers();
}

void SearchSuggestService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnSearchSuggestDataUpdated();
  }
}

bool SearchSuggestService::ImpressionCapReached() {
  const base::DictionaryValue* dict = profile_->GetPrefs()->GetDictionary(
      prefs::kNtpSearchSuggestionsImpressions);

  int first_shown_time_ms = 0;
  int impression_cap_expire_time_ms = 0;
  int impression_count = 0;
  int max_impressions = 0;
  dict->GetInteger(kFirstShownTimeMs, &first_shown_time_ms);
  dict->GetInteger(kImpressionCapExpireTimeMs, &impression_cap_expire_time_ms);
  dict->GetInteger(kImpressionsCount, &impression_count);
  dict->GetInteger(kMaxImpressions, &max_impressions);

  int64_t time_delta =
      base::TimeDelta(base::Time::Now() -
                      base::Time::FromTimeT(first_shown_time_ms))
          .InMilliseconds();
  if (time_delta > impression_cap_expire_time_ms) {
    impression_count = 0;
    DictionaryPrefUpdate update(profile_->GetPrefs(),
                                prefs::kNtpSearchSuggestionsImpressions);
    update.Get()->SetInteger(kImpressionsCount, impression_count);
  }

  return impression_count >= max_impressions;
}

bool SearchSuggestService::RequestsFrozen() {
  const base::DictionaryValue* dict = profile_->GetPrefs()->GetDictionary(
      prefs::kNtpSearchSuggestionsImpressions);

  bool is_request_frozen = false;
  int request_freeze_time_ms = 0;
  int request_frozen_time_ms = 0;
  dict->GetBoolean(kIsRequestFrozen, &is_request_frozen);
  dict->GetInteger(kRequestFrozenTimeMs, &request_frozen_time_ms);
  dict->GetInteger(kRequestFreezeTimeMs, &request_freeze_time_ms);

  int64_t time_delta =
      base::TimeDelta(base::Time::Now() -
                      base::Time::FromTimeT(request_frozen_time_ms))
          .InMilliseconds();
  if (is_request_frozen) {
    if (time_delta < request_freeze_time_ms) {
      return true;
    } else {
      DictionaryPrefUpdate update(profile_->GetPrefs(),
                                  prefs::kNtpSearchSuggestionsImpressions);
      update.Get()->SetBoolean(kIsRequestFrozen, false);
    }
  }

  return false;
}

void SearchSuggestService::BlocklistSearchSuggestion(int task_version,
                                                     long task_id) {
  if (!search::DefaultSearchProviderIsGoogle(profile_))
    return;

  std::string task_version_id =
      std::to_string(task_version) + "_" + std::to_string(task_id);
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kNtpSearchSuggestionsBlocklist);
  base::DictionaryValue* blocklist = update.Get();
  blocklist->SetKey(task_version_id, base::ListValue());

  search_suggest_data_ = base::nullopt;
  Refresh();
}

void SearchSuggestService::BlocklistSearchSuggestionWithHash(
    int task_version,
    long task_id,
    const uint8_t hash[4]) {
  if (!search::DefaultSearchProviderIsGoogle(profile_))
    return;

  std::string* hash_string = ValidateHash(hash);

  if (!hash_string)
    return;

  std::string task_version_id =
      std::to_string(task_version) + "_" + std::to_string(task_id);

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kNtpSearchSuggestionsBlocklist);
  base::DictionaryValue* blocklist = update.Get();
  base::Value* value = blocklist->FindKey(task_version_id);
  if (!value)
    value = blocklist->SetKey(task_version_id, base::ListValue());
  value->GetList().emplace_back(base::Value(*hash_string));

  search_suggest_data_ = base::nullopt;
  Refresh();
}

void SearchSuggestService::SearchSuggestionSelected(int task_version,
                                                    long task_id,
                                                    const uint8_t hash[4]) {
  if (!search::DefaultSearchProviderIsGoogle(profile_))
    return;

  std::string* hash_string = ValidateHash(hash);

  if (!hash_string)
    return;

  std::string blocklist_item = std::to_string(task_version) + "_" +
                               std::to_string(task_id) + ":" + *hash_string;

  std::string blocklist = GetBlocklistAsString();
  if (!blocklist.empty())
    blocklist += ";";
  blocklist += blocklist_item;

  search_suggest_data_ = base::nullopt;
  MaybeLoadWithBlocklist(blocklist);
}

std::string SearchSuggestService::GetBlocklistAsString() {
  const base::DictionaryValue* blocklist = profile_->GetPrefs()->GetDictionary(
      prefs::kNtpSearchSuggestionsBlocklist);

  std::string blocklist_as_string;
  for (const auto& dict : blocklist->DictItems()) {
    blocklist_as_string += dict.first;

    if (!dict.second.GetList().empty()) {
      std::string list = ":";

      for (const auto& i : dict.second.GetList()) {
        list += i.GetString() + ",";
      }

      // Remove trailing comma.
      list.pop_back();
      blocklist_as_string += list;
    }

    blocklist_as_string += ";";
  }

  // Remove trailing semi-colon.
  if (!blocklist_as_string.empty())
    blocklist_as_string.pop_back();
  return blocklist_as_string;
}

void SearchSuggestService::SuggestionsDisplayed() {
  search_suggest_data_ = base::nullopt;

  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              prefs::kNtpSearchSuggestionsImpressions);
  base::DictionaryValue* dict = update.Get();

  int impression_count = 0;
  dict->GetInteger(kImpressionsCount, &impression_count);
  dict->SetInteger(kImpressionsCount, impression_count + 1);

  // When suggestions are displayed for the first time record the timestamp.
  if (impression_count == 0) {
    dict->SetInteger(kFirstShownTimeMs, base::Time::Now().ToTimeT());
  }
}

void SearchSuggestService::OptOutOfSearchSuggestions() {
  if (!search::DefaultSearchProviderIsGoogle(profile_))
    return;

  profile_->GetPrefs()->SetBoolean(prefs::kNtpSearchSuggestionsOptOut, true);

  search_suggest_data_ = base::nullopt;
}

// static
void SearchSuggestService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpSearchSuggestionsBlocklist);
  registry->RegisterDictionaryPref(prefs::kNtpSearchSuggestionsImpressions,
                                   ImpressionDictDefaults());
  registry->RegisterBooleanPref(prefs::kNtpSearchSuggestionsOptOut, false);
}
