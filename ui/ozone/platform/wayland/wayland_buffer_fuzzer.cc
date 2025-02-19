// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This fuzzer tests browser-side implementation of
// ozone::mojom::WaylandConnection.

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/test/fuzzed_data_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

using testing::_;

namespace {

// Off-class equivalent of WaylandTest::Sync.
void Sync(wl::TestWaylandServerThread* server) {
  DCHECK(server);

  server->Resume();
  base::RunLoop().RunUntilIdle();
  server->Pause();
}

// Copied from ui/ozone/test/mock_platform_window_delegate.h to avoid
// dependency from the whole library (it causes link problems).
class MockPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate() = default;
  ~MockPlatformWindowDelegate() = default;

  MOCK_METHOD1(OnBoundsChanged, void(const gfx::Rect& new_bounds));
  MOCK_METHOD1(OnDamageRect, void(const gfx::Rect& damaged_region));
  MOCK_METHOD1(DispatchEvent, void(ui::Event* event));
  MOCK_METHOD0(OnCloseRequest, void());
  MOCK_METHOD0(OnClosed, void());
  MOCK_METHOD1(OnWindowStateChanged, void(ui::PlatformWindowState new_state));
  MOCK_METHOD0(OnLostCapture, void());
  MOCK_METHOD1(OnAcceleratedWidgetAvailable,
               void(gfx::AcceleratedWidget widget));
  MOCK_METHOD0(OnAcceleratedWidgetDestroyed, void());
  MOCK_METHOD1(OnActivationChanged, void(bool active));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPlatformWindowDelegate);
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  std::vector<uint32_t> known_fourccs{
      DRM_FORMAT_R8,          DRM_FORMAT_GR88,        DRM_FORMAT_ABGR8888,
      DRM_FORMAT_XBGR8888,    DRM_FORMAT_ARGB8888,    DRM_FORMAT_XRGB8888,
      DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_RGB565,
      DRM_FORMAT_UYVY,        DRM_FORMAT_NV12,        DRM_FORMAT_YVU420};

  base::MessageLoopForUI message_loop;

  MockPlatformWindowDelegate delegate;
  std::unique_ptr<ui::WaylandConnection> connection =
      std::make_unique<ui::WaylandConnection>();
  std::unique_ptr<ui::WaylandWindow> window =
      std::make_unique<ui::WaylandWindow>(&delegate, connection.get());
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

  wl::TestWaylandServerThread server;
  CHECK(server.Start(6));
  CHECK(connection->Initialize());

  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(testing::SaveArg<0>(&widget));
  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 800, 600);
  properties.type = ui::PlatformWindowType::kWindow;
  CHECK(window->Initialize(std::move(properties)));
  CHECK_NE(widget, gfx::kNullAcceleratedWidget);

  base::RunLoop().RunUntilIdle();
  server.Pause();

  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  base::File temp(temp_path,
                  base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);

  // 10K screens are reality these days.
  const uint32_t kWidth = data_provider.ConsumeIntegralInRange(1U, 20000U);
  const uint32_t kHeight = data_provider.ConsumeIntegralInRange(1U, 20000U);
  // The buffer manager opens a file descriptor for each plane so |plane_count|
  // cannot be really large.  Technically, the maximum is |ulimit| minus number
  // of file descriptors opened by this process already (which is 17 at the time
  // of writing) but there is little sense in having more than a few planes in a
  // real system so here is a hard limit of 500.
  const uint32_t kPlaneCount = data_provider.ConsumeIntegralInRange(1U, 500U);
  const uint32_t kFormat = known_fourccs[data_provider.ConsumeIntegralInRange(
      0UL, known_fourccs.size() - 1)];

  std::vector<uint32_t> strides(kPlaneCount);
  std::vector<uint32_t> offsets(kPlaneCount);
  std::vector<uint64_t> modifiers(kPlaneCount);
  for (uint32_t i = 0; i < kPlaneCount; ++i) {
    strides[i] = data_provider.ConsumeIntegralInRange(1U, UINT_MAX);
    offsets[i] = data_provider.ConsumeIntegralInRange(0U, UINT_MAX);
    modifiers[i] =
        data_provider.ConsumeIntegralInRange(uint64_t(0), UINT64_MAX);
  }

  const uint32_t kBufferId = 1;

  EXPECT_CALL(*server.zwp_linux_dmabuf_v1(), CreateParams(_, _, _));

  connection->CreateZwpLinuxDmabuf(std::move(temp), kWidth, kHeight, strides,
                                   offsets, kFormat, modifiers, kPlaneCount,
                                   kBufferId);

  Sync(&server);
  Sync(&server);

  connection->DestroyZwpLinuxDmabuf(kBufferId);

  Sync(&server);

  return 0;
}
