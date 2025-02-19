// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_bottom_toolbar.h"

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_new_tab_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridBottomToolbar {
  UIBarButtonItem* _spaceItem;
  UIImage* _transparentBackground;
  UIImage* _translucentBackground;
}

- (void)hide {
  self.newTabButton.button.alpha = 0.0;
}

- (void)show {
  self.newTabButton.button.alpha = 1.0;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // The first time this moves to a superview, perform the view setup.
  if (newSuperview && self.subviews.count == 0) {
    [self setupViews];
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLayout];
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (page == _page)
    return;
  _page = page;
  self.newTabButton.page = page;
  [self updateLayout];
}

#pragma mark - Private

- (void)updateLayout {
  if (self.page == TabGridPageRemoteTabs) {
    if ([self shouldUseCompactLayout]) {
      [self setItems:@[ _spaceItem, self.trailingButton ]];
      [self setBackgroundImage:_translucentBackground
            forToolbarPosition:UIBarPositionAny
                    barMetrics:UIBarMetricsDefault];
    } else {
      [self setItems:@[]];
      [self setBackgroundImage:_transparentBackground
            forToolbarPosition:UIToolbarPositionAny
                    barMetrics:UIBarMetricsDefault];
    }
  } else {
    if ([self shouldUseCompactLayout]) {
      self.newTabButton.sizeClass = TabGridNewTabButtonSizeClassSmall;
      [self setItems:@[
        self.leadingButton, _spaceItem, _newTabButton, _spaceItem,
        self.trailingButton
      ]];
      [self setBackgroundImage:_translucentBackground
            forToolbarPosition:UIBarPositionAny
                    barMetrics:UIBarMetricsDefault];
    } else {
      self.newTabButton.sizeClass = TabGridNewTabButtonSizeClassLarge;
      [self setItems:@[ _spaceItem, _newTabButton ]];
      [self setBackgroundImage:_transparentBackground
            forToolbarPosition:UIToolbarPositionAny
                    barMetrics:UIBarMetricsDefault];
    }
  }
}

- (void)setupViews {
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.barStyle = UIBarStyleBlack;
  self.translucent = YES;
  // Remove the border of UIToolbar.
  [self setShadowImage:[[UIImage alloc] init]
      forToolbarPosition:UIBarPositionAny];

  _leadingButton = [[UIBarButtonItem alloc] init];
  _leadingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _trailingButton = [[UIBarButtonItem alloc] init];
  _trailingButton.style = UIBarButtonItemStyleDone;
  _trailingButton.tintColor = UIColorFromRGB(kTabGridToolbarTextButtonColor);

  _newTabButton = [[TabGridNewTabButton alloc] init];

  _spaceItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];

  // Store the translucent background generated by self.translucent=YES.
  _translucentBackground =
      [self backgroundImageForToolbarPosition:UIBarPositionAny
                                   barMetrics:UIBarMetricsDefault];
  _transparentBackground = [[UIImage alloc] init];

  [self updateLayout];
}

// Returns YES if should use compact bottom toolbar layout.
- (BOOL)shouldUseCompactLayout {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassRegular &&
         self.traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact;
}

@end
