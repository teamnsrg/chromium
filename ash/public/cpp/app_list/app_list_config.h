// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class FontList;
}

namespace app_list {

// Shared layout type information for app list. Use the instance() method to
// obtain the AppListConfig.
class ASH_PUBLIC_EXPORT AppListConfig {
 public:
  AppListConfig();
  ~AppListConfig();

  static const AppListConfig& instance();

  int grid_tile_width() const { return grid_tile_width_; }
  int grid_tile_height() const { return grid_tile_height_; }
  int grid_tile_spacing() const { return grid_tile_spacing_; }
  int grid_icon_dimension() const { return grid_icon_dimension_; }
  int grid_icon_bottom_padding() const { return grid_icon_bottom_padding_; }
  int grid_title_top_padding() const { return grid_title_top_padding_; }
  int grid_title_horizontal_padding() const {
    return grid_title_horizontal_padding_;
  }
  int grid_title_width() const { return grid_title_width_; }
  int grid_focus_dimension() const { return grid_focus_dimension_; }
  int grid_focus_corner_radius() const { return grid_focus_corner_radius_; }
  SkColor grid_title_color() const { return grid_title_color_; }
  int search_tile_icon_dimension() const { return search_tile_icon_dimension_; }
  int search_tile_badge_icon_dimension() const {
    return search_tile_badge_icon_dimension_;
  }
  int search_tile_badge_icon_offset() const {
    return search_tile_badge_icon_offset_;
  }
  int search_list_icon_dimension() const { return search_list_icon_dimension_; }
  int search_list_badge_icon_dimension() const {
    return search_list_badge_icon_dimension_;
  }
  int suggestion_chip_icon_dimension() const {
    return suggestion_chip_icon_dimension_;
  }
  int app_title_max_line_height() const { return app_title_max_line_height_; }
  const gfx::FontList& app_title_font() const { return app_title_font_; }
  int peeking_app_list_height() const { return peeking_app_list_height_; }
  int search_box_closed_top_padding() const {
    return search_box_closed_top_padding_;
  }
  int search_box_peeking_top_padding() const {
    return search_box_peeking_top_padding_;
  }
  int search_box_fullscreen_top_padding() const {
    return search_box_fullscreen_top_padding_;
  }
  int preferred_cols() const { return preferred_cols_; }
  int preferred_rows() const { return preferred_rows_; }
  int page_spacing() const { return page_spacing_; }
  int expand_arrow_tile_height() const { return expand_arrow_tile_height_; }
  int folder_bubble_radius() const { return folder_bubble_radius_; }
  int folder_bubble_y_offset() const { return folder_bubble_y_offset_; }
  int folder_icon_dimension() const { return folder_icon_dimension_; }
  int folder_unclipped_icon_dimension() const {
    return folder_unclipped_icon_dimension_;
  }
  int item_icon_in_folder_icon_dimension() const {
    return item_icon_in_folder_icon_dimension_;
  }
  int folder_icon_radius() const { return folder_icon_radius_; }
  int folder_background_radius() const { return folder_background_radius_; }
  int folder_bubble_color() const { return folder_bubble_color_; }
  int folder_dropping_circle_radius() const {
    return folder_dropping_circle_radius_;
  }
  int folder_dropping_delay() const { return folder_dropping_delay_; }
  SkColor folder_background_color() const { return folder_background_color_; }
  int page_flip_zone_size() const { return page_flip_zone_size_; }
  int grid_tile_spacing_in_folder() const {
    return grid_tile_spacing_in_folder_;
  }
  int shelf_height() const { return shelf_height_; }
  int blur_radius() const { return blur_radius_; }
  SkColor contents_background_color() const {
    return contents_background_color_;
  }
  SkColor grid_selected_color() const { return grid_selected_color_; }
  SkColor card_background_color() const { return card_background_color_; }
  int page_transition_duration_ms() const {
    return page_transition_duration_ms_;
  }
  int overscroll_page_transition_duration_ms() const {
    return overscroll_page_transition_duration_ms_;
  }
  int folder_transition_in_duration_ms() const {
    return folder_transition_in_duration_ms_;
  }
  int folder_transition_out_duration_ms() const {
    return folder_transition_out_duration_ms_;
  }
  size_t num_start_page_tiles() const { return num_start_page_tiles_; }
  size_t max_search_results() const { return max_search_results_; }
  size_t max_folder_pages() const { return max_folder_pages_; }
  size_t max_folder_items_per_page() const {
    return max_folder_items_per_page_;
  }
  size_t max_folder_name_chars() const { return max_folder_name_chars_; }
  float all_apps_opacity_start_px() const { return all_apps_opacity_start_px_; }
  float all_apps_opacity_end_px() const { return all_apps_opacity_end_px_; }
  ui::ResourceBundle::FontStyle search_result_title_font_style() const {
    return search_result_title_font_style_;
  }
  int search_tile_height() const { return search_tile_height_; }

  gfx::Size grid_icon_size() const {
    return gfx::Size(grid_icon_dimension_, grid_icon_dimension_);
  }

  gfx::Size grid_focus_size() const {
    return gfx::Size(grid_focus_dimension_, grid_focus_dimension_);
  }

  gfx::Size search_tile_icon_size() const {
    return gfx::Size(search_tile_icon_dimension_, search_tile_icon_dimension_);
  }

  gfx::Size search_tile_badge_icon_size() const {
    return gfx::Size(search_tile_badge_icon_dimension_,
                     search_tile_badge_icon_dimension_);
  }

  gfx::Size search_list_icon_size() const {
    return gfx::Size(search_list_icon_dimension_, search_list_icon_dimension_);
  }

  gfx::Size search_list_badge_icon_size() const {
    return gfx::Size(search_list_badge_icon_dimension_,
                     search_list_badge_icon_dimension_);
  }

  gfx::Size folder_icon_size() const {
    return gfx::Size(folder_icon_dimension_, folder_icon_dimension_);
  }

  gfx::Size folder_unclipped_icon_size() const {
    return gfx::Size(folder_unclipped_icon_dimension_,
                     folder_unclipped_icon_dimension_);
  }

  int folder_icon_insets() const {
    return (folder_unclipped_icon_dimension_ - folder_icon_dimension_) / 2;
  }

  gfx::Size item_icon_in_folder_icon_size() const {
    return gfx::Size(item_icon_in_folder_icon_dimension_,
                     item_icon_in_folder_icon_dimension_);
  }

  // Returns the dimension at which a result's icon should be displayed.
  int GetPreferredIconDimension(
      ash::SearchResultDisplayType display_type) const;

  // Returns the maximum number of items allowed in specified page in apps grid.
  int GetMaxNumOfItemsPerPage(int page) const;

 private:
  // The tile view's width and height of the item in apps grid view.
  const int grid_tile_width_;
  const int grid_tile_height_;

  // The spacing between tile views in apps grid view.
  const int grid_tile_spacing_;

  // The icon dimension of tile views in apps grid view.
  const int grid_icon_dimension_;

  // The icon bottom padding in tile views in apps grid view.
  const int grid_icon_bottom_padding_;

  // The title top and horizontal padding in tile views in apps grid view.
  const int grid_title_top_padding_;
  const int grid_title_horizontal_padding_;

  // The title width and color of tile views in apps grid view.
  const int grid_title_width_;
  const SkColor grid_title_color_;

  // The focus dimension and corner radius of tile views in apps grid view.
  const int grid_focus_dimension_;
  const int grid_focus_corner_radius_;

  // The icon dimension of tile views in search result page view.
  const int search_tile_icon_dimension_;

  // The badge icon dimension of tile views in search result page view.
  const int search_tile_badge_icon_dimension_;

  // The badge icon offset of tile views in search result page view.
  const int search_tile_badge_icon_offset_;

  // The icon dimension of list views in search result page view.
  const int search_list_icon_dimension_;

  // The badge background corner radius of list views in search result page
  // view.
  const int search_list_badge_icon_dimension_;

  // The suggestion chip icon dimension.
  const int suggestion_chip_icon_dimension_;

  // The maximum line height for app title in app list.
  const int app_title_max_line_height_;

  // The font for app title in app list.
  const gfx::FontList app_title_font_;

  // The height of app list in peeking mode.
  const int peeking_app_list_height_;

  // The top padding of search box in closed state.
  const int search_box_closed_top_padding_;

  // The top padding of search box in peeking state.
  const int search_box_peeking_top_padding_;

  // The top padding of search box in fullscreen state.
  const int search_box_fullscreen_top_padding_;

  // Preferred number of columns and rows in apps grid.
  const int preferred_cols_;
  const int preferred_rows_;

  // The spacing between each page.
  const int page_spacing_;

  // The tile height of expand arrow.
  const int expand_arrow_tile_height_;

  // The folder image bubble radius.
  const int folder_bubble_radius_;

  // The y offset of folder image bubble center.
  const int folder_bubble_y_offset_;

  // The icon dimension of folder.
  const int folder_icon_dimension_;

  // The unclipped icon dimension of folder.
  const int folder_unclipped_icon_dimension_;

  // The corner radius of folder icon.
  const int folder_icon_radius_;

  // The corner radius of folder background.
  const int folder_background_radius_;

  // The color of folder bubble.
  const int folder_bubble_color_;

  // The dimension of the item icon in folder icon.
  const int item_icon_in_folder_icon_dimension_;

  // Radius of the circle, in which if entered, show folder dropping preview
  // UI.
  const int folder_dropping_circle_radius_;

  // Delays in milliseconds to show folder dropping preview circle.
  const int folder_dropping_delay_;

  // The background color of folder.
  const SkColor folder_background_color_;

  // Width in pixels of the area on the sides that triggers a page flip.
  const int page_flip_zone_size_;

  // The spacing between tile views in folder.
  const int grid_tile_spacing_in_folder_;

  // The height/width of the shelf from the bottom/side of the screen.
  const int shelf_height_;

  // The blur radius used in the app list.
  const int blur_radius_;

  // The background color of app list overlay.
  const SkColor contents_background_color_;

  // The keyboard select color for grid views, which are on top of a black
  // shield view for new design (12% white).
  const SkColor grid_selected_color_;

  // The background color for views in search results page.
  const SkColor card_background_color_;

  // Duration in milliseconds for page transition.
  const int page_transition_duration_ms_;

  // Duration in milliseconds for over scroll page transition.
  const int overscroll_page_transition_duration_ms_;

  // Duration in milliseconds for fading in the target page when opening
  // or closing a folder, and the duration for the top folder icon animation
  // for flying in or out the folder.
  const int folder_transition_in_duration_ms_;

  // Duration in milliseconds for fading out the old page when opening or
  // closing a folder.
  const int folder_transition_out_duration_ms_;

  // The number of apps shown in the start page app grid.
  const size_t num_start_page_tiles_;

  // Maximum number of results to show in the launcher Search UI.
  const size_t max_search_results_;

  // Max pages allowed in a folder.
  const size_t max_folder_pages_;

  // Max items per page allowed in a folder.
  const size_t max_folder_items_per_page_;

  // Maximum length of the folder name in chars.
  const size_t max_folder_name_chars_;

  // Range of the height of centerline above screen bottom that all apps should
  // change opacity. NOTE: this is used to change page switcher's opacity as
  // well.
  const float all_apps_opacity_start_px_ = 8.0f;
  const float all_apps_opacity_end_px_ = 144.0f;

  // Font style for AppListSearchResultTileItemViews that are not suggested
  // apps.
  const ui::ResourceBundle::FontStyle search_result_title_font_style_;

  // The height of tiles in search result.
  const int search_tile_height_ = 90;
};

}  // namespace app_list

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_CONFIG_H_
