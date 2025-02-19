// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "content/public/browser/visibility.h"

namespace resource_coordinator {

class LifecycleUnitSourceBase;
class UsageClock;

using ::mojom::LifecycleUnitState;
using ::mojom::LifecycleUnitStateChangeReason;

// Base class for a LifecycleUnit.
class LifecycleUnitBase : public LifecycleUnit {
 public:
  explicit LifecycleUnitBase(LifecycleUnitSourceBase* source,
                             content::Visibility visibility,
                             UsageClock* usage_clock);
  ~LifecycleUnitBase() override;

  // LifecycleUnit:
  LifecycleUnitSource* GetSource() const override;
  int32_t GetID() const override;
  base::TimeTicks GetWallTimeWhenHidden() const override;
  base::TimeDelta GetChromeUsageTimeWhenHidden() const override;
  LifecycleUnitState GetState() const override;
  base::TimeTicks GetStateChangeTime() const override;
  bool Discard(LifecycleUnitDiscardReason discard_reason) override;
  size_t GetDiscardCount() const override;
  LifecycleUnitDiscardReason GetDiscardReason() const override;
  void AddObserver(LifecycleUnitObserver* observer) override;
  void RemoveObserver(LifecycleUnitObserver* observer) override;
  ukm::SourceId GetUkmSourceId() const override;

  void SetDiscardCountForTesting(size_t discard_count);

 protected:
  // TODO(chrisha|fdoray): Clean up the virtual methods below and make them
  // pure virtual.

  // Sets the state of this LifecycleUnit to |state| and notifies observers.
  // |reason| indicates what caused the state change.
  void SetState(LifecycleUnitState state,
                LifecycleUnitStateChangeReason reason);

  // Invoked when the state of the LifecycleUnit changes, before external
  // observers are notified. Derived classes can override to add their own
  // logic. The default implementation is empty. |last_state| is the state
  // before the change and |reason| indicates what caused the change.
  virtual void OnLifecycleUnitStateChanged(
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason);

  // Invoked to dispatch a call to Discard. The base class takes care of
  // maintaining |discard_count_| and |discard_reason_|, and delegates the
  // actually discarding to this.
  virtual bool DiscardImpl(LifecycleUnitDiscardReason discard_reason);

  // Notifies observers that the visibility of the LifecycleUnit has changed.
  void OnLifecycleUnitVisibilityChanged(content::Visibility visibility);

  // Notifies observers that the LifecycleUnit is being destroyed. This is
  // invoked by derived classes rather than by the base class to avoid notifying
  // observers when the LifecycleUnit has been partially destroyed. This also
  // forwards the notification to the lifecycle unit source via
  // LifecycleUnitSourceBase.
  void OnLifecycleUnitDestroyed();

 private:
  static int32_t next_id_;

  // A unique id representing this LifecycleUnit.
  const int32_t id_ = ++next_id_;

  // The source that owns this lifecycle unit. This can be nullptr.
  LifecycleUnitSourceBase* source_;

  // Current state of this LifecycleUnit.
  LifecycleUnitState state_ = LifecycleUnitState::ACTIVE;

  // Time at which the state changed.
  base::TimeTicks state_change_time_ = NowTicks();

  // The wall time when this LifecycleUnit was last hidden, or TimeDelta::Max()
  // if this LifecycleUnit is currently visible.
  base::TimeTicks wall_time_when_hidden_;

  // A clock that measures Chrome usage time.
  UsageClock* const usage_clock_;

  // The Chrome usage time measured by |usage_clock_| when this LifecycleUnit
  // was last hidden, or TimeDelta::Max() if this LifecycleUnit is currently
  // visible.
  base::TimeDelta chrome_usage_time_when_hidden_;

  // The number of times that this lifecycle unit has been discarded.
  int discard_count_ = 0;

  // Maintains the most recent LifecycleUnitDiscardReason that was passed into
  // Discard().
  LifecycleUnitDiscardReason discard_reason_ =
      LifecycleUnitDiscardReason::EXTERNAL;

  base::ObserverList<LifecycleUnitObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleUnitBase);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
