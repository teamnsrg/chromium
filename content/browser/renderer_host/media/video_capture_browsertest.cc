// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::AtLeast;
using testing::Bool;
using testing::Combine;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Values;

namespace content {

static const char kFakeDeviceFactoryConfigString[] = "device-count=3";
static const float kFrameRateToRequest = 15.0f;

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  MOCK_METHOD3(DoOnNewBuffer,
               void(VideoCaptureControllerID id,
                    media::mojom::VideoBufferHandlePtr* buffer_handle,
                    int buffer_id));
  MOCK_METHOD2(OnBufferDestroyed,
               void(VideoCaptureControllerID, int buffer_id));
  MOCK_METHOD3(OnBufferReady,
               void(VideoCaptureControllerID id,
                    int buffer_id,
                    const media::mojom::VideoFrameInfoPtr& frame_info));
  MOCK_METHOD1(OnStarted, void(VideoCaptureControllerID));
  MOCK_METHOD1(OnEnded, void(VideoCaptureControllerID));
  MOCK_METHOD2(OnError,
               void(VideoCaptureControllerID, media::VideoCaptureError));
  MOCK_METHOD1(OnStartedUsingGpuDecode, void(VideoCaptureControllerID));
  MOCK_METHOD1(OnStoppedUsingGpuDecode, void(VideoCaptureControllerID));

  void OnNewBuffer(VideoCaptureControllerID id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   int buffer_id) override {
    DoOnNewBuffer(id, &buffer_handle, buffer_id);
  }
};

class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MOCK_METHOD2(Opened, void(blink::MediaStreamType, int));
  MOCK_METHOD2(Closed, void(blink::MediaStreamType, int));
  MOCK_METHOD2(Aborted, void(blink::MediaStreamType, int));
};

using DeviceIndex = size_t;
using Resolution = gfx::Size;
using ExerciseAcceleratedJpegDecoding = bool;
using UseMojoService = bool;

// For converting the std::tuple<> used as test parameters back to something
// human-readable.
struct TestParams {
  TestParams() : device_index_to_use(0u) {}
  TestParams(const std::tuple<DeviceIndex,
                              Resolution,
                              ExerciseAcceleratedJpegDecoding,
                              UseMojoService>& params)
      : device_index_to_use(std::get<0>(params)),
        resolution_to_use(std::get<1>(params)),
        exercise_accelerated_jpeg_decoding(std::get<2>(params)),
        use_mojo_service(std::get<3>(params)) {}

  media::VideoPixelFormat GetPixelFormatToUse() {
    return (device_index_to_use == 1u) ? media::PIXEL_FORMAT_Y16
                                       : media::PIXEL_FORMAT_I420;
  }

  size_t device_index_to_use;
  gfx::Size resolution_to_use;
  bool exercise_accelerated_jpeg_decoding;
  bool use_mojo_service;
};

struct FrameInfo {
  gfx::Size size;
  media::VideoPixelFormat pixel_format;
  base::TimeDelta timestamp;
};

// Integration test that exercises the VideoCaptureManager instance running in
// the Browser process.
class VideoCaptureBrowserTest : public ContentBrowserTest,
                                public ::testing::WithParamInterface<
                                    std::tuple<DeviceIndex,
                                               Resolution,
                                               ExerciseAcceleratedJpegDecoding,
                                               UseMojoService>> {
 public:
  VideoCaptureBrowserTest() {
    params_ = TestParams(GetParam());
    if (params_.use_mojo_service) {
      scoped_feature_list_.InitAndEnableFeature(features::kMojoVideoCapture);
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kMojoVideoCapture);
    }
  }

  ~VideoCaptureBrowserTest() override {}

  void SetUpAndStartCaptureDeviceOnIOThread(base::Closure continuation) {
    video_capture_manager_ = media_stream_manager_->video_capture_manager();
    ASSERT_TRUE(video_capture_manager_);
    video_capture_manager_->RegisterListener(&mock_stream_provider_listener_);
    video_capture_manager_->EnumerateDevices(
        base::BindOnce(&VideoCaptureBrowserTest::OnDeviceDescriptorsReceived,
                       base::Unretained(this), std::move(continuation)));
  }

  void TearDownCaptureDeviceOnIOThread(base::Closure continuation,
                                       bool post_to_end_of_message_queue) {
    // DisconnectClient() must not be called synchronously from either the
    // |done_cb| passed to StartCaptureForClient() nor any callback made to a
    // VideoCaptureControllerEventHandler. To satisfy this, we have to post our
    // invocation to the end of the IO message queue.
    if (post_to_end_of_message_queue) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
              base::Unretained(this), continuation, false));
      return;
    }

    video_capture_manager_->DisconnectClient(controller_.get(), stub_client_id_,
                                             &mock_controller_event_handler_,
                                             media::VideoCaptureError::kNone);

    EXPECT_CALL(mock_stream_provider_listener_, Closed(_, _))
        .WillOnce(InvokeWithoutArgs([continuation]() { continuation.Run(); }));

    video_capture_manager_->Close(session_id_);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kUseFakeDeviceForMediaStream,
                                    kFakeDeviceFactoryConfigString);
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
    if (params_.exercise_accelerated_jpeg_decoding) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kUseFakeJpegDecodeAccelerator);
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kDisableAcceleratedMjpegDecode);
    }
  }

  // This cannot be part of an override of SetUp(), because at the time when
  // SetUp() is invoked, the BrowserMainLoop does not exist yet.
  void SetUpRequiringBrowserMainLoopOnMainThread() {
    BrowserMainLoop* browser_main_loop = BrowserMainLoop::GetInstance();
    ASSERT_TRUE(browser_main_loop);
    media_stream_manager_ = browser_main_loop->media_stream_manager();
    ASSERT_TRUE(media_stream_manager_);
  }

  void OnDeviceDescriptorsReceived(
      base::Closure continuation,
      const media::VideoCaptureDeviceDescriptors& descriptors) {
    ASSERT_TRUE(params_.device_index_to_use < descriptors.size());
    const auto& descriptor = descriptors[params_.device_index_to_use];
    blink::MediaStreamDevice media_stream_device(
        blink::MEDIA_DEVICE_VIDEO_CAPTURE, descriptor.device_id,
        descriptor.display_name(), descriptor.facing);
    session_id_ = video_capture_manager_->Open(media_stream_device);
    media::VideoCaptureParams capture_params;
    capture_params.requested_format = media::VideoCaptureFormat(
        params_.resolution_to_use, kFrameRateToRequest,
        params_.GetPixelFormatToUse());
    video_capture_manager_->ConnectClient(
        session_id_, capture_params, stub_client_id_,
        &mock_controller_event_handler_,
        base::Bind(&VideoCaptureBrowserTest::OnConnectClientToControllerAnswer,
                   base::Unretained(this), std::move(continuation)));
  }

  void OnConnectClientToControllerAnswer(
      base::Closure continuation,
      const base::WeakPtr<VideoCaptureController>& controller) {
    ASSERT_TRUE(controller.get());
    controller_ = controller;
    if (!continuation)
      return;
    std::move(continuation).Run();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  TestParams params_;
  MediaStreamManager* media_stream_manager_ = nullptr;
  VideoCaptureManager* video_capture_manager_ = nullptr;
  int session_id_ = 0;
  const VideoCaptureControllerID stub_client_id_ = 123;
  MockMediaStreamProviderListener mock_stream_provider_listener_;
  MockVideoCaptureControllerEventHandler mock_controller_event_handler_;
  base::WeakPtr<VideoCaptureController> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureBrowserTest);
};

IN_PROC_BROWSER_TEST_P(VideoCaptureBrowserTest, StartAndImmediatelyStop) {
  SetUpRequiringBrowserMainLoopOnMainThread();
  base::RunLoop run_loop;
  auto quit_run_loop_on_current_thread_cb =
      media::BindToCurrentLoop(run_loop.QuitClosure());
  auto after_start_continuation =
      base::Bind(&VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
                 base::Unretained(this),
                 std::move(quit_run_loop_on_current_thread_cb), true);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &VideoCaptureBrowserTest::SetUpAndStartCaptureDeviceOnIOThread,
          base::Unretained(this), std::move(after_start_continuation)));
  run_loop.Run();
}

// Flaky on MSAN. https://crbug.com/840294
#if defined(MEMORY_SANITIZER)
#define MAYBE_ReceiveFramesFromFakeCaptureDevice \
  DISABLED_ReceiveFramesFromFakeCaptureDevice
#else
#define MAYBE_ReceiveFramesFromFakeCaptureDevice \
  ReceiveFramesFromFakeCaptureDevice
#endif
IN_PROC_BROWSER_TEST_P(VideoCaptureBrowserTest,
                       MAYBE_ReceiveFramesFromFakeCaptureDevice) {
  // Only fake device with index 2 delivers MJPEG.
  if (params_.exercise_accelerated_jpeg_decoding &&
      params_.device_index_to_use != 2) {
    return;
  }

  SetUpRequiringBrowserMainLoopOnMainThread();

  std::vector<FrameInfo> received_frame_infos;
  static const size_t kMinFramesToReceive = 5;
  static const size_t kMaxFramesToReceive = 300;
  base::RunLoop run_loop;

  auto quit_run_loop_on_current_thread_cb =
      media::BindToCurrentLoop(run_loop.QuitClosure());
  auto finish_test_cb =
      base::Bind(&VideoCaptureBrowserTest::TearDownCaptureDeviceOnIOThread,
                 base::Unretained(this),
                 std::move(quit_run_loop_on_current_thread_cb), true);

  bool must_wait_for_gpu_decode_to_start = false;
  if (params_.exercise_accelerated_jpeg_decoding) {
    // Since the GPU jpeg decoder is created asynchronously while decoding
    // in software is ongoing, we have to keep pushing frames until a message
    // arrives that tells us that the GPU decoder is being used. Otherwise,
    // it may happen that all test frames are decoded using the non-GPU
    // decoding path before the GPU decoder has started getting used.
    must_wait_for_gpu_decode_to_start = true;
    EXPECT_CALL(mock_controller_event_handler_, OnStartedUsingGpuDecode(_))
        .WillOnce(InvokeWithoutArgs([&must_wait_for_gpu_decode_to_start]() {
          must_wait_for_gpu_decode_to_start = false;
        }));
  }
  EXPECT_CALL(mock_controller_event_handler_, DoOnNewBuffer(_, _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(mock_controller_event_handler_, OnBufferReady(_, _, _))
      .WillRepeatedly(Invoke(
          [this, &received_frame_infos, &must_wait_for_gpu_decode_to_start,
           &finish_test_cb](VideoCaptureControllerID id, int buffer_id,
                            const media::mojom::VideoFrameInfoPtr& frame_info) {
            FrameInfo received_frame_info;
            received_frame_info.pixel_format = frame_info->pixel_format;
            received_frame_info.size = frame_info->coded_size;
            received_frame_info.timestamp = frame_info->timestamp;
            received_frame_infos.emplace_back(received_frame_info);

            const double kArbitraryUtilization = 0.5;
            controller_->ReturnBuffer(id, &mock_controller_event_handler_,
                                      buffer_id, kArbitraryUtilization);

            if ((received_frame_infos.size() >= kMinFramesToReceive &&
                 !must_wait_for_gpu_decode_to_start) ||
                (received_frame_infos.size() == kMaxFramesToReceive)) {
              finish_test_cb.Run();
            }
          }));

  base::Closure do_nothing;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &VideoCaptureBrowserTest::SetUpAndStartCaptureDeviceOnIOThread,
          base::Unretained(this), std::move(do_nothing)));
  run_loop.Run();

  EXPECT_FALSE(must_wait_for_gpu_decode_to_start);
  EXPECT_GE(received_frame_infos.size(), kMinFramesToReceive);
  EXPECT_LT(received_frame_infos.size(), kMaxFramesToReceive);
  base::TimeDelta previous_timestamp;
  bool first_frame = true;
  for (const auto& frame_info : received_frame_infos) {
    EXPECT_EQ(params_.GetPixelFormatToUse(), frame_info.pixel_format);
    EXPECT_EQ(params_.resolution_to_use, frame_info.size);
    // Timestamps are expected to increase
    if (!first_frame)
      EXPECT_GT(frame_info.timestamp, previous_timestamp);
    first_frame = false;
    previous_timestamp = frame_info.timestamp;
  }
}

INSTANTIATE_TEST_CASE_P(,
                        VideoCaptureBrowserTest,
                        Combine(Values(0, 1, 2),             // DeviceIndex
                                Values(gfx::Size(640, 480),  // Resolution
                                       gfx::Size(1280, 720)),
                                Bool(),    // ExerciseAcceleratedJpegDecoding
                                Bool()));  // UseMojoService

}  // namespace content
