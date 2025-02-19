// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates the Camera App main object.
 * @constructor
 */
cca.App = function() {
  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = new cca.models.Gallery();

  /**
   * @type {cca.GalleryButton}
   * @private
   */
  this.galleryButton_ = new cca.GalleryButton(this.model_);

  /**
   * @type {cca.views.Browser}
   * @private
   */
  this.browserView_ = new cca.views.Browser(this.model_);

  // End of properties. Seal the object.
  Object.seal(this);

  document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

  document.title = chrome.i18n.getMessage('name');
  this.setupI18nElements_();
  this.setupToggles_();

  // Set up views navigation by their DOM z-order.
  cca.nav.setup([
    new cca.views.Camera(this.model_),
    new cca.views.MasterSettings(),
    new cca.views.GridSettings(),
    new cca.views.TimerSettings(),
    this.browserView_,
    new cca.views.Warning(),
    new cca.views.Dialog(),
  ]);
};

/*
 * Checks if it is applicable to use CrOS gallery app.
 * @return {boolean} Whether applicable or not.
 */
cca.App.useGalleryApp = function() {
  return chrome.fileManagerPrivate &&
      document.body.classList.contains('ext-fs');
};

/**
 * Sets up i18n messages on elements by i18n attributes.
 * @private
 */
cca.App.prototype.setupI18nElements_ = function() {
  var getElements = (attr) => document.querySelectorAll('[' + attr + ']');
  var getMessage = (element, attr) => chrome.i18n.getMessage(
      element.getAttribute(attr));
  var setAriaLabel = (element, attr) => element.setAttribute(
      'aria-label', getMessage(element, attr));

  getElements('i18n-content').forEach(
      (element) => element.textContent = getMessage(element, 'i18n-content'));
  getElements('i18n-aria').forEach(
      (element) => setAriaLabel(element, 'i18n-aria'));
  cca.tooltip.setup(getElements('i18n-label')).forEach(
      (element) => setAriaLabel(element, 'i18n-label'));
};

/**
 * Sets up toggles (checkbox and radio) by data attributes.
 * @private
 */
cca.App.prototype.setupToggles_ = function() {
  document.querySelectorAll('input').forEach((element) => {
    element.addEventListener('keypress', (event) =>
        cca.util.getShortcutIdentifier(event) == 'Enter' && element.click());

    var css = element.getAttribute('data-css');
    var key = element.getAttribute('data-key');
    var payload = () => {
      var keys = {};
      keys[key] = element.checked;
      return keys;
    };
    element.addEventListener('change', (event) => {
      if (css) {
        document.body.classList.toggle(css, element.checked);
      }
      if (event.isTrusted) {
        element.save();
        if (element.type == 'radio' && element.checked) {
          // Handle unchecked grouped sibling radios.
          var grouped = `input[type=radio][name=${element.name}]:not(:checked)`;
          document.querySelectorAll(grouped).forEach((radio) =>
              radio.dispatchEvent(new Event('change')) && radio.save());
        }
      }
    });
    element.toggleChecked = (checked) => {
      element.checked = checked;
      element.dispatchEvent(new Event('change')); // Trigger toggling css.
    };
    element.save = () => {
      return key && chrome.storage.local.set(payload());
    };
    if (key) {
      // Restore the previously saved state on startup.
      chrome.storage.local.get(payload(),
          (values) => element.toggleChecked(values[key]));
    }
  });
};

/**
 * Starts the app by loading the model and opening the camera-view.
 */
cca.App.prototype.start = function() {
  cca.models.FileSystem.initialize(() => {
    // Prompt to migrate pictures if needed.
    var message = chrome.i18n.getMessage('migratePicturesMsg');
    return cca.nav.open('dialog', message, false).then((acked) => {
      if (!acked) {
        throw new Error('no-migrate');
      }
    });
  }).then((external) => {
    document.body.classList.toggle('ext-fs', external);
    this.model_.addObserver(this.galleryButton_);
    if (!cca.App.useGalleryApp()) {
      this.model_.addObserver(this.browserView_);
    }
    this.model_.load();
    cca.nav.open('camera');
  }).catch((error) => {
    console.error(error);
    if (error && error.message == 'no-migrate') {
      chrome.app.window.current().close();
      return;
    }
    cca.nav.open('warning', 'filesystem-failure');
  });
};

/**
 * Handles pressed keys.
 * @param {Event} event Key press event.
 * @private
 */
cca.App.prototype.onKeyPressed_ = function(event) {
  cca.tooltip.hide(); // Hide shown tooltip on any keypress.
  cca.nav.onKeyPressed(event);
};

/**
 * @type {cca.App} Singleton of the App object.
 * @private
 */
cca.App.instance_ = null;

/**
 * Creates the App object and starts camera stream.
 */
document.addEventListener('DOMContentLoaded', () => {
  if (!cca.App.instance_) {
    cca.App.instance_ = new cca.App();
  }
  cca.App.instance_.start();
  chrome.app.window.current().show();
});
