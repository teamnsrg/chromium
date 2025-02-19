// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/demo_session_metrics_recorder.h"

#include <iostream>
#include <string>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_window_watcher.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using DemoModeApp = DemoSessionMetricsRecorder::DemoModeApp;

// How often to sample.
constexpr auto kSamplePeriod = base::TimeDelta::FromSeconds(1);

// How many periods to wait for user activity before discarding samples.
// This timeout is low because demo sessions tend to be very short. If we
// recorded samples for a full minute while the device is in between uses, we
// would bias our measurements toward whatever app was used last.
constexpr int kMaxPeriodsWithoutActivity =
    base::TimeDelta::FromSeconds(15) / kSamplePeriod;

// Maps a Chrome app ID to a DemoModeApp value for metrics.
DemoModeApp GetAppFromAppId(const std::string& app_id) {
  // Each version of the Highlights app is bucketed into the same value.
  if (app_id == extension_misc::kHighlightsAppId ||
      app_id == extension_misc::kHighlightsAlt1AppId ||
      app_id == extension_misc::kHighlightsAlt2AppId) {
    return DemoModeApp::kHighlights;
  }

  // Each version of the Screensaver app is bucketed into the same value.
  if (app_id == extension_misc::kScreensaverAppId ||
      app_id == extension_misc::kScreensaverAlt1AppId ||
      app_id == extension_misc::kScreensaverAlt2AppId) {
    return DemoModeApp::kScreensaver;
  }

  if (app_id == extension_misc::kCameraAppId)
    return DemoModeApp::kCamera;
  if (app_id == extension_misc::kFilesManagerAppId)
    return DemoModeApp::kFiles;
  if (app_id == extension_misc::kGeniusAppId)
    return DemoModeApp::kGetHelp;
  if (app_id == extension_misc::kGoogleKeepAppId)
    return DemoModeApp::kGoogleKeep;
  if (app_id == extensions::kWebStoreAppId)
    return DemoModeApp::kWebStore;
  if (app_id == extension_misc::kYoutubeAppId)
    return DemoModeApp::kYouTube;
  return DemoModeApp::kOtherChromeApp;
}

// Maps an ARC++ package name to a DemoModeApp value for metrics.
DemoModeApp GetAppFromPackageName(const std::string& package_name) {
  // Google apps.
  if (package_name == "com.google.Photos")
    return DemoModeApp::kGooglePhotos;
  if (package_name == "com.google.Sheets")
    return DemoModeApp::kGoogleSheets;
  if (package_name == "com.google.Slides")
    return DemoModeApp::kGoogleSlides;
  if (package_name == "com.android.vending")
    return DemoModeApp::kPlayStore;

  // Third-party apps.
  if (package_name == "com.gameloft.android.ANMP.GloftA8HMD")
    return DemoModeApp::kAsphalt8;
  if (package_name == "com.brakefield.painter")
    return DemoModeApp::kInfinitePainter;
  if (package_name == "com.myscript.nebo.demo")
    return DemoModeApp::kMyScriptNebo;
  if (package_name == "com.steadfastinnovation.android.projectpapyrus")
    return DemoModeApp::kSquid;

  return DemoModeApp::kOtherArcApp;
}

const std::string* GetArcPackageName(const aura::Window* window) {
  return window->GetProperty(kArcPackageNameKey);
}

const ShelfID GetShelfID(const aura::Window* window) {
  return ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
}

AppType GetAppType(const aura::Window* window) {
  return static_cast<AppType>(window->GetProperty(aura::client::kAppType));
}

// Maps the app-like thing in |window| to a DemoModeApp value for metrics.
DemoModeApp GetAppFromWindow(const aura::Window* window) {
  AppType app_type = GetAppType(window);
  if (app_type == AppType::ARC_APP) {
    // The ShelfID app id isn't used to identify ARC++ apps since it's a hash of
    // both the package name and the activity.
    const std::string* package_name = GetArcPackageName(window);
    return package_name ? GetAppFromPackageName(*package_name)
                        : DemoModeApp::kOtherArcApp;
  }

  std::string app_id = GetShelfID(window).app_id;

  // The Chrome "app" in the shelf is just the browser.
  if (app_id == extension_misc::kChromeAppId)
    return DemoModeApp::kBrowser;

  auto is_default = [](const std::string& app_id) {
    if (!features::IsMultiProcessMash())
      return app_id.empty();

    return base::StartsWith(app_id, ShelfWindowWatcher::kDefaultShelfIdPrefix,
                            base::CompareCase::SENSITIVE);
  };

  // If the window is the "browser" type, having an app ID other than the
  // default indicates a hosted/bookmark app.
  if (app_type == AppType::CHROME_APP ||
      (app_type == AppType::BROWSER && !is_default(app_id))) {
    return GetAppFromAppId(app_id);
  }

  if (app_type == AppType::BROWSER)
    return DemoModeApp::kBrowser;
  return DemoModeApp::kOtherWindow;
}

}  // namespace

// Observes for changes in a window's ArcPackageName property for the purpose of
// logging of unique launches of ARC apps.
class DemoSessionMetricsRecorder::UniqueAppsLaunchedArcPackageNameObserver
    : public aura::WindowObserver {
 public:
  explicit UniqueAppsLaunchedArcPackageNameObserver(
      DemoSessionMetricsRecorder* metrics_recorder)
      : metrics_recorder_(metrics_recorder), scoped_observer_(this) {}

  // aura::WindowObserver
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key != kArcPackageNameKey)
      return;

    const std::string* package_name = GetArcPackageName(window);

    if (package_name != nullptr)
      metrics_recorder_->RecordAppLaunch(*package_name);
    else
      VLOG(1) << "Got null ARC package name";

    window->RemoveObserver(this);
  }

  void ObserveWindow(aura::Window* window) { scoped_observer_.Add(window); }

  void OnWindowDestroyed(aura::Window* window) override {
    window->RemoveObserver(this);
  }

 private:
  DemoSessionMetricsRecorder* metrics_recorder_;
  ScopedObserver<aura::Window, UniqueAppsLaunchedArcPackageNameObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(UniqueAppsLaunchedArcPackageNameObserver);
};

DemoSessionMetricsRecorder::DemoSessionMetricsRecorder(
    std::unique_ptr<base::RepeatingTimer> timer)
    : timer_(std::move(timer)),
      observer_(this),
      unique_apps_arc_package_name_observer_(
          std::make_unique<UniqueAppsLaunchedArcPackageNameObserver>(this)) {
  // Outside of tests, use a normal repeating timer.
  if (!timer_.get())
    timer_ = std::make_unique<base::RepeatingTimer>();

  StartRecording();
  observer_.Add(ui::UserActivityDetector::Get());

  // Subscribe to window activation updates.  Even though this gets us
  // notifications for all window activations, we ignore the ARC
  // notifications because they don't contain the app_id.  We handle
  // accounting for ARC windows with OnTaskCreated.
  if (Shell::Get()->GetPrimaryRootWindow()) {
    activation_client_ = Shell::Get()->focus_controller();
    activation_client_->AddObserver(this);
  }
}

DemoSessionMetricsRecorder::~DemoSessionMetricsRecorder() {
  // Report any remaining stored samples on exit. (If the user went idle, there
  // won't be any.)
  ReportSamples();

  // Unsubscribe from window activation events.
  activation_client_->RemoveObserver(this);

  ReportUniqueAppsLaunched();
}

// This method will only record 1 launch for each unique app_id, regardless of
// how many times it is called with that app_id.
void DemoSessionMetricsRecorder::RecordAppLaunch(const std::string& app_id) {
  if (unique_apps_launched_recording_enabled_ &&
      GetAppFromAppId(app_id) != DemoModeApp::kHighlights &&
      GetAppFromAppId(app_id) != DemoModeApp::kScreensaver) {
    unique_apps_launched_.insert(app_id);
  }
}

void DemoSessionMetricsRecorder::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  if (!gained_active)
    return;

  // Don't count popup windows.
  if (gained_active->type() != aura::client::WINDOW_TYPE_NORMAL)
    return;

  AppType app_type = GetAppType(gained_active);

  std::string app_id;
  if (app_type == AppType::ARC_APP) {
    const std::string* package_name = GetArcPackageName(gained_active);

    if (!package_name) {
      // The package name property for the window has not been set yet.
      // Listen for changes to the window properties so we can
      // be informed when the package name gets set.
      if (!gained_active->HasObserver(
              unique_apps_arc_package_name_observer_.get())) {
        unique_apps_arc_package_name_observer_->ObserveWindow(gained_active);
      }
      return;
    }
    app_id = *package_name;
  } else {
    // This is a non-ARC window, so we just get the shelf ID, which should
    // be unique per app.
    app_id = GetShelfID(gained_active).app_id;
  }

  // Some app_ids are empty, i.e the "You will be signed out
  // in X seconds" modal dialog in Demo Mode, so skip those.
  if (app_id.empty())
    return;

  RecordAppLaunch(app_id);
}

void DemoSessionMetricsRecorder::OnUserActivity(const ui::Event* event) {
  // Report samples recorded since the last activity.
  ReportSamples();

  // Restart the timer if the device has been idle.
  if (!timer_->IsRunning())
    StartRecording();
  periods_since_activity_ = 0;
}

void DemoSessionMetricsRecorder::StartRecording() {
  unique_apps_launched_recording_enabled_ = true;
  timer_->Start(FROM_HERE, kSamplePeriod, this,
                &DemoSessionMetricsRecorder::TakeSampleOrPause);
}

void DemoSessionMetricsRecorder::TakeSampleOrPause() {
  // After enough inactive time, assume the user left.
  if (++periods_since_activity_ > kMaxPeriodsWithoutActivity) {
    // These samples were collected since the last user activity.
    unreported_samples_.clear();
    timer_->Stop();
    return;
  }

  const aura::Window* window =
      Shell::Get()->activation_client()->GetActiveWindow();
  if (!window)
    return;

  DemoModeApp app = window->type() == aura::client::WINDOW_TYPE_NORMAL
                        ? GetAppFromWindow(window)
                        : DemoModeApp::kOtherWindow;
  unreported_samples_.push_back(app);
}

void DemoSessionMetricsRecorder::ReportSamples() {
  for (DemoModeApp app : unreported_samples_)
    UMA_HISTOGRAM_ENUMERATION("DemoMode.ActiveApp", app);
  unreported_samples_.clear();
}

void DemoSessionMetricsRecorder::ReportUniqueAppsLaunched() {
  if (unique_apps_launched_recording_enabled_)
    UMA_HISTOGRAM_COUNTS_100("DemoMode.UniqueAppsLaunched",
                             unique_apps_launched_.size());

  unique_apps_launched_.clear();
}

}  // namespace ash
