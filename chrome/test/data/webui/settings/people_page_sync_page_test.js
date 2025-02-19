// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_people_page_sync_page', function() {
  suite('SyncSettingsTests', function() {
    let syncPage = null;
    let browserProxy = null;
    let encryptWithGoogle = null;
    let encryptWithPassphrase = null;

    /**
     * Returns sync prefs with everything synced and no passphrase required.
     * @return {!settings.SyncPrefs}
     */
    function getSyncAllPrefs() {
      return {
        appsEnforced: false,
        appsRegistered: true,
        appsSynced: true,
        autofillEnforced: false,
        autofillRegistered: true,
        autofillSynced: true,
        bookmarksEnforced: false,
        bookmarksRegistered: true,
        bookmarksSynced: true,
        encryptAllData: false,
        encryptAllDataAllowed: true,
        enterGooglePassphraseBody: 'Enter Google passphrase.',
        enterPassphraseBody: 'Enter custom passphrase.',
        extensionsEnforced: false,
        extensionsRegistered: true,
        extensionsSynced: true,
        fullEncryptionBody: '',
        passphrase: '',
        passphraseRequired: false,
        passphraseTypeIsCustom: false,
        passwordsEnforced: false,
        passwordsRegistered: true,
        passwordsSynced: true,
        paymentsIntegrationEnabled: true,
        preferencesEnforced: false,
        preferencesRegistered: true,
        preferencesSynced: true,
        setNewPassphrase: false,
        syncAllDataTypes: true,
        tabsEnforced: false,
        tabsRegistered: true,
        tabsSynced: true,
        themesEnforced: false,
        themesRegistered: true,
        themesSynced: true,
        typedUrlsEnforced: false,
        typedUrlsRegistered: true,
        typedUrlsSynced: true,
      };
    }

    setup(function() {
      browserProxy = new TestSyncBrowserProxy();
      settings.SyncBrowserProxyImpl.instance_ = browserProxy;

      PolymerTest.clearBody();
      syncPage = document.createElement('settings-sync-page');
      settings.navigateTo(settings.routes.SYNC);

      document.body.appendChild(syncPage);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(syncPage.$$('#' + settings.PageStatus.CONFIGURE).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.TIMEOUT).hidden);
      assertTrue(syncPage.$$('#' + settings.PageStatus.SPINNER).hidden);

      // Start with Sync All with no encryption selected.
      cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
      Polymer.dom.flush();

      return test_util.waitForRender().then(() => {
        encryptWithGoogle =
            syncPage.$$('cr-radio-button[name="encrypt-with-google"]');
        encryptWithPassphrase =
            syncPage.$$('cr-radio-button[name="encrypt-with-passphrase"]');
        assertTrue(!!encryptWithGoogle);
        assertTrue(!!encryptWithPassphrase);
      });
    });

    teardown(function() {
      syncPage.remove();
    });

    test('NotifiesHandlerOfNavigation', function() {
      function testNavigateAway() {
        settings.navigateTo(settings.routes.PEOPLE);
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testNavigateBack() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        settings.navigateTo(settings.routes.SYNC);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      function testDetach() {
        browserProxy.resetResolver('didNavigateAwayFromSyncPage');
        syncPage.remove();
        return browserProxy.whenCalled('didNavigateAwayFromSyncPage');
      }

      function testRecreate() {
        browserProxy.resetResolver('didNavigateToSyncPage');
        syncPage = document.createElement('settings-sync-page');
        settings.navigateTo(settings.routes.SYNC);

        document.body.appendChild(syncPage);
        return browserProxy.whenCalled('didNavigateToSyncPage');
      }

      return browserProxy.whenCalled('didNavigateToSyncPage')
          .then(testNavigateAway)
          .then(testNavigateBack)
          .then(testDetach)
          .then(testRecreate);
    });

    test('SyncSectionLayout_NoUnifiedConsent_SignedIn', function() {
      const syncSection = syncPage.$$('#sync-section');
      const otherItems = syncPage.$$('#other-sync-items');

      syncPage.syncStatus = {signedIn: true, disabled: false};
      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();
      assertFalse(syncSection.hidden);
      assertTrue(syncPage.$$('#sync-separator').hidden);
      assertFalse(otherItems.classList.contains('list-frame'));
      assertFalse(!!otherItems.querySelector('list-item'));
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SignedIn', function() {
      const syncSection = syncPage.$$('#sync-section');
      const otherItems = syncPage.$$('#other-sync-items');

      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertFalse(syncSection.hidden);
      assertTrue(syncPage.$$('#sync-separator').hidden);
      assertTrue(otherItems.classList.contains('list-frame'));
      assertEquals(
          otherItems.querySelectorAll(':scope > .list-item').length, 4);

      // Test sync paused state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.REAUTHENTICATE
      };
      assertTrue(syncSection.hidden);
      assertFalse(syncPage.$$('#sync-separator').hidden);

      // Test passphrase error state.
      syncPage.syncStatus = {
        signedIn: true,
        disabled: false,
        hasError: true,
        statusAction: settings.StatusAction.ENTER_PASSPHRASE
      };
      assertFalse(syncSection.hidden);
      assertTrue(syncPage.$$('#sync-separator').hidden);
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SignedOut', function() {
      const syncSection = syncPage.$$('#sync-section');

      syncPage.syncStatus = {
        signedIn: false,
        disabled: false,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertTrue(syncSection.hidden);
      assertFalse(syncPage.$$('#sync-separator').hidden);
    });

    test('SyncSectionLayout_UnifiedConsentEnabled_SyncDisabled', function() {
      const syncSection = syncPage.$$('#sync-section');

      syncPage.syncStatus = {
        signedIn: false,
        disabled: true,
        hasError: false,
        statusAction: settings.StatusAction.NO_ACTION,
      };
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();
      assertTrue(syncSection.hidden);
    });

    test('LoadingAndTimeout', function() {
      const configurePage = syncPage.$$('#' + settings.PageStatus.CONFIGURE);
      const spinnerPage = syncPage.$$('#' + settings.PageStatus.SPINNER);
      const timeoutPage = syncPage.$$('#' + settings.PageStatus.TIMEOUT);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.SPINNER);
      assertTrue(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertFalse(spinnerPage.hidden);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.TIMEOUT);
      assertTrue(configurePage.hidden);
      assertFalse(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);

      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.CONFIGURE);
      assertFalse(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);

      // Should remain on the CONFIGURE page even if the passphrase failed.
      cr.webUIListenerCallback(
          'page-status-changed', settings.PageStatus.PASSPHRASE_FAILED);
      assertFalse(configurePage.hidden);
      assertTrue(timeoutPage.hidden);
      assertTrue(spinnerPage.hidden);
    });

    test('RadioBoxesEnabledWhenUnencrypted', function() {
      // Verify that the encryption radio boxes are enabled.
      assertFalse(encryptWithGoogle.disabled);
      assertFalse(encryptWithPassphrase.disabled);

      assertTrue(encryptWithGoogle.checked);

      // Select 'Encrypt with passphrase' to create a new passphrase.
      assertFalse(!!syncPage.$$('#create-password-box'));

      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      // Test that a sync prefs update does not reset the selection.
      cr.webUIListenerCallback('sync-prefs-changed', getSyncAllPrefs());
      Polymer.dom.flush();
      assertTrue(encryptWithPassphrase.checked);
    });

    test('ClickingLinkDoesNotChangeRadioValue', function() {
      assertFalse(encryptWithPassphrase.disabled);
      assertFalse(encryptWithPassphrase.checked);

      const link = encryptWithPassphrase.querySelector('a[href]');
      assertTrue(!!link);

      // Suppress opening a new tab, since then the test will continue running
      // on a background tab (which has throttled timers) and will timeout.
      link.target = '';
      link.href = '#';
      // Prevent the link from triggering a page navigation when tapped.
      // Breaks the test in Vulcanized mode.
      link.addEventListener('click', function(e) {
        e.preventDefault();
      });

      link.click();

      assertFalse(encryptWithPassphrase.checked);
    });

    test('SaveButtonDisabledWhenPassphraseOrConfirmationEmpty', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');

      passphraseInput.value = '';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = '';
      assertTrue(saveNewPassphrase.disabled);

      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';
      assertFalse(saveNewPassphrase.disabled);
    });

    test('CreatingPassphraseMismatchedPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'bar';

      saveNewPassphrase.click();
      Polymer.dom.flush();

      assertFalse(passphraseInput.invalid);
      assertTrue(passphraseConfirmationInput.invalid);

      assertFalse(syncPage.syncPrefs.encryptAllData);
    });

    test('CreatingPassphraseValidPassphrase', function() {
      encryptWithPassphrase.click();
      Polymer.dom.flush();

      assertTrue(!!syncPage.$$('#create-password-box'));
      const saveNewPassphrase = syncPage.$$('#saveNewPassphrase');
      assertTrue(!!saveNewPassphrase);

      const passphraseInput = syncPage.$$('#passphraseInput');
      const passphraseConfirmationInput =
          syncPage.$$('#passphraseConfirmationInput');
      passphraseInput.value = 'foo';
      passphraseConfirmationInput.value = 'foo';
      saveNewPassphrase.click();

      function verifyPrefs(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = true;
        expected.passphrase = 'foo';
        expected.encryptAllData = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        expected.fullEncryptionBody = 'Encrypted with custom passphrase';
        cr.webUIListenerCallback('sync-prefs-changed', expected);

        Polymer.dom.flush();

        return test_util.waitForRender(syncPage).then(() => {
          // Need to re-retrieve this, as a different show passphrase radio
          // button is shown once |syncPrefs.fullEncryptionBody| is non-empty.
          encryptWithPassphrase =
              syncPage.$$('cr-radio-button[name="encrypt-with-passphrase"]');

          // Assert that the radio boxes are disabled after encryption enabled.
          assertTrue(syncPage.$$('cr-radio-group').disabled);
          assertEquals('-1', encryptWithGoogle.getAttribute('tabindex'));
          assertEquals('-1', encryptWithPassphrase.getAttribute('tabindex'));
        });
      }
      return browserProxy.whenCalled('setSyncEncryption').then(verifyPrefs);
    });

    test('RadioBoxesHiddenWhenEncrypted', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      prefs.fullEncryptionBody = 'Sync already encrypted.';
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      Polymer.dom.flush();

      assertTrue(syncPage.$.encryptionDescription.hidden);
      assertTrue(syncPage.$.encryptionRadioGroupContainer.hidden);
    });

    test(
        'ExistingPassphraseSubmitButtonDisabledWhenExistingPassphraseEmpty',
        function() {
          const prefs = getSyncAllPrefs();
          prefs.encryptAllData = true;
          prefs.passphraseRequired = true;
          cr.webUIListenerCallback('sync-prefs-changed', prefs);

          syncPage.unifiedConsentEnabled = false;
          Polymer.dom.flush();

          const existingPassphraseInput =
              syncPage.$$('#existingPassphraseInput');
          const submitExistingPassphrase =
              syncPage.$$('#submitExistingPassphrase');

          existingPassphraseInput.value = '';
          assertTrue(submitExistingPassphrase.disabled);

          existingPassphraseInput.value = 'foo';
          assertFalse(submitExistingPassphrase.disabled);
        });

    test('EnterExistingWrongPassphrase', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'wrong';
      browserProxy.encryptionResponse = settings.PageStatus.PASSPHRASE_FAILED;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'wrong';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        Polymer.dom.flush();

        assertTrue(existingPassphraseInput.invalid);
      });
    });

    test('EnterExistingCorrectPassphrase', function() {
      const prefs = getSyncAllPrefs();
      prefs.encryptAllData = true;
      prefs.passphraseRequired = true;
      cr.webUIListenerCallback('sync-prefs-changed', prefs);

      syncPage.unifiedConsentEnabled = false;
      Polymer.dom.flush();

      const existingPassphraseInput = syncPage.$$('#existingPassphraseInput');
      assertTrue(!!existingPassphraseInput);
      existingPassphraseInput.value = 'right';
      browserProxy.encryptionResponse = settings.PageStatus.CONFIGURE;

      const submitExistingPassphrase = syncPage.$$('#submitExistingPassphrase');
      assertTrue(!!submitExistingPassphrase);
      submitExistingPassphrase.click();

      return browserProxy.whenCalled('setSyncEncryption').then(function(prefs) {
        const expected = getSyncAllPrefs();
        expected.setNewPassphrase = false;
        expected.passphrase = 'right';
        expected.encryptAllData = true;
        expected.passphraseRequired = true;
        assertEquals(JSON.stringify(expected), JSON.stringify(prefs));

        const newPrefs = getSyncAllPrefs();
        newPrefs.encryptAllData = true;
        cr.webUIListenerCallback('sync-prefs-changed', newPrefs);

        Polymer.dom.flush();

        // Verify that the encryption radio boxes are shown but disabled.
        assertTrue(syncPage.$$('cr-radio-group').disabled);
        assertEquals('-1', encryptWithGoogle.getAttribute('tabindex'));
        assertEquals('-1', encryptWithPassphrase.getAttribute('tabindex'));
      });
    });

    test('SyncAdvancedRow', function() {
      syncPage.unifiedConsentEnabled = true;
      Polymer.dom.flush();

      const syncAdvancedRow = syncPage.$$('#sync-advanced-row');
      assertFalse(syncAdvancedRow.hidden);

      syncAdvancedRow.click();
      Polymer.dom.flush();

      assertEquals(settings.routes.SYNC_ADVANCED, settings.getCurrentRoute());
    });

    if (!cr.isChromeOS) {
      test('SyncSetupCancel_UnifiedConsentDisabled', function() {
        syncPage.unifiedConsentEnabled = false;
        Polymer.dom.flush();

        const toast = syncPage.$$('cr-toast');
        assertTrue(!!toast);
        assertFalse(toast.open);
        syncPage.syncStatus = {setupInProgress: true};
        Polymer.dom.flush();
        assertTrue(toast.open);

        toast.querySelector('paper-button').click();

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('SyncSetupLeavePage UnifiedConsentDisabled', function() {
        syncPage.unifiedConsentEnabled = false;
        Polymer.dom.flush();

        settings.navigateTo(settings.routes.BASIC);

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertFalse(abort);
            });
      });

      test('SyncSetupCancel UnifiedConsentEnabled', function() {
        syncPage.diceEnabled = true;
        syncPage.unifiedConsentEnabled = true;
        syncPage.syncStatus = {
          signinAllowed: true,
          syncSystemEnabled: true,
          setupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();
        sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);

        const cancelButton =
            syncPage.$$('settings-sync-account-control')
                .shadowRoot.querySelector('#setup-buttons .secondary-button');
        assertTrue(!!cancelButton);

        // Clicking the setup cancel button opens the 'Cancel sync?' dialog.
        cancelButton.click();
        Polymer.dom.flush();

        assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
        assertTrue(!!syncPage.$$('#setupCancelDialog'));
        assertTrue(syncPage.$$('#setupCancelDialog').open);

        // Clicking the cancel button on the 'Cancel sync?' dialog closes the
        // dialog and removes it from the DOM.
        syncPage.$$('#setupCancelDialog')
            .querySelector('.cancel-button')
            .click();
        return test_util
            .eventToPromise('close', syncPage.$$('#setupCancelDialog'))
            .then(() => {
              Polymer.dom.flush();
              assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
              assertFalse(!!syncPage.$$('#setupCancelDialog'));

              // Clicking the setup cancel button shows the 'Cancel sync?'
              // dialog again.
              cancelButton.click();
              Polymer.dom.flush();
              assertTrue(syncPage.$$('#setupCancelDialog').open);

              // Clicking the confirm button on the dialog aborts sync.
              syncPage.$$('#setupCancelDialog')
                  .querySelector('.action-button')
                  .click();

              return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
                  .then(abort => {
                    assertTrue(abort);
                  });
            });
      });

      test('SyncSetupConfirm UnifiedConsentEnabled', function() {
        syncPage.diceEnabled = true;
        syncPage.unifiedConsentEnabled = true;
        syncPage.syncStatus = {
          signinAllowed: true,
          syncSystemEnabled: true,
          setupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();
        sync_test_util.simulateStoredAccounts([{email: 'foo@foo.com'}]);

        const confirmButton =
            syncPage.$$('settings-sync-account-control')
                .shadowRoot.querySelector('#setup-buttons .action-button');

        assertTrue(!!confirmButton);
        confirmButton.click();

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertFalse(abort);
            });
      });

      test('SyncSetupLeavePage UnifiedConsentEnabled', function() {
        syncPage.unifiedConsentEnabled = true;
        syncPage.syncStatus = {
          signinAllowed: true,
          syncSystemEnabled: true,
          setupInProgress: true,
          signedIn: true
        };
        Polymer.dom.flush();

        // Navigating away while setup is in progress opens the 'Cancel sync?'
        // dialog.
        settings.navigateTo(settings.routes.BASIC);
        Polymer.dom.flush();

        assertEquals(settings.routes.SYNC, settings.getCurrentRoute());
        assertTrue(syncPage.$$('#setupCancelDialog').open);

        // Clicking the confirm button on the dialog aborts sync.
        syncPage.$$('#setupCancelDialog')
            .querySelector('.action-button')
            .click();

        return browserProxy.whenCalled('didNavigateAwayFromSyncPage')
            .then(abort => {
              assertTrue(abort);
            });
      });

      test('ShowAccountRow', function() {
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.diceEnabled = true;
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.unifiedConsentEnabled = true;
        syncPage.syncStatus = {signinAllowed: false, syncSystemEnabled: false};
        Polymer.dom.flush();
        assertFalse(!!syncPage.$$('settings-sync-account-control'));
        syncPage.syncStatus = {signinAllowed: true, syncSystemEnabled: true};
        Polymer.dom.flush();
        assertTrue(!!syncPage.$$('settings-sync-account-control'));
      });
    }
  });
});
