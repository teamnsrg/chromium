// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/test/test_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win_test_data.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/views/controls/webview/web_dialog_view.h"
#include "ui/views/test/widget_test.h"

#define TEST_TASK_FUNC_NAME_FOR_THREAD_RUNNER(Func) Run##Func##Test

class CredentialProviderSigninDialogWinIntegrationTest;

class SigninDialogLoadingStoppedObserver : public content::WebContentsObserver {
 public:
  SigninDialogLoadingStoppedObserver(content::WebContents* web_contents,
                                     base::OnceClosure idle_closure)
      : content::WebContentsObserver(web_contents),
        idle_closure_(std::move(idle_closure)) {}

  void DidStopLoading() override {
    if (idle_closure_)
      std::move(idle_closure_).Run();
  }

  base::OnceClosure idle_closure_;
};

class CredentialProviderSigninDialogWinBaseTest : public InProcessBrowserTest {
 protected:
  CredentialProviderSigninDialogWinBaseTest();

  content::WebContents* web_contents() { return web_contents_; }
  virtual void WaitForDialogToLoad();

  content::WebContents* web_contents_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialProviderSigninDialogWinBaseTest);
};

CredentialProviderSigninDialogWinBaseTest::
    CredentialProviderSigninDialogWinBaseTest()
    : InProcessBrowserTest() {}

void CredentialProviderSigninDialogWinBaseTest::WaitForDialogToLoad() {
  EXPECT_TRUE(web_contents());

  base::RunLoop run_loop;
  SigninDialogLoadingStoppedObserver observer(web_contents(),
                                              run_loop.QuitWhenIdleClosure());
  run_loop.Run();
}

///////////////////////////////////////////////////////////////////////////////
// CredentialProviderSigninDialogWinDialogTest tests the dialog portion of the
// credential provider sign in without checking whether the fetch of additional
// information was successful.

class CredentialProviderSigninDialogWinDialogTest
    : public CredentialProviderSigninDialogWinBaseTest {
 protected:
  CredentialProviderSigninDialogWinDialogTest();

  void SendSigninCompleteMessage(const base::Value& value);
  void SendValidSigninCompleteMessage();
  void WaitForSigninCompleteMessage();

  void ShowSigninDialog();

  // A HandleGCPWSiginCompleteResult callback to check that the signin dialog
  // has correctly received and procesed the sign in complete message.
  void HandleSignInComplete(
      base::Value signin_result,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader);
  bool signin_complete_called_ = false;

  std::string result_access_token_;
  std::string result_refresh_token_;
  int exit_code_;
  base::Value result_value_;
  CredentialProviderSigninDialogTestDataStorage test_data_storage_;

 private:
  base::OnceClosure signin_complete_closure_;

  DISALLOW_COPY_AND_ASSIGN(CredentialProviderSigninDialogWinDialogTest);
};

CredentialProviderSigninDialogWinDialogTest::
    CredentialProviderSigninDialogWinDialogTest()
    : CredentialProviderSigninDialogWinBaseTest() {}

void CredentialProviderSigninDialogWinDialogTest::SendSigninCompleteMessage(
    const base::Value& value) {
  std::string json_string;
  EXPECT_TRUE(base::JSONWriter::Write(value, &json_string));

  std::string login_complete_message =
      "chrome.send('lstFetchResults', [" + json_string + "]);";
  content::RenderFrameHost* root = web_contents()->GetMainFrame();
  content::ExecuteScriptAsync(root, login_complete_message);
  WaitForSigninCompleteMessage();
}

void CredentialProviderSigninDialogWinDialogTest::
    SendValidSigninCompleteMessage() {
  SendSigninCompleteMessage(test_data_storage_.MakeValidSignInResponseValue());
}

void CredentialProviderSigninDialogWinDialogTest::
    WaitForSigninCompleteMessage() {
  // Run until the dialog has received the signin complete message.
  base::RunLoop run_loop;
  signin_complete_closure_ = run_loop.QuitWhenIdleClosure();
  run_loop.Run();
}

void CredentialProviderSigninDialogWinDialogTest::ShowSigninDialog() {
  views::WebDialogView* web_view = ShowCredentialProviderSigninDialog(
      base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM),
      browser()->profile(),
      base::BindOnce(
          &CredentialProviderSigninDialogWinDialogTest::HandleSignInComplete,
          base::Unretained(this)));

  web_contents_ = web_view->web_contents();
}

void CredentialProviderSigninDialogWinDialogTest::HandleSignInComplete(
    base::Value signin_result,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader) {
  exit_code_ = signin_result
                   .FindKeyOfType(credential_provider::kKeyExitCode,
                                  base::Value::Type::INTEGER)
                   ->GetInt();
  if (exit_code_ == credential_provider::kUiecSuccess) {
    result_access_token_ =
        signin_result
            .FindKeyOfType(credential_provider::kKeyAccessToken,
                           base::Value::Type::STRING)
            ->GetString();
    result_refresh_token_ =
        signin_result
            .FindKeyOfType(credential_provider::kKeyRefreshToken,
                           base::Value::Type::STRING)
            ->GetString();
  }
  EXPECT_FALSE(signin_complete_called_);
  signin_complete_called_ = true;
  result_value_ = std::move(signin_result);

  if (signin_complete_closure_)
    std::move(signin_complete_closure_).Run();
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendEmptySigninComplete) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue());
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoId) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      std::string(), test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoPassword) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(), std::string(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoEmail) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(), std::string(),
      test_data_storage_.GetSuccessAccessToken(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoAccessToken) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(), std::string(),
      test_data_storage_.GetSuccessRefreshToken()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SendInvalidSigninCompleteNoRefreshToken) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendSigninCompleteMessage(test_data_storage_.MakeSignInResponseValue(
      test_data_storage_.GetSuccessId(),
      test_data_storage_.GetSuccessPassword(),
      test_data_storage_.GetSuccessEmail(),
      test_data_storage_.GetSuccessAccessToken(), std::string()));
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(result_value_.DictSize(), 1u);
  EXPECT_TRUE(result_access_token_.empty());
  EXPECT_TRUE(result_refresh_token_.empty());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinDialogTest,
                       SuccessfulLoginMessage) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  SendValidSigninCompleteMessage();
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_GT(result_value_.DictSize(), 1u);
  const base::DictionaryValue* result_dict;
  EXPECT_TRUE(result_value_.GetAsDictionary(&result_dict));
  std::string id_in_dict;
  EXPECT_TRUE(result_dict->GetString("id", &id_in_dict));
  std::string email_in_dict;
  EXPECT_TRUE(result_dict->GetString("email", &email_in_dict));
  std::string password_in_dict;
  EXPECT_TRUE(result_dict->GetString("password", &password_in_dict));

  EXPECT_EQ(id_in_dict, test_data_storage_.GetSuccessId());
  EXPECT_EQ(email_in_dict, test_data_storage_.GetSuccessEmail());
  EXPECT_EQ(password_in_dict, test_data_storage_.GetSuccessPassword());
  EXPECT_EQ(result_access_token_, test_data_storage_.GetSuccessAccessToken());
  EXPECT_EQ(result_refresh_token_, test_data_storage_.GetSuccessRefreshToken());
}

// Tests the various exit codes for success / failure.
class CredentialProviderSigninDialogWinDialogExitCodeTest
    : public CredentialProviderSigninDialogWinDialogTest,
      public ::testing::WithParamInterface<int> {};

IN_PROC_BROWSER_TEST_P(CredentialProviderSigninDialogWinDialogExitCodeTest,
                       SigninResultWithExitCode) {
  ShowSigninDialog();
  WaitForDialogToLoad();
  base::Value signin_result = test_data_storage_.MakeValidSignInResponseValue();

  int expected_error_code = GetParam();
  bool should_succeed =
      expected_error_code == (int)credential_provider::kUiecSuccess;
  signin_result.SetKey(credential_provider::kKeyExitCode,
                       base::Value(expected_error_code));

  SendSigninCompleteMessage(signin_result);
  EXPECT_TRUE(signin_complete_called_);
  EXPECT_TRUE(result_value_.is_dict());
  EXPECT_EQ(exit_code_, expected_error_code);
  const base::Value* exit_code_value = result_value_.FindKeyOfType(
      credential_provider::kKeyExitCode, base::Value::Type::INTEGER);
  EXPECT_NE(exit_code_value, nullptr);
  EXPECT_EQ(exit_code_value->GetInt(), expected_error_code);

  if (should_succeed) {
    EXPECT_GT(result_value_.DictSize(), 1u);

    std::string id_in_dict =
        result_value_.FindKeyOfType("id", base::Value::Type::STRING)
            ->GetString();
    std::string email_in_dict =
        result_value_.FindKeyOfType("email", base::Value::Type::STRING)
            ->GetString();
    std::string password_in_dict =
        result_value_.FindKeyOfType("password", base::Value::Type::STRING)
            ->GetString();

    EXPECT_EQ(id_in_dict, test_data_storage_.GetSuccessId());
    EXPECT_EQ(email_in_dict, test_data_storage_.GetSuccessEmail());
    EXPECT_EQ(password_in_dict, test_data_storage_.GetSuccessPassword());
    EXPECT_EQ(result_access_token_, test_data_storage_.GetSuccessAccessToken());
    EXPECT_EQ(result_refresh_token_,
              test_data_storage_.GetSuccessRefreshToken());
  } else {
    EXPECT_EQ(result_value_.DictSize(), 1u);
    EXPECT_TRUE(result_access_token_.empty());
    EXPECT_TRUE(result_refresh_token_.empty());
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    CredentialProviderSigninDialogWinDialogExitCodeTest,
    ::testing::Range(0, static_cast<int>(credential_provider::kUiecCount)));

///////////////////////////////////////////////////////////////////////////////
// CredentialProviderSigninDialogWinIntegrationTest is used for testing
// the integration of the dialog into Chrome, This test mainly verifies
// correct start up state if we provide the --gcpw-signin switch.

class CredentialProviderSigninDialogWinIntegrationTest
    : public CredentialProviderSigninDialogWinBaseTest {
 protected:
  CredentialProviderSigninDialogWinIntegrationTest();

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // CredentialProviderSigninDialogWinBaseTest:
  void WaitForDialogToLoad() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialProviderSigninDialogWinIntegrationTest);
};

CredentialProviderSigninDialogWinIntegrationTest::
    CredentialProviderSigninDialogWinIntegrationTest()
    : CredentialProviderSigninDialogWinBaseTest() {}

void CredentialProviderSigninDialogWinIntegrationTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(::credential_provider::kGcpwSigninSwitch);
}

void CredentialProviderSigninDialogWinIntegrationTest::WaitForDialogToLoad() {
  // The browser has already been created by the time this start starts and
  // web_contents_ is not yet available. In this run case there should only
  // be one widget available and that widget should contain the web contents
  // needed for the test.
  EXPECT_FALSE(web_contents_);
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  EXPECT_EQ(all_widgets.size(), 1ull);
  views::WebDialogView* web_dialog = static_cast<views::WebDialogView*>(
      (*all_widgets.begin())->GetContentsView());
  web_contents_ = web_dialog->web_contents();
  EXPECT_TRUE(web_contents_);

  CredentialProviderSigninDialogWinBaseTest::WaitForDialogToLoad();

  // When running with --gcpw-signin, browser creation is completely bypassed
  // only a dialog for the signin should be created directly. In a normal
  // browser test, there is always a browser created so make sure that is not
  // the case for our tests.
  EXPECT_FALSE(browser());
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinIntegrationTest,
                       ShowDialogOnlyTest) {
  WaitForDialogToLoad();
  EXPECT_EQ(Profile::ProfileType::INCOGNITO_PROFILE,
            ((Profile*)(web_contents_->GetBrowserContext()))->GetProfileType());
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  (*all_widgets.begin())->Close();
  RunUntilBrowserProcessQuits();
}

IN_PROC_BROWSER_TEST_F(CredentialProviderSigninDialogWinIntegrationTest,
                       EscapeClosesDialogTest) {
  WaitForDialogToLoad();
  views::Widget::Widgets all_widgets = views::test::WidgetTest::GetAllWidgets();
  ui::KeyEvent escape_key_event(ui::EventType::ET_KEY_PRESSED,
                                ui::KeyboardCode::VKEY_ESCAPE,
                                ui::DomCode::ESCAPE, 0);
  (*all_widgets.begin())->OnKeyEvent(&escape_key_event);
  RunUntilBrowserProcessQuits();
}
