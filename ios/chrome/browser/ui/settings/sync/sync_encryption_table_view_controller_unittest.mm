// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using testing::NiceMock;
using testing::Return;

std::unique_ptr<KeyedService> CreateNiceProfileSyncServiceMock(
    web::BrowserState* context) {
  browser_sync::ProfileSyncService::InitParams init_params =
      CreateProfileSyncServiceParamsForTest(
          nullptr, ios::ChromeBrowserState::FromBrowserState(context));
  return std::make_unique<NiceMock<browser_sync::ProfileSyncServiceMock>>(
      std::move(init_params));
}

class SyncEncryptionTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateNiceProfileSyncServiceMock));
    chrome_browser_state_ = test_cbs_builder.Build();
    ChromeTableViewControllerTest::SetUp();

    mock_profile_sync_service_ =
        static_cast<browser_sync::ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    ON_CALL(*mock_profile_sync_service_, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
    ON_CALL(*mock_profile_sync_service_->GetUserSettingsMock(),
            IsUsingSecondaryPassphrase())
        .WillByDefault(Return(true));

    CreateController();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[SyncEncryptionTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Weak, owned by |chrome_browser_state_|.
  browser_sync::ProfileSyncServiceMock* mock_profile_sync_service_;
};

TEST_F(SyncEncryptionTableViewControllerTest, TestModel) {
  CheckController();
  CheckTitleWithId(IDS_IOS_SYNC_ENCRYPTION_TITLE);

  EXPECT_EQ(1, NumberOfSections());

  NSInteger const kSection = 0;
  EXPECT_EQ(2, NumberOfItemsInSection(kSection));

  TableViewTextItem* accountItem = GetTableViewItem(kSection, 0);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_BASIC_ENCRYPTION_DATA),
              accountItem.text);

  TableViewTextItem* passphraseItem = GetTableViewItem(kSection, 1);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SYNC_FULL_ENCRYPTION_DATA),
              passphraseItem.text);
}

}  // namespace
