// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */
Polymer({
  is: 'site-entry',

  behaviors: [SiteSettingsBehavior, cr.ui.FocusRowBehavior],

  properties: {
    /**
     * An object representing a group of sites with the same eTLD+1.
     * @type {!SiteGroup}
     */
    siteGroup: {
      type: Object,
      observer: 'onSiteGroupChanged_',
    },

    /**
     * The name to display beside the icon. If grouped_() is true, it will be
     * the eTLD+1 for all the origins, otherwise, it will return the host.
     * @private
     */
    displayName_: String,

    /**
     * The string to display when there is a non-zero number of cookies.
     * @private
     */
    cookieString_: String,

    /**
     * The position of this site-entry in its parent list.
     */
    listIndex: {
      type: Number,
      value: -1,
    },

    /**
     * The string to display showing the overall usage of this site-entry.
     * @private
     */
    overallUsageString_: String,

    /**
     * An array containing the strings to display showing the individual disk
     * usage for each origin in |siteGroup|.
     * @type {!Array<string>}
     * @private
     */
    originUsages_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * An array containing the strings to display showing the individual cookies
     * number for each origin in |siteGroup|.
     * @type {!Array<string>}
     * @private
     */
    cookiesNum_: {
      type: Array,
      value: function() {
        return [];
      }
    }
  },

  listeners: {
    'focus': 'onFocus_',
  },

  /** @private {?settings.LocalDataBrowserProxy} */
  localDataBrowserProxy_: null,

  /** @private {?Element} */
  button_: null,

  /** @override */
  created: function() {
    this.localDataBrowserProxy_ =
        settings.LocalDataBrowserProxyImpl.getInstance();
  },

  /** @override */
  detached: function() {
    if (this.button_) {
      this.unlisten(this.button_, 'keydown', 'onButtonKeydown_');
    }
  },

  /** @param {!KeyboardEvent} e */
  onButtonKeydown_: function(e) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  },

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {boolean}
   * @private
   */
  grouped_: function(siteGroup) {
    if (siteGroup) {
      return siteGroup.origins.length != 1;
    }
    return false;
  },

  /**
   * Returns a user-friendly name for the origin corresponding to |originIndex|.
   * If grouped_() is true and |originIndex| is not provided, returns the eTLD+1
   * for all the origins, otherwise, return the host for that origin.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @param {number} originIndex Index of the origin to get a user-friendly name
   *     for. If -1, returns the eTLD+1 name if any, otherwise defaults to the
   *     first origin.
   * @return {string} The user-friendly name.
   * @private
   */
  siteRepresentation_: function(siteGroup, originIndex) {
    if (!siteGroup) {
      return '';
    }
    if (this.grouped_(siteGroup) && originIndex == -1) {
      if (siteGroup.etldPlus1 != '') {
        return siteGroup.etldPlus1;
      }
      // Fall back onto using the host of the first origin, if no eTLD+1 name
      // was computed.
    }
    originIndex = this.getIndexBoundToOriginList_(siteGroup, originIndex);
    const url = this.toUrl(siteGroup.origins[originIndex].origin);
    return url.host;
  },

  /**
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  onSiteGroupChanged_: function(siteGroup) {
    this.displayName_ = this.siteRepresentation_(siteGroup, -1);

    // Update the button listener.
    if (this.button_) {
      this.unlisten(this.button_, 'keydown', 'onButtonKeydown_');
    }
    this.button_ = /** @type Element */
        (this.root.querySelector('#toggleButton *:not([hidden]) button'));
    this.listen(assert(this.button_), 'keydown', 'onButtonKeydown_');

    if (!this.grouped_(siteGroup)) {
      // Ensure ungrouped |siteGroup|s do not get stuck in an opened state.
      if (this.$.collapseChild.opened) {
        this.toggleCollapsible_();
      }
    }
    if (!siteGroup) {
      return;
    }
    this.calculateUsageInfo_(siteGroup);
    this.calculateNumberOfCookies_(siteGroup);
  },

  /**
   * Returns any non-HTTPS scheme/protocol for the origin corresponding to
   * |originIndex|. Otherwise, returns a empty string.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @param {number} originIndex Index of the origin to get the non-HTTPS scheme
   *     for. If -1, returns an empty string for the grouped |siteGroup|s but
   *     defaults to 0 for non-grouped.
   * @return {string} The scheme if non-HTTPS, or empty string if HTTPS.
   * @private
   */
  scheme_: function(siteGroup, originIndex) {
    if (!siteGroup || (this.grouped_(siteGroup) && originIndex == -1)) {
      return '';
    }
    originIndex = this.getIndexBoundToOriginList_(siteGroup, originIndex);

    const url = this.toUrl(siteGroup.origins[originIndex].origin);
    const scheme = url.protocol.replace(new RegExp(':*$'), '');
    /** @type{string} */ const HTTPS_SCHEME = 'https';
    if (scheme == HTTPS_SCHEME) {
      return '';
    }
    return scheme;
  },

  /**
   * Get an appropriate favicon that represents this group of eTLD+1 sites as a
   * whole.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @return {string} URL that is used for fetching the favicon
   * @private
   */
  getSiteGroupIcon_: function(siteGroup) {
    // TODO(https://crbug.com/835712): Implement heuristic for finding a good
    // favicon.
    return siteGroup.origins[0].origin;
  },

  /**
   * Calculates the amount of disk storage used by the given group of origins
   * and eTLD+1. Also updates the corresponding display strings.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  calculateUsageInfo_: function(siteGroup) {
    const getFormattedBytesForSize = (numBytes) => {
      return this.browserProxy.getFormattedBytes(numBytes);
    };

    let overallUsage = 0;
    this.originUsages_ = new Array(siteGroup.origins.length);
    siteGroup.origins.forEach((originInfo, i) => {
      overallUsage += originInfo.usage;
      if (this.grouped_(siteGroup)) {
        getFormattedBytesForSize(originInfo.usage).then((string) => {
          this.set(`originUsages_.${i}`, string);
        });
      }
    });

    getFormattedBytesForSize(overallUsage).then(string => {
      this.overallUsageString_ = string;
    });
  },

  /**
   * Calculates the number of cookies set on the given group of origins
   * and eTLD+1. Also updates the corresponding display strings.
   * @param {SiteGroup} siteGroup The eTLD+1 group of origins.
   * @private
   */
  calculateNumberOfCookies_: function(siteGroup) {
    const getCookieNumString = (numCookies) => {
      if (numCookies == 0) {
        return Promise.resolve('');
      }
      return this.localDataBrowserProxy_.getNumCookiesString(numCookies);
    };

    this.cookiesNum_ = new Array(siteGroup.origins.length);
    siteGroup.origins.forEach((originInfo, i) => {
      if (this.grouped_(siteGroup)) {
        getCookieNumString(originInfo.numCookies).then((string) => {
          this.set(`cookiesNum_.${i}`, string);
        });
      }
    });

    getCookieNumString(siteGroup.numCookies).then(string => {
      this.cookieString_ = string;
    });
  },

  /**
   * Array binding for the |originUsages_| array for use in the HTML.
   * @param {!{base: !Array<string>}} change The change record for the array.
   * @param {number} index The index of the array item.
   * @return {string}
   * @private
   */
  originUsagesItem_: function(change, index) {
    return change.base[index];
  },

  /**
   * Array binding for the |cookiesNum_| array for use in the HTML.
   * @param {!{base: !Array<string>}} change The change record for the array.
   * @param {number} index The index of the array item.
   * @return {string}
   * @private
   */
  originCookiesItem_: function(change, index) {
    return change.base[index];
  },

  /**
   * Navigates to the corresponding Site Details page for the given origin.
   * @param {string} origin The origin to navigate to the Site Details page for
   * it.
   * @private
   */
  navigateToSiteDetails_: function(origin) {
    this.fire(
        'site-entry-selected', {item: this.siteGroup, index: this.listIndex});
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
  },

  /**
   * A handler for selecting a site (by clicking on the origin).
   * @param {!{model: !{index: !number}}} e
   * @private
   */
  onOriginTap_: function(e) {
    this.navigateToSiteDetails_(this.siteGroup.origins[e.model.index].origin);
  },

  /**
   * A handler for clicking on a site-entry heading. This will either show a
   * list of origins or directly navigates to Site Details if there is only one.
   * @private
   */
  onSiteEntryTap_: function() {
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.grouped_(this.siteGroup)) {
      this.navigateToSiteDetails_(this.siteGroup.origins[0].origin);
      return;
    }
    this.toggleCollapsible_();

    // Make sure the expanded origins can be viewed without further scrolling
    // (in case |this| is already at the bottom of the viewport).
    this.scrollIntoViewIfNeeded();
  },

  /**
   * Toggles open and closed the list of origins if there is more than one.
   * @private
   */
  toggleCollapsible_: function() {
    const collapseChild =
        /** @type {IronCollapseElement} */ (this.$.collapseChild);
    collapseChild.toggle();
    this.$.toggleButton.setAttribute('aria-expanded', collapseChild.opened);
    this.$.expandIcon.toggleClass('icon-expand-more');
    this.$.expandIcon.toggleClass('icon-expand-less');
    this.fire('iron-resize');
  },

  /**
   * Opens the overflow menu at event target.
   * @param {!{target: !Element}} e
   * @private
   */
  showOverflowMenu_: function(e) {
    this.$.menu.get().showAt(e.target);
  },

  /** @private */
  onCloseDialog_: function(e) {
    e.target.closest('cr-dialog').close();
    this.$.menu.get().close();
  },

  /**
   * Confirms the resetting of all content settings for an origin.
   * @param {!Event} e
   * @private
   */
  onConfirmResetSettings_: function(e) {
    e.preventDefault();
    this.$.confirmResetSettings.showModal();
  },

  /**
   * Confirms the clearing of all storage data for an etld+1.
   * @param {!Event} e
   * @private
   */
  onConfirmClearData_: function(e) {
    e.preventDefault();
    this.$.confirmClearData.showModal();
  },

  /**
   * Resets all permissions for all origins listed in |siteGroup.origins|.
   * @param {!Event} e
   * @private
   */
  onResetSettings_: function(e) {
    const contentSettingsTypes = this.getCategoryList();
    for (let i = 0; i < this.siteGroup.origins.length; ++i) {
      const origin = this.siteGroup.origins[i].origin;
      this.browserProxy.setOriginPermissions(
          origin, contentSettingsTypes, settings.ContentSetting.DEFAULT);
      if (contentSettingsTypes.includes(
              settings.ContentSettingsTypes.PLUGINS)) {
        this.browserProxy.clearFlashPref(origin);
      }
      this.siteGroup.origins[i].hasPermissionSettings = false;
    }
    // Create a new |siteGroup| to make an observable change.
    const updatedSiteGroup = {
      etldPlus1: this.siteGroup.etldPlus1,
      numCookies: this.siteGroup.numCookies,
      origins: []
    };
    for (let i = 0; i < this.siteGroup.origins.length; ++i) {
      const updatedOrigin = this.siteGroup.origins[i];
      if (updatedOrigin.numCookies > 0 || updatedOrigin.usage > 0) {
        updatedOrigin.hasPermissionSettings = false;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
    if (updatedSiteGroup.origins.length > 0) {
      this.siteGroup = updatedSiteGroup;
    } else if (this.siteGroup.numCookies > 0) {
      // If there is no origin for this site group that has any data,
      // but the ETLD+1 has cookies in use, create a origin placeholder
      // for display purposes.
      const originPlaceHolder = {
        origin: 'http://' + this.siteGroup.etldPlus1 + '/',
        engagement: 0,
        usage: 0,
        numCookies: this.siteGroup.numCookies,
        hasPermissionSettings: false
      };
      updatedSiteGroup.origins.push(originPlaceHolder);
      this.siteGroup = updatedSiteGroup;
    } else {
      this.fire('delete-current-entry', {
        etldPlus1: this.siteGroup.etldPlus1,
      });
    }
    this.fire('iron-resize');
    this.onCloseDialog_(e);
  },

  /**
   * Clear data and cookies for an etldPlus1.
   * @param {!Event} e
   * @private
   */
  onClearData_: function(e) {
    // Clean up the SiteGroup.
    this.browserProxy.clearEtldPlus1DataAndCookies(this.siteGroup.etldPlus1);
    // Create a new |siteGroup| to make an observable change.
    const updatedSiteGroup = {
      etldPlus1: this.siteGroup.etldPlus1,
      numCookies: 0,
      origins: []
    };
    for (let i = 0; i < this.siteGroup.origins.length; ++i) {
      const updatedOrigin = this.siteGroup.origins[i];
      if (updatedOrigin.hasPermissionSettings) {
        updatedOrigin.numCookies = 0;
        updatedOrigin.usage = 0;
        updatedSiteGroup.origins.push(updatedOrigin);
      }
    }
    if (updatedSiteGroup.origins.length > 0) {
      this.siteGroup = updatedSiteGroup;
    } else {
      this.fire('delete-current-entry', {
        etldPlus1: this.siteGroup.etldPlus1,
      });
    }
    this.fire('iron-resize');
    this.onCloseDialog_(e);
  },

  /**
   * Formats the |label| string with |name|, using $<num> as markers.
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getFormatString_: function(label, name) {
    return loadTimeData.substituteString(label, name);
  },

  /**
   * Returns a valid index for an origin contained in |siteGroup.origins| by
   * clamping the given |index|. This also replaces undefined |index|es with 0.
   * Use this to prevent being given out-of-bounds indexes by dom-repeat when
   * scrolling an iron-list storing these site-entries too quickly.
   * @param {!number=} index
   * @return {number}
   * @private
   */
  getIndexBoundToOriginList_: function(siteGroup, index) {
    return Math.max(0, Math.min(index, siteGroup.origins.length - 1));
  },

  /**
   * Returns the correct class to apply depending on this site-entry's position
   * in a list.
   * @param {number} index
   * @private
   */
  getClassForIndex_: function(index) {
    if (index == 0) {
      return 'first';
    }
    return '';
  },

  /**
   * Focuses the first focusable button in this site-entry.
   * @private
   */
  onFocus_: function() {
    this.button_.focus();
  },
});
