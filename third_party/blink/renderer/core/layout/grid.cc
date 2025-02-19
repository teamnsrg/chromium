// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/grid.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/layout/layout_grid.h"

namespace blink {

namespace {

static inline GridTrackSizingDirection OrthogonalDirection(
    GridTrackSizingDirection direction) {
  return direction == kForRows ? kForColumns : kForRows;
};

}  // namespace

std::unique_ptr<Grid> Grid::Create(const LayoutGrid* layout_grid) {
  return base::WrapUnique(new ListGrid(layout_grid));
}

Grid::Grid(const LayoutGrid* grid) : order_iterator_(grid) {}

void Grid::SetSmallestTracksStart(int row_start, int column_start) {
  smallest_row_start_ = row_start;
  smallest_column_start_ = column_start;
}

int Grid::SmallestTrackStart(GridTrackSizingDirection direction) const {
  return direction == kForRows ? smallest_row_start_ : smallest_column_start_;
}

GridArea Grid::GridItemArea(const LayoutBox& item) const {
  DCHECK(grid_item_area_.Contains(&item));
  return grid_item_area_.at(&item);
}

void Grid::SetGridItemArea(const LayoutBox& item, GridArea area) {
  grid_item_area_.Set(&item, area);
}

size_t Grid::GridItemPaintOrder(const LayoutBox& item) const {
  return grid_items_indexes_map_.at(&item);
}

void Grid::SetGridItemPaintOrder(const LayoutBox& item, size_t order) {
  grid_items_indexes_map_.Set(&item, order);
}

#if DCHECK_IS_ON()
bool Grid::HasAnyGridItemPaintOrder() const {
  return !grid_items_indexes_map_.IsEmpty();
}
#endif

void Grid::SetAutoRepeatTracks(size_t auto_repeat_rows,
                               size_t auto_repeat_columns) {
  DCHECK_GE(static_cast<unsigned>(kGridMaxTracks),
            NumTracks(kForRows) + auto_repeat_rows);
  DCHECK_GE(static_cast<unsigned>(kGridMaxTracks),
            NumTracks(kForColumns) + auto_repeat_columns);
  auto_repeat_rows_ = auto_repeat_rows;
  auto_repeat_columns_ = auto_repeat_columns;
}

size_t Grid::AutoRepeatTracks(GridTrackSizingDirection direction) const {
  return direction == kForRows ? auto_repeat_rows_ : auto_repeat_columns_;
}

void Grid::SetAutoRepeatEmptyColumns(
    std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_columns) {
  auto_repeat_empty_columns_ = std::move(auto_repeat_empty_columns);
}

void Grid::SetAutoRepeatEmptyRows(
    std::unique_ptr<OrderedTrackIndexSet> auto_repeat_empty_rows) {
  auto_repeat_empty_rows_ = std::move(auto_repeat_empty_rows);
}

bool Grid::HasAutoRepeatEmptyTracks(GridTrackSizingDirection direction) const {
  return direction == kForColumns ? !!auto_repeat_empty_columns_
                                  : !!auto_repeat_empty_rows_;
}

bool Grid::IsEmptyAutoRepeatTrack(GridTrackSizingDirection direction,
                                  size_t line) const {
  DCHECK(HasAutoRepeatEmptyTracks(direction));
  return AutoRepeatEmptyTracks(direction)->Contains(line);
}

OrderedTrackIndexSet* Grid::AutoRepeatEmptyTracks(
    GridTrackSizingDirection direction) const {
  DCHECK(HasAutoRepeatEmptyTracks(direction));
  return direction == kForColumns ? auto_repeat_empty_columns_.get()
                                  : auto_repeat_empty_rows_.get();
}

GridSpan Grid::GridItemSpan(const LayoutBox& grid_item,
                            GridTrackSizingDirection direction) const {
  GridArea area = GridItemArea(grid_item);
  return direction == kForColumns ? area.columns : area.rows;
}

void Grid::SetNeedsItemsPlacement(bool needs_items_placement) {
  needs_items_placement_ = needs_items_placement;

  if (!needs_items_placement) {
    ConsolidateGridDataStructure();
    return;
  }

  ClearGridDataStructure();
  grid_item_area_.clear();
  grid_items_indexes_map_.clear();
  smallest_row_start_ = 0;
  smallest_column_start_ = 0;
  auto_repeat_columns_ = 0;
  auto_repeat_rows_ = 0;
  auto_repeat_empty_columns_ = nullptr;
  auto_repeat_empty_rows_ = nullptr;
}

Grid::GridIterator::GridIterator(GridTrackSizingDirection direction,
                                 size_t fixed_track_index,
                                 size_t varying_track_index)
    : direction_(direction),
      row_index_((direction == kForColumns) ? varying_track_index
                                            : fixed_track_index),
      column_index_((direction == kForColumns) ? fixed_track_index
                                               : varying_track_index),
      child_index_(0) {}

ListGrid::GridCell* ListGrid::GridTrack::Find(size_t index) const {
  auto orthogonal_axis = OrthogonalDirection(direction_);
  for (auto* cell = cells_.Head(); cell;
       cell = cell->NextInDirection(direction_)) {
    size_t cell_index = cell->Index(orthogonal_axis);
    if (cell_index == index)
      return cell;
    if (cell_index > index)
      return nullptr;
  }
  return nullptr;
}

static int ComparePositions(size_t first, size_t second) {
  return first < second ? -1 : (first != second);
}

DoublyLinkedList<ListGrid::GridCell>::AddResult ListGrid::GridTrack::Insert(
    GridCell* cell) {
  cell->SetTraversalMode(direction_);

  return cells_.Insert(
      cell, [this](ListGrid::GridCell* first, ListGrid::GridCell* second) {
        // This is ugly but we need to do this in order the
        // DoublyLinkedList::Insert() algorithm to work at that code
        // only uses next_ and prev_.
        first->SetTraversalMode(direction_);
        second->SetTraversalMode(direction_);
        auto ortho_direction = OrthogonalDirection(direction_);
        return ComparePositions(first->Index(ortho_direction),
                                second->Index(ortho_direction));
      });
}

DoublyLinkedList<ListGrid::GridCell>::AddResult ListGrid::GridTrack::Insert(
    LayoutBox& item,
    const GridSpan& span) {
  auto compare_cells = [this](ListGrid::GridCell* first,
                              ListGrid::GridCell* second) {
    first->SetTraversalMode(direction_);
    second->SetTraversalMode(direction_);
    auto ortho_direction = OrthogonalDirection(direction_);
    return ComparePositions(first->Index(ortho_direction),
                            second->Index(ortho_direction));
  };

  size_t col_index = direction_ == kForColumns ? Index() : span.StartLine();
  size_t row_index = direction_ == kForColumns ? span.StartLine() : Index();

  auto result = cells_.Insert(
      base::WrapUnique(new GridCell(row_index, col_index)), compare_cells);
  auto* cell = result.node;
  for (auto index : span) {
    cell->AppendItem(item);

    if (index == span.EndLine() - 1)
      break;

    cell->SetTraversalMode(direction_);
    auto ortho_direction = OrthogonalDirection(direction_);
    if (!cell->Next() ||
        (cell->Next()->Index(ortho_direction) != (index + 1))) {
      size_t next_col_index = direction_ == kForColumns ? Index() : index + 1;
      size_t next_row_index = direction_ == kForColumns ? index + 1 : Index();
      auto next_cell =
          base::WrapUnique(new GridCell(next_row_index, next_col_index));
      auto result = InsertAfter(next_cell.get(), cell);
      if (result.is_new_entry)
        next_cell.release();
    }
    cell = cell->Next();
  }
  return result;
}

DoublyLinkedList<ListGrid::GridCell>::AddResult
ListGrid::GridTrack::InsertAfter(GridCell* cell, GridCell* insertion_point) {
  insertion_point->SetTraversalMode(direction_);
  cell->SetTraversalMode(direction_);
  if (auto* next = insertion_point->Next()) {
    if (next == cell)
      return {cell, false};
    // We need to set the traversal mode for the next cell as we're
    // going to insert in between and we need to properly update next_
    // and prev_ pointers.
    next->SetTraversalMode(direction_);
  }
  return cells_.InsertAfter(cell, insertion_point);
}

ListGrid::GridTrack::~GridTrack() {
  // We destroy cells just when disposing columns as we don't want to
  // double free them.
  // TODO(svillar): we need to eventually get rid of this different
  // destructors depending on the axis.
  if (direction_ == kForRows) {
    cells_.Clear();
    return;
  }

  while (!cells_.IsEmpty()) {
    cells_.Head()->SetTraversalMode(kForColumns);
    if (cells_.Head()->Next())
      cells_.Head()->Next()->SetTraversalMode(kForColumns);
    delete cells_.RemoveHead();
  }
}

const GridItemList& ListGrid::Cell(size_t row_index,
                                   size_t column_index) const {
  DEFINE_STATIC_LOCAL(const GridItemList, empty_vector, ());
  for (auto* row = rows_.Head(); row; row = row->Next()) {
    if (row->Index() == row_index) {
      auto* cell = row->Find(column_index);
      return cell ? cell->Items() : empty_vector;
    }
    if (row->Index() > row_index)
      return empty_vector;
  }
  return empty_vector;
}

ListGrid::GridTrack* ListGrid::InsertTracks(
    DoublyLinkedList<GridTrack>& tracks,
    const GridSpan& span,
    GridTrackSizingDirection direction) {
  auto compare_tracks = [](ListGrid::GridTrack* first,
                           ListGrid::GridTrack* second) {
    return ComparePositions(first->Index(), second->Index());
  };

  size_t start_line = span.StartLine();
  size_t end_line = span.EndLine();

  DoublyLinkedList<ListGrid::GridTrack>::AddResult result = tracks.Insert(
      base::WrapUnique(new GridTrack(start_line, direction)), compare_tracks);
  auto* track = result.node;
  DCHECK(track);

  auto* iter = track;
  for (size_t track_index = start_line + 1; iter && track_index < end_line;
       ++track_index) {
    if (!iter->Next() || track_index < iter->Next()->Index()) {
      tracks.InsertAfter(
          base::WrapUnique(new GridTrack(track_index, direction)), iter);
    }
    iter = iter->Next();
  }

  return track;
}

void ListGrid::Insert(LayoutBox& item, const GridArea& area) {
  DCHECK(area.rows.IsTranslatedDefinite() &&
         area.columns.IsTranslatedDefinite());
  EnsureGridSize(area.rows.EndLine(), area.columns.EndLine());

  GridTrack* first_row = InsertTracks(rows_, area.rows, kForRows);
  DCHECK(first_row);
  GridTrack* first_column = InsertTracks(columns_, area.columns, kForColumns);
  DCHECK(first_column);

  GridCell* above_cell = nullptr;
  GridTrack* row = first_row;
  for (auto row_index : area.rows) {
    auto result = row->Insert(item, area.columns);
    // We need to call Insert() for the first row of cells to get the
    // column pointers right. For the following rows we can use
    // InsertAfter() as it's cheaper (it doesn't traverse the
    // list). We need to keep track of the cell in the row above
    // (above_cell) in order to properly update the column next_ &
    // prev_ pointers.
    auto* cell_iter = result.node;
    auto* col_iter = first_column;
    while (col_iter && col_iter->Index() < area.columns.EndLine()) {
      if (row_index == area.rows.StartLine()) {
        col_iter->Insert(cell_iter);
      } else {
        col_iter->InsertAfter(cell_iter, above_cell);
        above_cell = above_cell->NextInDirection(kForRows);
      }
      cell_iter = cell_iter->NextInDirection(kForRows);
      col_iter = col_iter->Next();
    }
    above_cell = result.node;
    row = row->Next();
  }

  SetGridItemArea(item, area);
}

void ListGrid::EnsureGridSize(size_t maximum_row_size,
                              size_t maximum_column_size) {
  num_rows_ = std::max(num_rows_, maximum_row_size);
  num_columns_ = std::max(num_columns_, maximum_column_size);
}

void ListGrid::ClearGridDataStructure() {
  num_rows_ = num_columns_ = 0;
  while (!rows_.IsEmpty())
    delete rows_.RemoveHead();
  DCHECK(rows_.IsEmpty());
  while (!columns_.IsEmpty())
    delete columns_.RemoveHead();
  DCHECK(columns_.IsEmpty());
}

ListGrid::~ListGrid() {
  ClearGridDataStructure();
}

void ListGrid::GridCell::SetTraversalMode(GridTrackSizingDirection direction) {
  if (direction == direction_)
    return;
  direction_ = direction;
  std::swap(next_, next_ortho_);
  std::swap(prev_, prev_ortho_);
}

ListGrid::GridCell* ListGrid::GridCell::NextInDirection(
    GridTrackSizingDirection direction) const {
  return direction_ == direction ? next_ : next_ortho_;
}

std::unique_ptr<Grid::GridIterator> ListGrid::CreateIterator(
    GridTrackSizingDirection direction,
    size_t fixed_track_index,
    size_t varying_track_index) const {
  return base::WrapUnique(new ListGridIterator(
      *this, direction, fixed_track_index, varying_track_index));
}

ListGridIterator::ListGridIterator(const ListGrid& grid,
                                   GridTrackSizingDirection direction,
                                   size_t fixed_track_index,
                                   size_t varying_track_index)
    : GridIterator(direction, fixed_track_index, varying_track_index),
      grid_(grid) {}

LayoutBox* ListGridIterator::NextGridItem() {
  DCHECK(grid_.NumTracks(kForRows));
  DCHECK(grid_.NumTracks(kForColumns));

  bool is_row_axis = direction_ == kForColumns;
  if (!cell_node_) {
    auto* track = is_row_axis ? grid_.columns_.Head() : grid_.rows_.Head();
    DCHECK(track);
    const size_t fixed_index = is_row_axis ? column_index_ : row_index_;
    while (track && track->Index() != fixed_index)
      track = track->Next();

    if (!track)
      return nullptr;

    child_index_ = 0;
    cell_node_ = track->Cells().Head();
    DCHECK(cell_node_);
    DCHECK(!cell_node_->Items().IsEmpty());
    return cell_node_->Items()[child_index_++];
  }

  if (child_index_ < cell_node_->Items().size())
    return cell_node_->Items()[child_index_++];

  child_index_ = 0;
  cell_node_ = cell_node_->NextInDirection(direction_);
  if (!cell_node_)
    return nullptr;

  DCHECK(!cell_node_->Items().IsEmpty());
  return cell_node_->Items()[child_index_++];
}

std::unique_ptr<GridArea> ListGridIterator::NextEmptyGridArea(
    size_t fixed_track_span,
    size_t varying_track_span) {
  auto FindCellOrClosest = [](ListGrid::GridCell* cell_node,
                              GridTrackSizingDirection direction,
                              size_t index) {
    auto ortho_direction = OrthogonalDirection(direction);
    while (cell_node && cell_node->Index(direction) < index)
      cell_node = cell_node->NextInDirection(ortho_direction);

    return cell_node;
  };

  auto CreateUniqueGridArea = [this, fixed_track_span, varying_track_span]() {
    bool is_row_axis = direction_ == kForColumns;
    size_t row_span = is_row_axis ? varying_track_span : fixed_track_span;
    size_t column_span = is_row_axis ? fixed_track_span : varying_track_span;
    return std::make_unique<GridArea>(
        GridSpan::TranslatedDefiniteGridSpan(row_index_, row_index_ + row_span),
        GridSpan::TranslatedDefiniteGridSpan(column_index_,
                                             column_index_ + column_span));
  };

  auto CellIsInsideSpan = [](ListGrid::GridCell* cell_node,
                             GridTrackSizingDirection direction, size_t start,
                             size_t end) {
    DCHECK(cell_node);
    size_t cell_index = cell_node->Index(direction);
    return cell_index >= start && cell_index <= end;
  };

  auto orthogonal_axis = OrthogonalDirection(direction_);
  auto& tracks = grid_.Tracks(orthogonal_axis);

  bool is_row_axis = direction_ == kForColumns;
  auto& varying_index = is_row_axis ? row_index_ : column_index_;
  const size_t fixed_index = is_row_axis ? column_index_ : row_index_;
  const size_t end_fixed_span = fixed_index + fixed_track_span - 1;
  auto* track_node = tracks.Head();
  while (track_node && track_node->Index() < varying_index)
    track_node = track_node->Next();

  for (; track_node; track_node = track_node->Next()) {
    if (!track_node)
      return CreateUniqueGridArea();

    if (track_node->Index() - varying_index >= varying_track_span)
      return CreateUniqueGridArea();

    auto* cell_node =
        FindCellOrClosest(track_node->Cells().Head(), direction_, fixed_index);
    if (cell_node &&
        CellIsInsideSpan(cell_node, direction_, fixed_index, end_fixed_span))
      varying_index = track_node->Index() + 1;
    else if (track_node->Index() - varying_index >= varying_track_span)
      return CreateUniqueGridArea();
  }

  return CreateUniqueGridArea();
}

}  // namespace blink
