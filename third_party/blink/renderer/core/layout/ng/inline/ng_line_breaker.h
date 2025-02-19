// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGLineBreaker_h
#define NGLineBreaker_h

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_line_layout_opportunity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_result.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class Hyphenation;
class NGInlineBreakToken;
class NGInlineItem;

// The line breaker needs to know which mode its in to properly handle floats.
enum class NGLineBreakerMode { kContent, kMinContent, kMaxContent };

// Represents a line breaker.
//
// This class measures each NGInlineItem and determines items to form a line,
// so that NGInlineLayoutAlgorithm can build a line box from the output.
class CORE_EXPORT NGLineBreaker {
  STACK_ALLOCATED();

 public:
  NGLineBreaker(NGInlineNode,
                NGLineBreakerMode,
                const NGConstraintSpace&,
                const NGLineLayoutOpportunity&,
                const NGPositionedFloatVector& leading_floats,
                unsigned handled_leading_floats_index,
                const NGInlineBreakToken*,
                NGExclusionSpace*,
                Vector<LayoutObject*>* out_floats_for_min_max = nullptr);
  ~NGLineBreaker();

  // Compute the next line break point and produces NGInlineItemResults for
  // the line.
  void NextLine(NGLineInfo*);

  bool IsFinished() const { return item_index_ >= Items().size(); }

  // Create an NGInlineBreakToken for the last line returned by NextLine().
  scoped_refptr<NGInlineBreakToken> CreateBreakToken(const NGLineInfo&) const;

  // Compute NGInlineItemResult for an open tag item.
  // Returns true if this item has edge and may have non-zero inline size.
  static bool ComputeOpenTagResult(const NGInlineItem&,
                                   const NGConstraintSpace&,
                                   NGInlineItemResult*);

  // This enum is private, except for |WhitespaceStateForTesting()|. See
  // |whitespace_| member.
  enum class WhitespaceState {
    kLeading,
    kNone,
    kUnknown,
    kCollapsible,
    kCollapsed,
    kPreserved,
  };
  WhitespaceState TrailingWhitespaceForTesting() const {
    return trailing_whitespace_;
  }

 private:
  const String& Text() const { return items_data_.text_content; }
  const Vector<NGInlineItem>& Items() const { return items_data_.items; }

  NGInlineItemResult* AddItem(const NGInlineItem&, unsigned end_offset);
  NGInlineItemResult* AddItem(const NGInlineItem&);
  void SetLineEndFragment(scoped_refptr<const NGPhysicalTextFragment>);
  void ComputeCanBreakAfter(NGInlineItemResult*) const;

  void BreakLine();

  void PrepareNextLine();

  void UpdatePosition();
  void ComputeLineLocation() const;

  enum class LineBreakState {
    // The line breaking is complete.
    kDone,

    // Should complete the line at the earliest possible point.
    // Trailing spaces, <br>, or close tags should be included to the line even
    // when it is overflowing.
    kTrailing,

    // Looking for more items to fit into the current line.
    kContinue,
  };

  void HandleText(const NGInlineItem&);
  void HandleText(const NGInlineItem&, const ShapeResult& shape_result);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 LayoutUnit available_width);
  void BreakText(NGInlineItemResult*,
                 const NGInlineItem&,
                 const ShapeResult& shape_result,
                 LayoutUnit available_width);
  bool HandleTextForFastMinContent(NGInlineItemResult* item_result,
                                   const NGInlineItem& item,
                                   const ShapeResult& shape_result);

  scoped_refptr<ShapeResultView> TruncateLineEndResult(
      const NGInlineItemResult& item_result,
      unsigned end_offset);
  void UpdateShapeResult(NGInlineItemResult*);
  scoped_refptr<ShapeResult> ShapeText(const NGInlineItem& item,
                                       unsigned start,
                                       unsigned end);

  void HandleTrailingSpaces(const NGInlineItem&,
                            const ShapeResult& shape_result);
  void RemoveTrailingCollapsibleSpace();
  LayoutUnit TrailingCollapsibleSpaceWidth();
  void ComputeTrailingCollapsibleSpace();

  void AppendHyphen(const NGInlineItem& item);

  void HandleControlItem(const NGInlineItem&);
  void HandleBidiControlItem(const NGInlineItem&);
  void HandleAtomicInline(const NGInlineItem&);
  void HandleFloat(const NGInlineItem&);

  LayoutUnit ComputeFloatAncestorInlineEndSize() const;

  void HandleOpenTag(const NGInlineItem&);
  void HandleCloseTag(const NGInlineItem&);

  void HandleOverflow();
  void Rewind(unsigned new_end);

  void SetCurrentStyle(const ComputedStyle&);

  void MoveToNextOf(const NGInlineItem&);
  void MoveToNextOf(const NGInlineItemResult&);

  void ComputeBaseDirection();

  LayoutUnit AvailableWidth() const {
    return line_opportunity_.AvailableInlineSize();
  }
  LayoutUnit AvailableWidthToFit() const {
    return AvailableWidth().AddEpsilon();
  }

  // These fields are the output of the current line.
  // NGInlineItemResults is a pointer because the move operation is not cheap
  // due to its inline buffer.
  NGLineInfo* line_info_ = nullptr;
  NGInlineItemResults* item_results_ = nullptr;

  // Represents the current offset of the input.
  LineBreakState state_;
  unsigned item_index_ = 0;
  unsigned offset_ = 0;

  // |WhitespaceState| of the current end. When a line is broken, this indicates
  // the state of trailing whitespaces.
  WhitespaceState trailing_whitespace_;

  // The current position from inline_start. Unlike NGInlineLayoutAlgorithm
  // that computes position in visual order, this position in logical order.
  LayoutUnit position_;
  NGLineLayoutOpportunity line_opportunity_;

  NGInlineNode node_;

  // True if this line is the "first formatted line".
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool is_first_formatted_line_ = false;

  bool use_first_line_style_ = false;

  // True when current box allows line wrapping.
  bool auto_wrap_ = false;

  // True when current box has 'word-break/word-wrap: break-word'.
  bool break_anywhere_if_overflow_ = false;

  // Force LineBreakType::kBreakCharacter by ignoring the current style if
  // |break_anywhere_if_overflow_| is set. Set to find grapheme cluster
  // boundaries for 'break-word' after overflow.
  bool override_break_anywhere_ = false;

  // True when breaking at soft hyphens (U+00AD) is allowed.
  bool enable_soft_hyphen_ = true;

  // True in quirks mode or limited-quirks mode, which require line-height
  // quirks.
  // https://quirks.spec.whatwg.org/#the-line-height-calculation-quirk
  bool in_line_height_quirks_mode_ = false;

  // True when the line we are breaking has a list marker.
  bool has_list_marker_ = false;

  // Set when the line ended with a forced break. Used to setup the states for
  // the next line.
  bool is_after_forced_break_ = false;

  bool ignore_floats_ = false;

  // Set in quirks mode when we're not supposed to break inside table cells
  // between images, and between text and images.
  bool sticky_images_quirk_ = false;

  const NGInlineItemsData& items_data_;

  NGLineBreakerMode mode_;
  const NGConstraintSpace& constraint_space_;
  NGExclusionSpace* exclusion_space_;
  scoped_refptr<const ComputedStyle> current_style_;

  LazyLineBreakIterator break_iterator_;
  HarfBuzzShaper shaper_;
  ShapeResultSpacing<String> spacing_;
  bool previous_line_had_forced_break_ = false;
  const Hyphenation* hyphenation_ = nullptr;

  // Cache the result of |ComputeTrailingCollapsibleSpace| to avoid shaping
  // multiple times.
  struct TrailingCollapsibleSpace {
    NGInlineItemResult* item_result;
    scoped_refptr<const ShapeResultView> collapsed_shape_result;
  };
  base::Optional<TrailingCollapsibleSpace> trailing_collapsible_space_;

  // Keep track of handled float items. See HandleFloat().
  const NGPositionedFloatVector& leading_floats_;
  unsigned leading_floats_index_ = 0u;
  unsigned handled_leading_floats_index_;

  Vector<LayoutObject*>* out_floats_for_min_max_;

  // Keep the last item |HandleTextForFastMinContent()| has handled. This is
  // used to fallback the last word to |HandleText()|.
  const NGInlineItem* fast_min_content_item_ = nullptr;

  // The current base direction for the bidi algorithm.
  // This is copied from NGInlineNode, then updated after each forced line break
  // if 'unicode-bidi: plaintext'.
  TextDirection base_direction_;
};

}  // namespace blink

#endif  // NGLineBreaker_h
