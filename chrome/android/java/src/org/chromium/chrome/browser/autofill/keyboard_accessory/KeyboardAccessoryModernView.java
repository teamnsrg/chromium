// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.content.Context;
import android.support.v4.view.ViewCompat;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.R;

/**
 * The Accessory sitting above the keyboard and below the content area. It is used for autofill
 * suggestions and manual entry points assisting the user in filling forms.
 */
class KeyboardAccessoryModernView extends KeyboardAccessoryView {
    private ImageView mKeyboardToggle;

    /**
     * This decoration ensures that the last item is right-aligned.
     * To do this, it subtracts the widths, margins and offsets of all items in the recycler view
     * from the RecyclerView's total width. If the items fill the whole recycler view, the last item
     * uses the same offset as all other items.
     */
    private class StickyLastItemDecoration extends HorizontalDividerItemDecoration {
        StickyLastItemDecoration() {
            super(0);
        }

        @Override
        protected int getItemOffsetInternal(
                final View view, final RecyclerView parent, RecyclerView.State state) {
            int minimalOffset = super.getItemOffsetInternal(view, parent, state);
            if (!isLastItem(parent, view, parent.getAdapter().getItemCount())) return minimalOffset;
            if (view.getWidth() == 0 && state.didStructureChange()) {
                // When the RecyclerView is first created, its children aren't measured yet and miss
                // dimensions. Therefore, estimate the offset and recalculate after UI has loaded.
                view.post(parent::invalidateItemDecorations);
                return parent.getWidth() - estimateLastElementWidth(view);
            }
            return Math.max(getSpaceLeftInParent(parent), minimalOffset);
        }

        private int getSpaceLeftInParent(RecyclerView parent) {
            int spaceLeftInParent = parent.getWidth();
            spaceLeftInParent -= getOccupiedSpaceByChildren(parent);
            spaceLeftInParent -= getOccupiedSpaceByChildrenOffsets(parent);
            spaceLeftInParent -= parent.getPaddingEnd() + parent.getPaddingStart();
            return spaceLeftInParent;
        }

        private int estimateLastElementWidth(View view) {
            assert view instanceof ViewGroup;
            return ((ViewGroup) view).getChildCount()
                    * getContext().getResources().getDimensionPixelSize(
                            R.dimen.keyboard_accessory_tab_size);
        }

        private int getOccupiedSpaceByChildren(RecyclerView parent) {
            int occupiedSpace = 0;
            for (int i = 0; i < parent.getChildCount(); i++) {
                occupiedSpace += getOccupiedSpaceForView(parent.getChildAt(i));
            }
            return occupiedSpace;
        }

        private int getOccupiedSpaceForView(View view) {
            int occupiedSpace = view.getWidth();
            ViewGroup.LayoutParams lp = view.getLayoutParams();
            if (lp instanceof MarginLayoutParams) {
                occupiedSpace += ((MarginLayoutParams) lp).leftMargin;
                occupiedSpace += ((MarginLayoutParams) lp).rightMargin;
            }
            return occupiedSpace;
        }

        private int getOccupiedSpaceByChildrenOffsets(RecyclerView parent) {
            return (parent.getChildCount() - 1) * super.getItemOffsetInternal(null, null, null);
        }

        private boolean isLastItem(RecyclerView parent, View view, int itemCount) {
            return parent.getChildAdapterPosition(view) == itemCount - 1;
        }
    }

    /**
     * Constructor for inflating from XML.
     */
    public KeyboardAccessoryModernView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mKeyboardToggle = findViewById(R.id.show_keyboard);
        mKeyboardToggle.setImageDrawable(
                AppCompatResources.getDrawable(getContext(), R.drawable.ic_arrow_back_24dp));

        int pad = getResources().getDimensionPixelSize(R.dimen.keyboard_accessory_bar_item_padding);
        // Ensure the last element (although scrollable) is always end-aligned.
        mBarItemsView.addItemDecoration(new StickyLastItemDecoration());
        ViewCompat.setPaddingRelative(mBarItemsView, pad, 0, 0, 0);
    }

    void setKeyboardToggleVisibility(boolean hasActiveTab) {
        mKeyboardToggle.setVisibility(hasActiveTab ? VISIBLE : GONE);
        mBarItemsView.setVisibility(hasActiveTab ? GONE : VISIBLE);
    }

    void setShowKeyboardCallback(Runnable showKeyboardCallback) {
        mKeyboardToggle.setOnClickListener(
                showKeyboardCallback == null ? null : view -> showKeyboardCallback.run());
    }
}
