// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include <stdint.h>

#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"

namespace viz {

ChildLocalSurfaceIdAllocator::ChildLocalSurfaceIdAllocator(
    const base::TickClock* tick_clock)
    : current_local_surface_id_allocation_(
          LocalSurfaceId(kInvalidParentSequenceNumber,
                         kInitialChildSequenceNumber,
                         base::UnguessableToken()),
          base::TimeTicks()),
      tick_clock_(tick_clock) {}

ChildLocalSurfaceIdAllocator::ChildLocalSurfaceIdAllocator()
    : ChildLocalSurfaceIdAllocator(base::DefaultTickClock::GetInstance()) {}

bool ChildLocalSurfaceIdAllocator::UpdateFromParent(
    const LocalSurfaceIdAllocation& parent_local_surface_id_allocation) {
  const LocalSurfaceId& current_local_surface_id =
      current_local_surface_id_allocation_.local_surface_id_;
  const LocalSurfaceId& parent_allocated_local_surface_id =
      parent_local_surface_id_allocation.local_surface_id();

  // If the parent has not incremented its parent sequence number or updated its
  // embed token then there is nothing to do here. This allocator already has
  // the latest LocalSurfaceId.
  if (current_local_surface_id.parent_sequence_number() >=
          parent_allocated_local_surface_id.parent_sequence_number() &&
      current_local_surface_id.embed_token() ==
          parent_allocated_local_surface_id.embed_token()) {
    return false;
  }

  if (current_local_surface_id.child_sequence_number() >
      parent_allocated_local_surface_id.child_sequence_number()) {
    // If the current LocalSurfaceId has a newer child sequence number
    // than the one provided by the parent, then the merged LocalSurfaceId
    // is actually a new LocalSurfaceId and so we report its allocation time
    // as now.
    current_local_surface_id_allocation_.allocation_time_ =
        tick_clock_->NowTicks();
  } else {
    current_local_surface_id_allocation_.allocation_time_ =
        parent_local_surface_id_allocation.allocation_time();
  }

  current_local_surface_id_allocation_.local_surface_id_
      .parent_sequence_number_ =
      parent_allocated_local_surface_id.parent_sequence_number_;
  current_local_surface_id_allocation_.local_surface_id_.embed_token_ =
      parent_allocated_local_surface_id.embed_token_;

  return true;
}

void ChildLocalSurfaceIdAllocator::GenerateId() {
  // UpdateFromParent must be called before we can generate a valid ID.
  DCHECK_NE(current_local_surface_id_allocation_.local_surface_id_
                .parent_sequence_number(),
            kInvalidParentSequenceNumber);

  ++current_local_surface_id_allocation_.local_surface_id_
        .child_sequence_number_;
  current_local_surface_id_allocation_.allocation_time_ =
      tick_clock_->NowTicks();

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Embed.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_allocation_.local_surface_id_
                          .embed_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ChildLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_allocation_.local_surface_id_.ToString());
  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(current_local_surface_id_allocation_.local_surface_id_
                          .submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "ChildLocalSurfaceIdAllocator::GenerateId", "local_surface_id",
      current_local_surface_id_allocation_.local_surface_id_.ToString());
}

void ChildLocalSurfaceIdAllocator::GenerateIdOrIncrementChild() {
  if (current_local_surface_id_allocation_.IsValid()) {
    GenerateId();
  } else {
    ++current_local_surface_id_allocation_.local_surface_id_
          .child_sequence_number_;
  }
}

}  // namespace viz
