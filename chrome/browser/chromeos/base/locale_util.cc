// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/locale_util.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font_skia.h"

namespace chromeos {

namespace {

struct SwitchLanguageData {
  SwitchLanguageData(const std::string& locale,
                     const bool enable_locale_keyboard_layouts,
                     const bool login_layouts_only,
                     const locale_util::SwitchLanguageCallback& callback,
                     Profile* profile)
      : callback(callback),
        result(locale, std::string(), false),
        enable_locale_keyboard_layouts(enable_locale_keyboard_layouts),
        login_layouts_only(login_layouts_only),
        profile(profile) {}

  const locale_util::SwitchLanguageCallback callback;

  locale_util::LanguageSwitchResult result;
  const bool enable_locale_keyboard_layouts;
  const bool login_layouts_only;
  Profile* profile;
};

// Runs on SequencedWorkerPool thread under PostTaskAndReply().
// So data is owned by "Reply" part of PostTaskAndReply() process.
void SwitchLanguageDoReloadLocale(SwitchLanguageData* data) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  data->result.loaded_locale =
      ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          data->result.requested_locale);

  data->result.success = !data->result.loaded_locale.empty();
}

// Callback after SwitchLanguageDoReloadLocale() back in UI thread.
void FinishSwitchLanguage(std::unique_ptr<SwitchLanguageData> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (data->result.success) {
    g_browser_process->SetApplicationLocale(data->result.loaded_locale);

    if (data->enable_locale_keyboard_layouts) {
      input_method::InputMethodManager* manager =
          input_method::InputMethodManager::Get();
      scoped_refptr<input_method::InputMethodManager::State> ime_state =
          UserSessionManager::GetInstance()->GetDefaultIMEState(data->profile);
      if (data->login_layouts_only) {
        // Enable the hardware keyboard layouts and locale-specific layouts
        // suitable for use on the login screen. This will also switch to the
        // first hardware keyboard layout since the input method currently in
        // use may not be supported by the new locale.
        ime_state->EnableLoginLayouts(
            data->result.loaded_locale,
            manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
      } else {
        // Enable all hardware keyboard layouts. This will also switch to the
        // first hardware keyboard layout.
        ime_state->ReplaceEnabledInputMethods(
            manager->GetInputMethodUtil()->GetHardwareInputMethodIds());

        // Enable all locale-specific layouts.
        std::vector<std::string> input_methods;
        manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
            data->result.loaded_locale,
            input_method::kKeyboardLayoutsOnly,
            &input_methods);
        for (std::vector<std::string>::const_iterator it =
                input_methods.begin(); it != input_methods.end(); ++it) {
          ime_state->EnableInputMethod(*it);
        }
      }
    }
  }

  // The font clean up of ResourceBundle should be done on UI thread, since the
  // cached fonts are thread unsafe.
  ui::ResourceBundle::GetSharedInstance().ReloadFonts();
  gfx::PlatformFontSkia::ReloadDefaultFont();
  if (!data->callback.is_null())
    data->callback.Run(data->result);
}

// Get parsed list of preferred languages from the 'kLanguagePreferredLanguages'
// setting.
std::vector<std::string> GetPreferredLanguagesList(const PrefService* prefs) {
  std::string preferred_languages_string =
      prefs->GetString(prefs::kLanguagePreferredLanguages);
  return base::SplitString(preferred_languages_string, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

}  // namespace

namespace locale_util {

constexpr const char* kAllowedUILanguageFallback = "en-US";

LanguageSwitchResult::LanguageSwitchResult(const std::string& requested_locale,
                                           const std::string& loaded_locale,
                                           bool success)
    : requested_locale(requested_locale),
      loaded_locale(loaded_locale),
      success(success) {
}

void SwitchLanguage(const std::string& locale,
                    const bool enable_locale_keyboard_layouts,
                    const bool login_layouts_only,
                    const SwitchLanguageCallback& callback,
                    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<SwitchLanguageData> data(
      new SwitchLanguageData(locale, enable_locale_keyboard_layouts,
                             login_layouts_only, callback, profile));
  base::Closure reloader(
      base::Bind(&SwitchLanguageDoReloadLocale, base::Unretained(data.get())));
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT}, reloader,
      base::Bind(&FinishSwitchLanguage, base::Passed(std::move(data))));
}

bool IsAllowedLanguage(const std::string& language, const PrefService* prefs) {
  const base::Value::ListStorage& allowed_languages =
      prefs->GetList(prefs::kAllowedLanguages)->GetList();

  // Empty list means all languages are allowed.
  if (allowed_languages.empty())
    return true;

  // Check if locale is in list of allowed UI locales.
  return base::ContainsValue(allowed_languages, base::Value(language));
}

bool IsAllowedUILanguage(const std::string& language,
                         const PrefService* prefs) {
  return IsAllowedLanguage(language, prefs) && IsNativeUILanguage(language);
}

bool IsNativeUILanguage(const std::string& locale) {
  std::string resolved_locale = locale;

  // The locale is a UI locale or can be converted to a UI locale.
  if (base::FeatureList::IsEnabled(translate::kRegionalLocalesAsDisplayUI))
    return language::ConvertToActualUILocale(&resolved_locale);
  return language::ConvertToFallbackUILocale(&resolved_locale);
}

void RemoveDisallowedLanguagesFromPreferred(PrefService* prefs) {
  // Do nothing if all languages are allowed
  if (prefs->GetList(prefs::kAllowedLanguages)->GetList().empty())
    return;

  std::vector<std::string> preferred_languages =
      GetPreferredLanguagesList(prefs);
  std::vector<std::string> updated_preferred_languages;
  bool have_ui_language = false;
  for (const std::string& language : preferred_languages) {
    if (IsAllowedLanguage(language, prefs)) {
      updated_preferred_languages.push_back(language);
      if (IsNativeUILanguage(language))
        have_ui_language = true;
    }
  }
  if (!have_ui_language)
    updated_preferred_languages.push_back(GetAllowedFallbackUILanguage(prefs));

  // Do not set setting if it did not change to not cause the update callback
  if (preferred_languages != updated_preferred_languages) {
    prefs->SetString(prefs::kLanguagePreferredLanguages,
                     base::JoinString(updated_preferred_languages, ","));
  }
}

std::string GetAllowedFallbackUILanguage(const PrefService* prefs) {
  // Check the user's preferred languages if one of them is an allowed UI
  // locale.
  std::string preferred_languages_string =
      prefs->GetString(prefs::kLanguagePreferredLanguages);
  std::vector<std::string> preferred_languages =
      GetPreferredLanguagesList(prefs);
  for (const std::string& language : preferred_languages) {
    if (IsAllowedUILanguage(language, prefs))
      return language;
  }

  // Check the allowed UI locales and return the first valid entry.
  const base::Value::ListStorage& allowed_languages =
      prefs->GetList(prefs::kAllowedLanguages)->GetList();
  for (const base::Value& value : allowed_languages) {
    const std::string& locale = value.GetString();
    if (IsAllowedUILanguage(locale, prefs))
      return locale;
  }

  // default fallback
  return kAllowedUILanguageFallback;
}

bool AddLocaleToPreferredLanguages(const std::string& locale,
                                   PrefService* prefs) {
  std::string preferred_languages_string =
      prefs->GetString(prefs::kLanguagePreferredLanguages);
  std::vector<std::string> preferred_languages =
      base::SplitString(preferred_languages_string, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (!base::ContainsValue(preferred_languages, locale)) {
    preferred_languages.push_back(locale);
    prefs->SetString(prefs::kLanguagePreferredLanguages,
                     base::JoinString(preferred_languages, ","));
    return true;
  }

  return false;
}

}  // namespace locale_util
}  // namespace chromeos
