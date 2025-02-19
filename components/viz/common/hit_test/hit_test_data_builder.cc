// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/hit_test/hit_test_data_builder.h"

#include "components/viz/common/quads/surface_draw_quad.h"

namespace viz {

// static
base::Optional<HitTestRegionList> HitTestDataBuilder::CreateHitTestData(
    const CompositorFrame& compositor_frame,
    bool root_accepts_events,
    bool should_ask_for_child_region) {
  base::Optional<HitTestRegionList> hit_test_region_list(base::in_place);
  hit_test_region_list->flags =
      (root_accepts_events ? HitTestRegionFlags::kHitTestMine
                           : HitTestRegionFlags::kHitTestIgnore) |
      HitTestRegionFlags::kHitTestMouse | HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds.set_size(compositor_frame.size_in_pixels());
  AddHitTestDataFromRenderPass(
      compositor_frame, compositor_frame.render_pass_list.back()->id,
      &hit_test_region_list->regions, should_ask_for_child_region);
  return hit_test_region_list;
}

// static
const RenderPass* HitTestDataBuilder::GetRenderPassInFrame(
    const CompositorFrame& frame,
    RenderPassId render_pass_id) {
  if (!render_pass_id)
    return frame.render_pass_list.back().get();

  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass->id == render_pass_id)
      return render_pass.get();
  }

  return nullptr;
}

// static
void HitTestDataBuilder::AddHitTestDataFromRenderPass(
    const CompositorFrame& frame,
    RenderPassId render_pass_id,
    std::vector<HitTestRegion>* regions,
    bool should_ask_for_child_region) {
  const RenderPass* render_pass = GetRenderPassInFrame(frame, render_pass_id);
  if (!render_pass)
    return;

  // Skip the render_pass if the transform is not invertible (i.e. it will not
  // be able to receive events).
  gfx::Transform transform_to_root_target =
      render_pass->transform_to_root_target;
  transform_to_root_target.FlattenTo2d();
  gfx::Transform transform_from_root_target;
  if (!transform_to_root_target.GetInverse(&transform_from_root_target)) {
    return;
  }

  for (const DrawQuad* quad : render_pass->quad_list) {
    if (quad->material == DrawQuad::SURFACE_CONTENT) {
      const SurfaceDrawQuad* surface_quad = SurfaceDrawQuad::MaterialCast(quad);

      // Skip the quad if it has pointer-events:none set.
      if (surface_quad->ignores_input_event)
        continue;

      // Skip the quad if the FrameSinkId between fallback and primary is not
      // the same, because we don't know which FrameSinkId would be used to
      // draw this quad.
      if (surface_quad->surface_range.start() &&
          surface_quad->surface_range.start()->frame_sink_id() !=
              surface_quad->surface_range.end().frame_sink_id()) {
        continue;
      }

      // Skip the quad if the transform is not invertible (i.e. it will not
      // be able to receive events).
      gfx::Transform quad_to_target_transform =
          quad->shared_quad_state->quad_to_target_transform;
      quad_to_target_transform.FlattenTo2d();
      gfx::Transform target_to_quad_transform;
      if (!quad_to_target_transform.GetInverse(&target_to_quad_transform)) {
        continue;
      }

      regions->emplace_back();
      HitTestRegion* hit_test_region = &regions->back();
      hit_test_region->frame_sink_id =
          surface_quad->surface_range.end().frame_sink_id();
      hit_test_region->flags = HitTestRegionFlags::kHitTestMouse |
                               HitTestRegionFlags::kHitTestTouch |
                               HitTestRegionFlags::kHitTestChildSurface;
      if (should_ask_for_child_region) {
        hit_test_region->flags |= HitTestRegionFlags::kHitTestAsk;
        hit_test_region->async_hit_test_reasons =
            AsyncHitTestReasons::kUseDrawQuadData;
      }
      hit_test_region->rect = surface_quad->rect;
      hit_test_region->transform =
          target_to_quad_transform * transform_from_root_target;
    } else if (quad->material == DrawQuad::RENDER_PASS) {
      const RenderPassDrawQuad* render_quad =
          RenderPassDrawQuad::MaterialCast(quad);
      AddHitTestDataFromRenderPass(frame, render_quad->render_pass_id, regions,
                                   should_ask_for_child_region);
    }
  }
}

}  // namespace viz
