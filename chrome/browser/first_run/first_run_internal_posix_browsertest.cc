// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <signal.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace first_run {

class FirstRunInternalPosixTest : public InProcessBrowserTest {
 protected:
  FirstRunInternalPosixTest() = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    internal::ForceFirstRunDialogShownForTesting(true);
    // The modal dialog will spawn and spin a nested RunLoop when
    // content::BrowserTestBase::SetUp() invokes content::ContentMain().
    // BrowserTestBase sets GetContentMainParams()->ui_task before this, but the
    // ui_task isn't actually Run() until after the dialog is spawned in
    // ChromeBrowserMainParts::PreMainMessageLoopRunImpl(). Instead, try to
    // inspect state by posting a task to run in that nested RunLoop.
    // There's no MessageLoop to enqueue a task on yet, there's also no
    // content::NotificationService, or anything else sensible that would allow
    // us to hook in a task. So use a testing-only Closure.
    GetBeforeShowFirstRunDialogHookForTesting() = base::BindOnce(
        &FirstRunInternalPosixTest::SetupNestedTask, base::Unretained(this));
    EXPECT_FALSE(inspected_state_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    EXPECT_TRUE(inspected_state_);
    // The test closure should have run. But clear the global in case it hasn't.
    EXPECT_FALSE(GetBeforeShowFirstRunDialogHookForTesting());
    GetBeforeShowFirstRunDialogHookForTesting().Reset();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

 private:
  // A task run immediately before first_run::DoPostImportPlatformSpecificTasks
  // shows the first-run dialog.
  void SetupNestedTask() {
    EXPECT_TRUE(base::SequencedTaskRunnerHandle::Get());
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&FirstRunInternalPosixTest::InspectState,
                              base::Unretained(this)));
  }

  // A task queued up to run once the first-run dialog starts pumping messages
  // in its modal loop.
  void InspectState() {
    // Send a signal to myself. This should post a task for the next run loop
    // iteration to set browser_shutdown::IsTryingToQuit(), and interrupt the
    // RunLoop.
    raise(SIGTERM);
    inspected_state_ = true;
  }

  bool inspected_state_ = false;

  DISALLOW_COPY_AND_ASSIGN(FirstRunInternalPosixTest);
};

// Test the first run flow for showing the modal dialog that surfaces the first
// run dialog. Ensure browser startup safely handles a signal while the modal
// RunLoop is running.
IN_PROC_BROWSER_TEST_F(FirstRunInternalPosixTest, HandleSigterm) {
  // Never reached. PreMainMessageLoopRunImpl() should return before this task
  // is run.
  ADD_FAILURE() << "Should never be called";
}

}  // namespace first_run
