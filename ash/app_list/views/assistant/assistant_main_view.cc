// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/assistant_main_stage.h"
#include "ash/assistant/util/assistant_util.h"
#include "base/time/time.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

AssistantMainView::AssistantMainView(ash::AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  for (ash::DialogPlateObserver* observer :
       delegate_->GetDialogPlateObservers()) {
    dialog_plate_->AddObserver(observer);
  }
}

AssistantMainView::~AssistantMainView() {
  for (ash::DialogPlateObserver* observer :
       delegate_->GetDialogPlateObservers()) {
    dialog_plate_->RemoveObserver(observer);
  }
}

const char* AssistantMainView::GetClassName() const {
  return "AssistantMainView";
}

gfx::Size AssistantMainView::CalculatePreferredSize() const {
  return gfx::Size(ash::kPreferredWidthDip,
                   GetHeightForWidth(ash::kPreferredWidthDip));
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // Even though the preferred size for |main_stage_| may change, its bounds
  // may not actually change due to height restrictions imposed by its parent.
  // For this reason, we need to explicitly trigger a layout pass so that the
  // children of |main_stage_| are properly updated.
  if (child == main_stage_) {
    Layout();
    SchedulePaint();
  }
}

void AssistantMainView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

views::View* AssistantMainView::FindFirstFocusableView() {
  // In those instances in which we want to override views::FocusSearch
  // behavior, DialogPlate will identify the first focusable view.
  return dialog_plate_->FindFirstFocusableView();
}

void AssistantMainView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Main stage.
  main_stage_ = new ash::AssistantMainStage(delegate_);
  AddChildView(main_stage_);

  layout_manager->SetFlexForView(main_stage_, 1);

  // Dialog plate.
  dialog_plate_ = new ash::DialogPlate(delegate_);

  // The dialog plate will be animated on its own layer.
  dialog_plate_->SetPaintToLayer();
  dialog_plate_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(dialog_plate_);
}

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace app_list
