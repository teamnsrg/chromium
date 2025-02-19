// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_
#define COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_

#include <memory>

namespace language {

class LanguageCodeLocator;

std::unique_ptr<LanguageCodeLocator> GetLanguageCodeLocator();

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CONTENT_BROWSER_LANGUAGE_CODE_LOCATOR_PROVIDER_H_
