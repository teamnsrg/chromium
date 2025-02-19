// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Used to create fake data for both passwords and autofill.
 * These sections are related, so it made sense to share this.
 */

function FakeDataMaker() {}

/**
 * Creates a single item for the list of passwords.
 * @param {string=} url
 * @param {string=} username
 * @param {number=} passwordLength
 * @param {number=} id
 * @return {chrome.passwordsPrivate.PasswordUiEntry}
 */
FakeDataMaker.passwordEntry = function(url, username, passwordLength, id) {
  // Generate fake data if param is undefined.
  url = url || FakeDataMaker.patternMaker_('www.xxxxxx.com', 16);
  username = username || FakeDataMaker.patternMaker_('user_xxxxx', 16);
  passwordLength = passwordLength || Math.floor(Math.random() * 15) + 3;
  id = id || 0;

  return {
    loginPair: {
      urls: {
        origin: 'http://' + url + '/login',
        shown: url,
        link: 'http://' + url + '/login',
      },
      username: username,
    },
    numCharactersInPassword: passwordLength,
    id: id,
  };
};

/**
 * Creates a single item for the list of password exceptions.
 * @param {string=} url
 * @param {number=} id
 * @return {chrome.passwordsPrivate.ExceptionEntry}
 */
FakeDataMaker.exceptionEntry = function(url, id) {
  url = url || FakeDataMaker.patternMaker_('www.xxxxxx.com', 16);
  id = id || 0;
  return {
    urls: {
      origin: 'http://' + url + '/login',
      shown: url,
      link: 'http://' + url + '/login',
    },
    id: id,
  };
};

/**
 * Creates a new fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
FakeDataMaker.emptyAddressEntry = function() {
  return {};
};

/**
 * Creates a fake address entry for testing.
 * @return {!chrome.autofillPrivate.AddressEntry}
 */
FakeDataMaker.addressEntry = function() {
  const ret = {};
  ret.guid = FakeDataMaker.makeGuid_();
  ret.fullNames = ['John Doe'];
  ret.companyName = 'Google';
  ret.addressLines = FakeDataMaker.patternMaker_('xxxx Main St', 10);
  ret.addressLevel1 = 'CA';
  ret.addressLevel2 = 'Venice';
  ret.postalCode = FakeDataMaker.patternMaker_('xxxxx', 10);
  ret.countryCode = 'US';
  ret.phoneNumbers = [FakeDataMaker.patternMaker_('(xxx) xxx-xxxx', 10)];
  ret.emailAddresses = [FakeDataMaker.patternMaker_('userxxxx@gmail.com', 16)];
  ret.languageCode = 'EN-US';
  ret.metadata = {isLocal: true};
  ret.metadata.summaryLabel = ret.fullNames[0];
  ret.metadata.summarySublabel = ', ' + ret.addressLines;
  return ret;
};

/**
 * Creates a new empty credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
FakeDataMaker.emptyCreditCardEntry = function() {
  const now = new Date();
  const expirationMonth = now.getMonth() + 1;
  const ret = {};
  ret.expirationMonth = expirationMonth.toString();
  ret.expirationYear = now.getFullYear().toString();
  return ret;
};

/**
 * Creates a new random credit card entry for testing.
 * @return {!chrome.autofillPrivate.CreditCardEntry}
 */
FakeDataMaker.creditCardEntry = function() {
  const ret = {};
  ret.guid = FakeDataMaker.makeGuid_();
  ret.name = 'Jane Doe';
  ret.cardNumber = FakeDataMaker.patternMaker_('xxxx xxxx xxxx xxxx', 10);
  ret.expirationMonth = Math.ceil(Math.random() * 11).toString();
  ret.expirationYear = (2016 + Math.floor(Math.random() * 5)).toString();
  ret.metadata = {isLocal: true};
  const cards = ['Visa', 'Mastercard', 'Discover', 'Card'];
  const card = cards[Math.floor(Math.random() * cards.length)];
  ret.metadata.summaryLabel = card + ' ' +
      '****' + ret.cardNumber.substr(-4);
  return ret;
};

/**
 * Creates a new random GUID for testing.
 * @return {string}
 * @private
 */
FakeDataMaker.makeGuid_ = function() {
  return FakeDataMaker.patternMaker_(
      'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx', 16);
};

/**
 * Replaces any 'x' in a string with a random number of the base.
 * @param {string} pattern The pattern that should be used as an input.
 * @param {number} base The number base. ie: 16 for hex or 10 for decimal.
 * @return {string}
 * @private
 */
FakeDataMaker.patternMaker_ = function(pattern, base) {
  return pattern.replace(/x/g, function() {
    return Math.floor(Math.random() * base).toString(base);
  });
};

/** @constructor */
function PasswordManagerExpectations() {
  this.requested = {
    passwords: 0,
    exceptions: 0,
    plaintextPassword: 0,
  };

  this.removed = {
    passwords: 0,
    exceptions: 0,
  };

  this.listening = {
    passwords: 0,
    exceptions: 0,
  };
}

/** Helper class to track AutofillManager expectations. */
class AutofillManagerExpectations {
  constructor() {
    this.requestedAddresses = 0;
    this.listeningAddresses = 0;
  }
}

/**
 * Test implementation
 * @implements {AutofillManager}
 * @constructor
 */
function TestAutofillManager() {
  this.actual_ = new AutofillManagerExpectations();

  // Set these to have non-empty data.
  this.data = {
    addresses: [],
  };

  // Holds the last callbacks so they can be called when needed.
  this.lastCallback = {
    addAddressListChangedListener: null,
  };
}

TestAutofillManager.prototype = {
  /** @override */
  addAddressListChangedListener: function(listener) {
    this.actual_.listeningAddresses++;
    this.lastCallback.addAddressListChangedListener = listener;
  },

  /** @override */
  removeAddressListChangedListener: function(listener) {
    this.actual_.listeningAddresses--;
  },

  /** @override */
  getAddressList: function(callback) {
    this.actual_.requestedAddresses++;
    callback(this.data.addresses);
  },

  /**
   * Verifies expectations.
   * @param {!AutofillManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedAddresses, actual.requestedAddresses);
    assertEquals(expected.listeningAddresses, actual.listeningAddresses);
  },
};

/** Helper class to track PaymentsManager expectations. */
class PaymentsManagerExpectations {
  constructor() {
    this.requestedCreditCards = 0;
    this.requestedLocalCreditCards = 0;
    this.requestedServerCreditCards = 0;
    this.listeningCreditCards = 0;
    this.listeningLocalCreditCards = 0;
    this.listeningServerCreditCards = 0;
  }
}

/**
 * Test implementation
 * @implements {PaymentsManager}
 * @constructor
 */
function TestPaymentsManager() {
  this.actual_ = new PaymentsManagerExpectations();

  // Set these to have non-empty data.
  this.data = {
    creditCards: [],
    localCreditCards: [],
    serverCreditCards: [],
  };

  // Holds the last callbacks so they can be called when needed.
  this.lastCallback = {
    addCreditCardListChangedListener: null,
    addLocalCreditCardListChangedListener: null,
    addServerCreditCardListChangedListener: null,
  };
}

TestPaymentsManager.prototype = {
  /** @override */
  addCreditCardListChangedListener: function(listener) {
    this.actual_.listeningCreditCards++;
    this.lastCallback.addCreditCardListChangedListener = listener;
  },

  /** @override */
  addLocalCreditCardListChangedListener: function(listener) {
    this.actual_.listeningLocalCreditCards++;
    this.lastCallback.addLocalCreditCardListChangedListener = listener;
  },

  /** @override */
  addServerCreditCardListChangedListener: function(listener) {
    this.actual_.listeningServerCreditCards++;
    this.lastCallback.addServerCreditCardListChangedListener = listener;
  },

  /** @override */
  removeCreditCardListChangedListener: function(listener) {
    this.actual_.listeningCreditCards--;
  },

  /** @override */
  removeLocalCreditCardListChangedListener: function(listener) {
    this.actual_.listeningLocalCreditCards--;
  },

  /** @override */
  removeServerCreditCardListChangedListener: function(listener) {
    this.actual_.listeningServerCreditCards--;
  },

  /** @override */
  getCreditCardList: function(callback) {
    this.actual_.requestedCreditCards++;
    callback(this.data.creditCards);
  },

  /** @override */
  getLocalCreditCardList: function(callback) {
    this.actual_.requestedLocalCreditCards++;
    callback(this.data.localCreditCards);
  },

  /** @override */
  getServerCreditCardList: function(callback) {
    this.actual_.requestedServerCreditCards++;
    callback(this.data.serverCreditCards);
  },

  /**
   * Verifies expectations.
   * @param {!PaymentsManagerExpectations} expected
   */
  assertExpectations: function(expected) {
    const actual = this.actual_;
    assertEquals(expected.requestedCreditCards, actual.requestedCreditCards);
    assertEquals(
        expected.requestedLocalCreditCards, actual.requestedLocalCreditCards);
    assertEquals(
        expected.requestedServerCreditCards, actual.requestedServerCreditCards);
    assertEquals(expected.listeningCreditCards, actual.listeningCreditCards);
    assertEquals(
        expected.listeningLocalCreditCards, actual.listeningLocalCreditCards);
    assertEquals(
        expected.listeningServerCreditCards, actual.listeningServerCreditCards);
  },
};
