// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_observer.h"

#include <algorithm>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_observer_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_observer_entry_list.h"
#include "third_party/blink/renderer/core/timing/performance_observer_init.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

PerformanceObserver* PerformanceObserver::Create(
    ScriptState* script_state,
    V8PerformanceObserverCallback* callback) {
  LocalDOMWindow* window = ToLocalDOMWindow(script_state->GetContext());
  ExecutionContext* context = ExecutionContext::From(script_state);
  if (window) {
    UseCounter::Count(context, WebFeature::kPerformanceObserverForWindow);
    return MakeGarbageCollected<PerformanceObserver>(
        context, DOMWindowPerformance::performance(*window), callback);
  }
  if (auto* scope = DynamicTo<WorkerGlobalScope>(context)) {
    UseCounter::Count(context, WebFeature::kPerformanceObserverForWorker);
    return MakeGarbageCollected<PerformanceObserver>(
        context, WorkerGlobalScopePerformance::performance(*scope), callback);
  }
  V8ThrowException::ThrowTypeError(
      script_state->GetIsolate(),
      ExceptionMessages::FailedToConstruct(
          "PerformanceObserver",
          "No 'worker' or 'window' in current context."));
  return nullptr;
}

// static
Vector<AtomicString> PerformanceObserver::supportedEntryTypes(
    ScriptState* script_state) {
  // The list of supported types, in alphabetical order.
  Vector<AtomicString> supportedEntryTypes;
  auto* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsDocument()) {
    if (origin_trials::ElementTimingEnabled(execution_context))
      supportedEntryTypes.push_back(performance_entry_names::kElement);
    if (origin_trials::EventTimingEnabled(execution_context))
      supportedEntryTypes.push_back(performance_entry_names::kEvent);
    supportedEntryTypes.push_back(performance_entry_names::kFirstInput);
    if (origin_trials::LayoutJankAPIEnabled(execution_context))
      supportedEntryTypes.push_back(performance_entry_names::kLayoutJank);
    supportedEntryTypes.push_back(performance_entry_names::kLongtask);
  }
  supportedEntryTypes.push_back(performance_entry_names::kMark);
  supportedEntryTypes.push_back(performance_entry_names::kMeasure);
  if (execution_context->IsDocument()) {
    supportedEntryTypes.push_back(performance_entry_names::kNavigation);
    supportedEntryTypes.push_back(performance_entry_names::kPaint);
  }
  supportedEntryTypes.push_back(performance_entry_names::kResource);
  return supportedEntryTypes;
}

PerformanceObserver::PerformanceObserver(
    ExecutionContext* execution_context,
    Performance* performance,
    V8PerformanceObserverCallback* callback)
    : ContextClient(execution_context),
      execution_context_(execution_context),
      callback_(callback),
      performance_(performance),
      filter_options_(PerformanceEntry::kInvalid),
      type_(PerformanceObserverType::kUnknown),
      is_registered_(false) {
  DCHECK(performance_);
}

void PerformanceObserver::observe(const PerformanceObserverInit* observer_init,
                                  ExceptionState& exception_state) {
  if (!performance_) {
    exception_state.ThrowTypeError(
        "Window/worker may be destroyed? Performance target is invalid.");
    return;
  }

  if (observer_init->hasEntryTypes()) {
    if (observer_init->hasType()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "An observe() call MUST NOT include both entryTypes and type.");
      return;
    }
    if (type_ == PerformanceObserverType::kTypeObserver) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "This observer has performed observe({type:...}, therefore it cannot "
          "perform observe({entryTypes:...})");
      return;
    }
    type_ = PerformanceObserverType::kEntryTypesObserver;
    PerformanceEntryTypeMask entry_types = PerformanceEntry::kInvalid;
    const Vector<String>& sequence = observer_init->entryTypes();
    for (const auto& entry_type_string : sequence) {
      entry_types |=
          PerformanceEntry::ToEntryTypeEnum(AtomicString(entry_type_string));
    }
    if (entry_types == PerformanceEntry::kInvalid) {
      String message =
          "The Performance Observer MUST have at least one valid entryType in "
          "its "
          "entryTypes attribute.";
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kWarningMessageLevel, message));
      return;
    }
    filter_options_ = entry_types;
  } else {
    if (!observer_init->hasType()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "An observe() call MUST include either entryTypes or type.");
      return;
    }
    if (type_ == PerformanceObserverType::kEntryTypesObserver) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidModificationError,
          "This observer has performed observe({entryTypes:...}, therefore it "
          "cannot perform observe({type:...})");
      return;
    }
    type_ = PerformanceObserverType::kTypeObserver;
    PerformanceEntryType entry_type =
        PerformanceEntry::ToEntryTypeEnum(AtomicString(observer_init->type()));
    if (entry_type == PerformanceEntry::kInvalid) {
      String message =
          "The Performance Observer MUST have a valid entryType in its "
          "type attribute.";
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kWarningMessageLevel, message));
      return;
    }
    filter_options_ |= entry_type;
  }
  if (filter_options_ & PerformanceEntry::kLayoutJank) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kLayoutJankExplicitlyRequested);
  }
  if (is_registered_)
    performance_->UpdatePerformanceObserverFilterOptions();
  else
    performance_->RegisterPerformanceObserver(*this);
  is_registered_ = true;
}

void PerformanceObserver::disconnect() {
  performance_entries_.clear();
  if (performance_)
    performance_->UnregisterPerformanceObserver(*this);
  is_registered_ = false;
}

PerformanceEntryVector PerformanceObserver::takeRecords() {
  PerformanceEntryVector performance_entries;
  performance_entries.swap(performance_entries_);
  return performance_entries;
}

void PerformanceObserver::EnqueuePerformanceEntry(PerformanceEntry& entry) {
  performance_entries_.push_back(&entry);
  if (performance_)
    performance_->ActivateObserver(*this);
}

bool PerformanceObserver::HasPendingActivity() const {
  return is_registered_;
}

bool PerformanceObserver::ShouldBeSuspended() const {
  return execution_context_->IsContextPaused();
}

void PerformanceObserver::Deliver() {
  DCHECK(!ShouldBeSuspended());

  if (!GetExecutionContext())
    return;

  if (performance_entries_.IsEmpty())
    return;

  PerformanceEntryVector performance_entries;
  performance_entries.swap(performance_entries_);
  PerformanceObserverEntryList* entry_list =
      MakeGarbageCollected<PerformanceObserverEntryList>(performance_entries);
  callback_->InvokeAndReportException(this, entry_list, this);
}

void PerformanceObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
  visitor->Trace(callback_);
  visitor->Trace(performance_);
  visitor->Trace(performance_entries_);
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
