// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/self_delete_full_card_requester.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/autofill_assistant/browser/web_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

// Maximum amount of time normal actions should implicitly wait for a selector
// to show up.
constexpr base::TimeDelta kShortWaitForElementDeadline =
    base::TimeDelta::FromSeconds(2);
}  // namespace

ScriptExecutor::ScriptExecutor(
    const std::string& script_path,
    const std::string& global_payload,
    const std::string& script_payload,
    ScriptExecutor::Listener* listener,
    std::map<std::string, ScriptStatusProto>* scripts_state,
    const std::vector<Script*>* ordered_interrupts,
    ScriptExecutorDelegate* delegate)
    : script_path_(script_path),
      last_global_payload_(global_payload),
      initial_script_payload_(script_payload),
      last_script_payload_(script_payload),
      listener_(listener),
      delegate_(delegate),
      at_end_(CONTINUE),
      should_stop_script_(false),
      should_clean_contextual_ui_on_finish_(false),
      previous_action_type_(ActionProto::ACTION_INFO_NOT_SET),
      scripts_state_(scripts_state),
      ordered_interrupts_(ordered_interrupts),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(ordered_interrupts_);
}
ScriptExecutor::~ScriptExecutor() {}

ScriptExecutor::Result::Result() = default;
ScriptExecutor::Result::~Result() = default;

void ScriptExecutor::Run(RunScriptCallback callback) {
  (*scripts_state_)[script_path_] = SCRIPT_STATUS_RUNNING;

  callback_ = std::move(callback);
  DCHECK(delegate_->GetService());

  delegate_->GetService()->GetActions(
      script_path_, delegate_->GetWebController()->GetUrl(),
      delegate_->GetParameters(), last_global_payload_, last_script_payload_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<BatchElementChecker>
ScriptExecutor::CreateBatchElementChecker() {
  return delegate_->GetWebController()->CreateBatchElementChecker();
}

void ScriptExecutor::ShortWaitForElementExist(
    const Selector& selector,
    base::OnceCallback<void(bool)> callback) {
  WaitForElement(kShortWaitForElementDeadline, kExistenceCheck, selector,
                 std::move(callback));
}

void ScriptExecutor::WaitForElementVisible(
    base::TimeDelta max_wait_time,
    bool allow_interrupt,
    const Selector& selector,
    base::OnceCallback<void(ProcessedActionStatusProto)> callback) {
  if (!allow_interrupt || ordered_interrupts_->empty()) {
    // No interrupts to worry about. Just run normal wait.
    WaitForElement(
        max_wait_time, kVisibilityCheck, selector,
        base::BindOnce(&ScriptExecutor::OnWaitForElementVisibleNoInterrupts,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  wait_with_interrupts_ = std::make_unique<WaitWithInterrupts>(
      this, max_wait_time, kVisibilityCheck, selector,
      base::BindOnce(&ScriptExecutor::OnWaitForElementVisibleWithInterrupts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  wait_with_interrupts_->Run();
}

void ScriptExecutor::SetStatusMessage(const std::string& message) {
  delegate_->SetStatusMessage(message);
}

std::string ScriptExecutor::GetStatusMessage() {
  return delegate_->GetStatusMessage();
}

void ScriptExecutor::ClickOrTapElement(
    const Selector& selector,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->ClickOrTapElement(selector,
                                                   std::move(callback));
}

void ScriptExecutor::GetPaymentInformation(
    payments::mojom::PaymentOptionsPtr payment_options,
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    const std::vector<std::string>& supported_basic_card_networks) {
  delegate_->EnterState(AutofillAssistantState::PROMPT);
  delegate_->GetUiController()->GetPaymentInformation(
      std::move(payment_options),
      base::BindOnce(&ScriptExecutor::OnGetPaymentInformation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      supported_basic_card_networks);
}

void ScriptExecutor::OnGetPaymentInformation(
    base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
    std::unique_ptr<PaymentInformation> result) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(std::move(result));
}

void ScriptExecutor::GetFullCard(GetFullCardCallback callback) {
  DCHECK(GetClientMemory()->selected_card());

  // User might be asked to provide the cvc.
  delegate_->EnterState(AutofillAssistantState::MODAL_DIALOG);

  // TODO(crbug.com/806868): Consider refactoring SelfDeleteFullCardRequester
  // so as to unit test it.
  (new SelfDeleteFullCardRequester())
      ->GetFullCard(
          GetWebContents(), GetClientMemory()->selected_card(),
          base::BindOnce(&ScriptExecutor::OnGetFullCard,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::OnGetFullCard(GetFullCardCallback callback,
                                   std::unique_ptr<autofill::CreditCard> card,
                                   const base::string16& cvc) {
  delegate_->EnterState(AutofillAssistantState::RUNNING);
  std::move(callback).Run(std::move(card), cvc);
}

void ScriptExecutor::Prompt(std::unique_ptr<std::vector<Chip>> chips) {
  if (touchable_element_area_) {
    // SetChips reproduces the end-of-script appearance and behavior during
    // script execution. This includes allowing access to touchable elements,
    // set through a previous call to the focus action with touchable_elements
    // set.
    delegate_->SetTouchableElementArea(*touchable_element_area_);

    // The touchable_elements_ currently set in the script is reset, so that it
    // won't affect the real end of the script.
    touchable_element_area_.reset();

    // The touchable element and overlays are cleared again in
    // ScriptExecutor::OnChosen or ScriptExecutor::ClearChips
  }

  // We change the chips callback with a callback that cleans up the state
  // before calling the initial callback.
  for (auto& chip : *chips) {
    chip.callback = base::BindOnce(&ScriptExecutor::OnChosen,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(chip.callback));
  }

  delegate_->EnterState(AutofillAssistantState::PROMPT);
  delegate_->SetChips(std::move(chips));
}

void ScriptExecutor::CancelPrompt() {
  delegate_->SetChips(nullptr);
  CleanUpAfterPrompt();
}

void ScriptExecutor::CleanUpAfterPrompt() {
  delegate_->ClearTouchableElementArea();
  delegate_->EnterState(AutofillAssistantState::RUNNING);
}

void ScriptExecutor::OnChosen(base::OnceClosure callback) {
  CleanUpAfterPrompt();
  std::move(callback).Run();
}

void ScriptExecutor::FillAddressForm(const autofill::AutofillProfile* profile,
                                     const Selector& selector,
                                     base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillAddressForm(profile, selector,
                                                 std::move(callback));
}

void ScriptExecutor::FillCardForm(std::unique_ptr<autofill::CreditCard> card,
                                  const base::string16& cvc,
                                  const Selector& selector,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillCardForm(std::move(card), cvc, selector,
                                              std::move(callback));
}

void ScriptExecutor::SelectOption(const Selector& selector,
                                  const std::string& selected_option,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SelectOption(selector, selected_option,
                                              std::move(callback));
}

void ScriptExecutor::HighlightElement(const Selector& selector,
                                      base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->HighlightElement(selector,
                                                  std::move(callback));
}

void ScriptExecutor::FocusElement(const Selector& selector,
                                  base::OnceCallback<void(bool)> callback) {
  last_focused_element_selector_ = selector;
  delegate_->GetWebController()->FocusElement(selector, std::move(callback));
}

void ScriptExecutor::SetTouchableElementArea(
    const ElementAreaProto& touchable_element_area) {
  touchable_element_area_ =
      std::make_unique<ElementAreaProto>(touchable_element_area);
}

void ScriptExecutor::SetProgress(int progress) {
  delegate_->SetProgress(progress);
}

void ScriptExecutor::SetFieldValue(const Selector& selector,
                                   const std::string& value,
                                   bool simulate_key_presses,
                                   base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SetFieldValue(
      selector, value, simulate_key_presses, std::move(callback));
}

void ScriptExecutor::SetAttribute(const Selector& selector,
                                  const std::vector<std::string>& attribute,
                                  const std::string& value,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SetAttribute(selector, attribute, value,
                                              std::move(callback));
}

void ScriptExecutor::SendKeyboardInput(
    const Selector& selector,
    const std::vector<std::string>& text_parts,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SendKeyboardInput(selector, text_parts,
                                                   std::move(callback));
}

void ScriptExecutor::GetOuterHtml(
    const Selector& selector,
    base::OnceCallback<void(bool, const std::string&)> callback) {
  delegate_->GetWebController()->GetOuterHtml(selector, std::move(callback));
}

void ScriptExecutor::LoadURL(const GURL& url) {
  delegate_->GetWebController()->LoadURL(url);
}

void ScriptExecutor::Shutdown() {
  // The following handles the case where scripts end with tell + stop
  // differently from just stop. TODO(b/806868): Make that difference explicit:
  // add an optional message to stop and update the scripts to use that.
  if (previous_action_type_ == ActionProto::kTell) {
    at_end_ = SHUTDOWN_GRACEFULLY;
  } else {
    at_end_ = SHUTDOWN;
  }
}

void ScriptExecutor::Terminate() {
  if (wait_with_interrupts_)
    wait_with_interrupts_->Terminate();
  at_end_ = TERMINATE;
  should_stop_script_ = true;
}

void ScriptExecutor::Close() {
  at_end_ = CLOSE_CUSTOM_TAB;
  should_stop_script_ = true;
}

void ScriptExecutor::Restart() {
  at_end_ = RESTART;
}

void ScriptExecutor::StopCurrentScriptAndShutdown(const std::string& message) {
  // Use a default message when |message| is empty.
  delegate_->SetStatusMessage(
      message.empty() ? l10n_util::GetStringUTF8(IDS_AUTOFILL_ASSISTANT_GIVE_UP)
                      : message);
  at_end_ = SHUTDOWN_GRACEFULLY;
  should_stop_script_ = true;
}

ClientMemory* ScriptExecutor::GetClientMemory() {
  return delegate_->GetClientMemory();
}

autofill::PersonalDataManager* ScriptExecutor::GetPersonalDataManager() {
  return delegate_->GetPersonalDataManager();
}

content::WebContents* ScriptExecutor::GetWebContents() {
  return delegate_->GetWebContents();
}

void ScriptExecutor::ClearDetails() {
  delegate_->ClearDetails();
}

void ScriptExecutor::SetDetails(const Details& details) {
  return delegate_->SetDetails(details);
}

void ScriptExecutor::OnGetActions(bool result, const std::string& response) {
  bool success = result && ProcessNextActionResponse(response);
  if (should_stop_script_) {
    // The last action forced the script to stop. Sending the result of the
    // action is considered best effort in this situation. Report a successful
    // run to the caller no matter what, so we don't confuse users with an error
    // message.
    RunCallback(true);
    return;
  }

  if (!success) {
    RunCallback(false);
    return;
  }

  if (!actions_.empty()) {
    ProcessNextAction();
    return;
  }

  RunCallback(true);
}

bool ScriptExecutor::ProcessNextActionResponse(const std::string& response) {
  processed_actions_.clear();
  actions_.clear();

  bool should_update_scripts = false;
  std::vector<std::unique_ptr<Script>> scripts;
  bool parse_result = ProtocolUtils::ParseActions(
      response, &last_global_payload_, &last_script_payload_, &actions_,
      &scripts, &should_update_scripts);
  if (!parse_result) {
    return false;
  }

  ReportPayloadsToListener();
  if (should_update_scripts) {
    ReportScriptsUpdateToListener(std::move(scripts));
  }
  return true;
}

void ScriptExecutor::ReportPayloadsToListener() {
  if (!listener_)
    return;

  listener_->OnServerPayloadChanged(last_global_payload_, last_script_payload_);
}

void ScriptExecutor::ReportScriptsUpdateToListener(
    std::vector<std::unique_ptr<Script>> scripts) {
  if (!listener_)
    return;

  listener_->OnScriptListChanged(std::move(scripts));
}

void ScriptExecutor::RunCallback(bool success) {
  DCHECK(callback_);
  if (should_clean_contextual_ui_on_finish_ || !success) {
    ClearDetails();
    should_clean_contextual_ui_on_finish_ = false;
  }

  Result result;
  result.success = success;
  result.at_end = at_end_;
  result.touchable_element_area = std::move(touchable_element_area_);

  RunCallbackWithResult(result);
}

void ScriptExecutor::RunCallbackWithResult(const Result& result) {
  (*scripts_state_)[script_path_] =
      result.success ? SCRIPT_STATUS_SUCCESS : SCRIPT_STATUS_FAILURE;
  std::move(callback_).Run(result);
}

void ScriptExecutor::ProcessNextAction() {
  // We could get into a strange situation if ProcessNextAction is called before
  // the action was reported as processed, which should not happen. In that case
  // we could have more |processed_actions| than |actions_|.
  if (actions_.size() <= processed_actions_.size()) {
    DCHECK_EQ(actions_.size(), processed_actions_.size());
    // Request more actions to execute.
    GetNextActions();
    return;
  }

  Action* action = actions_[processed_actions_.size()].get();
  should_clean_contextual_ui_on_finish_ = action->proto().clean_contextual_ui();
  int delay_ms = action->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScriptExecutor::ProcessAction,
                       weak_ptr_factory_.GetWeakPtr(), action),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    ProcessAction(action);
  }
}

void ScriptExecutor::ProcessAction(Action* action) {
  action->ProcessAction(this, base::BindOnce(&ScriptExecutor::OnProcessedAction,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::GetNextActions() {
  delegate_->GetService()->GetNextActions(
      last_global_payload_, last_script_payload_, processed_actions_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::OnProcessedAction(
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  previous_action_type_ = processed_action_proto->action().action_info_case();
  processed_actions_.emplace_back(*processed_action_proto);

  auto& processed_action = processed_actions_.back();
  if (at_end_ == TERMINATE) {
    // Let the backend know that the script has been terminated. The original
    // action status doesn't matter.
    processed_action.set_status(
        ProcessedActionStatusProto::USER_ABORTED_ACTION);
  }
  if (processed_action.status() != ProcessedActionStatusProto::ACTION_APPLIED) {
    // Report error immediately, interrupting action processing.
    GetNextActions();
    return;
  }
  ProcessNextAction();
}

void ScriptExecutor::WaitForElement(base::TimeDelta max_wait_time,
                                    ElementCheckType check_type,
                                    const Selector& selector,
                                    base::OnceCallback<void(bool)> callback) {
  DCHECK(!batch_element_checker_);
  batch_element_checker_ = CreateBatchElementChecker();
  batch_element_checker_->AddElementCheck(check_type, selector,
                                          base::DoNothing());
  batch_element_checker_->Run(
      max_wait_time, /* try_done= */ base::DoNothing(), /* all_done= */
      base::BindOnce(&ScriptExecutor::OnWaitForElement,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ScriptExecutor::OnWaitForElement(base::OnceCallback<void(bool)> callback) {
  bool all_found = batch_element_checker_->all_found();
  batch_element_checker_.reset();
  std::move(callback).Run(all_found);
}

void ScriptExecutor::OnWaitForElementVisibleWithInterrupts(
    base::OnceCallback<void(ProcessedActionStatusProto)> callback,
    bool element_found,
    const Result* interrupt_result,
    const std::set<std::string>& interrupt_paths) {
  ran_interrupts_.insert(interrupt_paths.begin(), interrupt_paths.end());
  if (interrupt_result) {
    if (!interrupt_result->success) {
      std::move(callback).Run(INTERRUPT_FAILED);
      return;
    }
    if (interrupt_result->at_end != CONTINUE) {
      at_end_ = interrupt_result->at_end;
      should_stop_script_ = true;
      std::move(callback).Run(MANUAL_FALLBACK);
      return;
    }
  }
  OnWaitForElementVisibleNoInterrupts(std::move(callback), element_found);
}

void ScriptExecutor::OnWaitForElementVisibleNoInterrupts(
    base::OnceCallback<void(ProcessedActionStatusProto)> callback,
    bool element_found) {
  std::move(callback).Run(element_found ? ACTION_APPLIED
                                        : ELEMENT_RESOLUTION_FAILED);
}

ScriptExecutor::WaitWithInterrupts::WaitWithInterrupts(
    ScriptExecutor* main_script,
    base::TimeDelta max_wait_time,
    ElementCheckType check_type,
    const Selector& selector,
    WaitWithInterrupts::Callback callback)
    : main_script_(main_script),
      max_wait_time_(max_wait_time),
      check_type_(check_type),
      selector_(selector),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

ScriptExecutor::WaitWithInterrupts::~WaitWithInterrupts() = default;

void ScriptExecutor::WaitWithInterrupts::Run() {
  // Reset state possibly left over from previous runs.
  element_found_ = false;
  runnable_interrupts_.clear();
  batch_element_checker_ =
      main_script_->delegate_->GetWebController()->CreateBatchElementChecker();

  batch_element_checker_->AddElementCheck(
      check_type_, selector_,
      base::BindOnce(&WaitWithInterrupts::OnElementCheckDone,
                     base::Unretained(this)));
  for (const auto* interrupt : *main_script_->ordered_interrupts_) {
    if (ran_interrupts_.find(interrupt->handle.path) != ran_interrupts_.end()) {
      // Only run an interrupt once in a WaitWithInterrupts, to avoid loops.
      continue;
    }

    interrupt->precondition->Check(
        main_script_->delegate_->GetWebController()->GetUrl(),
        batch_element_checker_.get(), main_script_->delegate_->GetParameters(),
        *main_script_->scripts_state_,
        base::BindOnce(&WaitWithInterrupts::OnPreconditionCheckDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::Unretained(interrupt)));
  }
  // The base::Unretained(this) above are safe, since the pointers belong to the
  // main script, which own this instance.

  batch_element_checker_->Run(
      max_wait_time_,
      base::BindRepeating(&WaitWithInterrupts::OnTryDone,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WaitWithInterrupts::OnAllDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::WaitWithInterrupts::OnServerPayloadChanged(
    const std::string& global_payload,
    const std::string& script_payload) {
  // Interrupts and main scripts share global payloads, but not script payloads.
  main_script_->last_global_payload_ = global_payload;
  main_script_->ReportPayloadsToListener();
}

void ScriptExecutor::WaitWithInterrupts::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  main_script_->ReportScriptsUpdateToListener(std::move(scripts));
}

void ScriptExecutor::WaitWithInterrupts::OnPreconditionCheckDone(
    const Script* interrupt,
    bool precondition_match) {
  if (precondition_match)
    runnable_interrupts_.insert(interrupt);
}

void ScriptExecutor::WaitWithInterrupts::OnElementCheckDone(bool found) {
  element_found_ = found;
  // Wait for all checks to run before reporting that the element was found to
  // the caller, so interrupts have a chance to run.
}

void ScriptExecutor::WaitWithInterrupts::OnTryDone() {
  if (!runnable_interrupts_.empty()) {
    // We must go through runnable_interrupts_ to make sure priority order is
    // respected in case more than one interrupt is ready to run.
    for (const auto* interrupt : *main_script_->ordered_interrupts_) {
      if (runnable_interrupts_.find(interrupt) != runnable_interrupts_.end()) {
        RunInterrupt(interrupt);
        return;
      }
    }
  }

  if (element_found_)
    RunCallback(true, nullptr);
}

void ScriptExecutor::WaitWithInterrupts::OnAllDone() {
  // This means that we've reached the end of the timeout. Report whether we
  // found the element unless an interrupt has just been started by OnTryDone.
  if (!interrupt_executor_)
    RunCallback(element_found_, nullptr);
}

void ScriptExecutor::WaitWithInterrupts::RunInterrupt(const Script* interrupt) {
  batch_element_checker_.reset();
  SavePreInterruptState();
  ran_interrupts_.insert(interrupt->handle.path);
  interrupt_executor_ = std::make_unique<ScriptExecutor>(
      interrupt->handle.path, main_script_->last_global_payload_,
      main_script_->initial_script_payload_,
      /* listener= */ this, main_script_->scripts_state_, &no_interrupts_,
      main_script_->delegate_);
  interrupt_executor_->Run(
      base::BindOnce(&ScriptExecutor::WaitWithInterrupts::OnInterruptDone,
                     base::Unretained(this)));
  // base::Unretained(this) is safe because interrupt_executor_ belongs to this
}

void ScriptExecutor::WaitWithInterrupts::OnInterruptDone(
    const ScriptExecutor::Result& result) {
  interrupt_executor_.reset();
  if (!result.success || result.at_end != ScriptExecutor::CONTINUE) {
    RunCallback(false, &result);
    return;
  }
  RestoreStatusMessage();

  // Restart. We use the original wait time since the interruption could have
  // triggered any kind of actions, including actions that wait on the user. We
  // don't trust a previous element_found_ result, since it could have changed.
  Run();
}

void ScriptExecutor::WaitWithInterrupts::RunCallback(
    bool found,
    const ScriptExecutor::Result* result) {
  // stop element checking if one is still in progress
  batch_element_checker_.reset();
  if (!callback_)
    return;

  RestorePreInterruptScroll(found);
  std::move(callback_).Run(found, result, ran_interrupts_);
}

void ScriptExecutor::WaitWithInterrupts::SavePreInterruptState() {
  if (saved_pre_interrupt_state_)
    return;

  pre_interrupt_status_ = main_script_->delegate_->GetStatusMessage();
  saved_pre_interrupt_state_ = true;
}

void ScriptExecutor::WaitWithInterrupts::RestoreStatusMessage() {
  if (!saved_pre_interrupt_state_)
    return;

  main_script_->delegate_->SetStatusMessage(pre_interrupt_status_);
}

void ScriptExecutor::WaitWithInterrupts::RestorePreInterruptScroll(
    bool element_found) {
  if (!saved_pre_interrupt_state_)
    return;

  auto* delegate = main_script_->delegate_;
  if (element_found) {
    delegate->GetWebController()->FocusElement(selector_, base::DoNothing());
  } else if (!main_script_->last_focused_element_selector_.empty()) {
    delegate->GetWebController()->FocusElement(
        main_script_->last_focused_element_selector_, base::DoNothing());
  }
}

void ScriptExecutor::WaitWithInterrupts::Terminate() {
  if (interrupt_executor_)
    interrupt_executor_->Terminate();
}

}  // namespace autofill_assistant
