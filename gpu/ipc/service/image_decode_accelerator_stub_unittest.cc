// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_entry.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/context_result.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "ipc/ipc_message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using testing::InSequence;
using testing::StrictMock;

namespace gpu {
class MemoryTracker;

namespace {

std::unique_ptr<MemoryTracker> CreateMockMemoryTracker(
    const GPUCreateCommandBufferConfig& init_params) {
  return std::make_unique<gles2::MockMemoryTracker>();
}

scoped_refptr<Buffer> MakeBufferForTesting() {
  return MakeMemoryBuffer(sizeof(base::subtle::Atomic32));
}

}  // namespace

// This mock allows individual tests to decide asynchronously when to finish a
// decode by using the FinishOneDecode() method.
class MockImageDecodeAcceleratorWorker : public ImageDecodeAcceleratorWorker {
 public:
  MockImageDecodeAcceleratorWorker() {}

  void Decode(std::vector<uint8_t> encoded_data,
              const gfx::Size& output_size,
              CompletedDecodeCB decode_cb) {
    pending_decodes_.push(PendingDecode{output_size, std::move(decode_cb)});
    DoDecode(output_size);
  }

  void FinishOneDecode(bool success) {
    if (pending_decodes_.empty())
      return;
    PendingDecode next_decode = std::move(pending_decodes_.front());
    pending_decodes_.pop();
    if (success) {
      base::CheckedNumeric<size_t> row_bytes = 4u;
      row_bytes *= next_decode.output_size.width();
      base::CheckedNumeric<size_t> rgba_bytes = row_bytes;
      rgba_bytes *= next_decode.output_size.height();
      std::vector<uint8_t> rgba_output(rgba_bytes.ValueOrDie(), 0u);
      std::move(next_decode.decode_cb)
          .Run(std::move(rgba_output), row_bytes.ValueOrDie(),
               SkImageInfo::Make(next_decode.output_size.width(),
                                 next_decode.output_size.height(),
                                 kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
    } else {
      std::move(next_decode.decode_cb)
          .Run(std::vector<uint8_t>(), 0u, SkImageInfo());
    }
  }

  MOCK_METHOD1(DoDecode, void(const gfx::Size&));

 private:
  struct PendingDecode {
    gfx::Size output_size;
    CompletedDecodeCB decode_cb;
  };

  base::queue<PendingDecode> pending_decodes_;

  DISALLOW_COPY_AND_ASSIGN(MockImageDecodeAcceleratorWorker);
};

const int kChannelId = 1;

const int32_t kCommandBufferRouteId =
    static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;

// Test fixture: the general strategy for testing is to have a GPU channel test
// infrastructure (provided by GpuChannelTestCommon), ask the channel to handle
// decode requests, and expect sync token releases, invocations to the
// ImageDecodeAcceleratorWorker functionality, and transfer cache entry
// creation.
class ImageDecodeAcceleratorStubTest : public GpuChannelTestCommon {
 public:
  ImageDecodeAcceleratorStubTest()
      : GpuChannelTestCommon(false /* use_stub_bindings */) {}
  ~ImageDecodeAcceleratorStubTest() override = default;

  SyncPointManager* sync_point_manager() const {
    return channel_manager()->sync_point_manager();
  }

  ServiceTransferCache* GetServiceTransferCache() {
    ContextResult context_result;
    scoped_refptr<SharedContextState> shared_context_state =
        channel_manager()->GetSharedContextState(&context_result);
    if (context_result != ContextResult::kSuccess || !shared_context_state) {
      return nullptr;
    }
    return shared_context_state->transfer_cache();
  }

  int GetRasterDecoderId() {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    if (!channel)
      return -1;
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    if (!command_buffer || !command_buffer->decoder_context())
      return -1;
    return command_buffer->decoder_context()->GetRasterDecoderId();
  }

  void SetUp() override {
    GpuChannelTestCommon::SetUp();
    // TODO(andrescj): get rid of the |feature_list_| when the feature is
    // enabled by default.
    feature_list_.InitAndEnableFeature(
        features::kVaapiJpegImageDecodeAcceleration);
    channel_manager()->SetImageDecodeAcceleratorWorkerForTesting(
        &image_decode_accelerator_worker_);

    // Initialize the GrContext so that texture uploading works.
    ContextResult context_result;
    scoped_refptr<SharedContextState> shared_context_state =
        channel_manager()->GetSharedContextState(&context_result);
    ASSERT_EQ(ContextResult::kSuccess, context_result);
    ASSERT_TRUE(shared_context_state);
    shared_context_state->InitializeGrContext(GpuDriverBugWorkarounds(),
                                              nullptr);

    GpuChannel* channel = CreateChannel(kChannelId, false /* is_gpu_host */);
    ASSERT_TRUE(channel);

    // Create a raster command buffer so that the ImageDecodeAcceleratorStub can
    // have access to a TransferBufferManager. Note that we mock the
    // MemoryTracker because GpuCommandBufferMemoryTracker uses a timer that
    // would make RunTasksUntilIdle() run forever.
    CommandBufferStub::SetMemoryTrackerFactoryForTesting(
        base::BindRepeating(&CreateMockMemoryTracker));
    GPUCreateCommandBufferConfig init_params;
    init_params.surface_handle = kNullSurfaceHandle;
    init_params.share_group_id = MSG_ROUTING_NONE;
    init_params.stream_id = 0;
    init_params.stream_priority = SchedulingPriority::kNormal;
    init_params.attribs = ContextCreationAttribs();
    init_params.attribs.enable_gles2_interface = false;
    init_params.attribs.enable_raster_interface = true;
    init_params.attribs.bind_generates_resource = false;
    init_params.active_url = GURL();
    ContextResult result = ContextResult::kTransientFailure;
    Capabilities capabilities;
    HandleMessage(channel,
                  new GpuChannelMsg_CreateCommandBuffer(
                      init_params, kCommandBufferRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities));
    ASSERT_EQ(ContextResult::kSuccess, result);
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    ASSERT_TRUE(command_buffer);

    // Make sure there are no pending tasks before starting the test.
    ASSERT_EQ(0u, task_runner()->NumPendingTasks());
    ASSERT_EQ(0u, io_task_runner()->NumPendingTasks());
  }

  void TearDown() override {
    // Make sure the channel is destroyed before the
    // |image_decode_accelerator_worker_| is destroyed.
    channel_manager()->DestroyAllChannels();
  }

  SyncToken SendDecodeRequest(const gfx::Size& output_size,
                              uint64_t release_count,
                              uint32_t transfer_cache_entry_id) {
    GpuChannel* channel = channel_manager()->LookupChannel(kChannelId);
    if (!channel) {
      // It's possible that the channel was destroyed as part of an earlier
      // SendDecodeRequest() call. This would happen if
      // ImageDecodeAcceleratorStub::OnScheduleImageDecode decides to destroy
      // the channel.
      return SyncToken();
    }

    // Create the decode sync token for the decode request so that we can test
    // that it's actually released.
    SyncToken decode_sync_token(
        CommandBufferNamespace::GPU_IO,
        CommandBufferIdFromChannelAndRoute(
            kChannelId, static_cast<int32_t>(
                            GpuChannelReservedRoutes::kImageDecodeAccelerator)),
        release_count);

    // We need a buffer to make sure that the ImageDecodeAcceleratorStub can
    // create a ServiceDiscardableHandle.
    scoped_refptr<Buffer> handle_buffer = MakeBufferForTesting();
    CommandBufferStub* command_buffer =
        channel->LookupCommandBuffer(kCommandBufferRouteId);
    if (!command_buffer)
      return SyncToken();
    scoped_refptr<gles2::ContextGroup> context_group =
        command_buffer->context_group();
    if (!context_group)
      return SyncToken();
    TransferBufferManager* transfer_buffer_manager =
        context_group->transfer_buffer_manager();
    if (!transfer_buffer_manager)
      return SyncToken();
    int32_t buffer_shm_id = GetNextBufferId();
    transfer_buffer_manager->RegisterTransferBuffer(buffer_shm_id,
                                                    std::move(handle_buffer));

    // Send the IPC decode request.
    GpuChannelMsg_ScheduleImageDecode_Params decode_params;
    decode_params.encoded_data = std::vector<uint8_t>();
    decode_params.output_size = output_size;
    decode_params.raster_decoder_route_id = kCommandBufferRouteId;
    decode_params.transfer_cache_entry_id = transfer_cache_entry_id;
    decode_params.discardable_handle_shm_id = buffer_shm_id;
    decode_params.discardable_handle_shm_offset = 0u;
    decode_params.target_color_space = gfx::ColorSpace();
    decode_params.needs_mips = false;

    HandleMessage(
        channel,
        new GpuChannelMsg_ScheduleImageDecode(
            static_cast<int32_t>(
                GpuChannelReservedRoutes::kImageDecodeAccelerator),
            std::move(decode_params), decode_sync_token.release_count()));
    return decode_sync_token;
  }

  void RunTasksUntilIdle() {
    while (task_runner()->HasPendingTask() ||
           io_task_runner()->HasPendingTask()) {
      task_runner()->RunUntilIdle();
      io_task_runner()->RunUntilIdle();
    }
  }

  void CheckTransferCacheEntries(std::vector<SkISize> expected_sizes) {
    ServiceTransferCache* transfer_cache = GetServiceTransferCache();
    ASSERT_TRUE(transfer_cache);

    // First, check the number of entries and early out if 0 entries are
    // expected.
    const size_t num_actual_cache_entries =
        transfer_cache->entries_count_for_testing();
    ASSERT_EQ(expected_sizes.size(), num_actual_cache_entries);
    if (expected_sizes.empty())
      return;

    // Then, check the dimensions of the entries to make sure they are as
    // expected.
    int raster_decoder_id = GetRasterDecoderId();
    ASSERT_GE(raster_decoder_id, 0);
    for (size_t i = 0; i < num_actual_cache_entries; i++) {
      auto* decode_entry = static_cast<cc::ServiceImageTransferCacheEntry*>(
          transfer_cache->GetEntry(ServiceTransferCache::EntryKey(
              raster_decoder_id, cc::TransferCacheEntryType::kImage, i + 1)));
      ASSERT_TRUE(decode_entry);
      ASSERT_TRUE(decode_entry->image());
      EXPECT_EQ(expected_sizes[i].width(),
                decode_entry->image()->dimensions().width());
      EXPECT_EQ(expected_sizes[i].height(),
                decode_entry->image()->dimensions().height());
    }
  }

 protected:
  StrictMock<MockImageDecodeAcceleratorWorker> image_decode_accelerator_worker_;

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeAcceleratorStubTest);
};

// Tests the following flow: two decode requests are sent. One of the decodes is
// completed. This should cause one sync token to be released and the scheduler
// sequence to be disabled. Then, the second decode is completed. This should
// cause the other sync token to be released.
TEST_F(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedAfterSequenceIsDisabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */,
      1u /* transfer_cache_entry_id */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */,
      2u /* transfer_cache_entry_id */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // Only the first decode sync token should be released after the first decode
  // is finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // The second decode sync token should be released after the second decode is
  // finished.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // The channel should still exist at the end.
  EXPECT_TRUE(channel_manager()->LookupChannel(kChannelId));

  // Check that the decoded images are in the transfer cache.
  CheckTransferCacheEntries({SkISize::Make(100, 100), SkISize::Make(200, 200)});
}

// Tests the following flow: three decode requests are sent. The first decode
// completes which should cause the scheduler sequence to be enabled. Right
// after that (while the sequence is still enabled), the other two decodes
// complete. At the end, all the sync tokens should be released.
TEST_F(ImageDecodeAcceleratorStubTest,
       MultipleDecodesCompletedWhileSequenceIsEnabled) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */,
      1u /* transfer_cache_entry_id */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */,
      2u /* transfer_cache_entry_id */);
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* release_count */,
      3u /* transfer_cache_entry_id */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // All decode sync tokens should be released after completing all the decodes.
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  RunTasksUntilIdle();
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // The channel should still exist at the end.
  EXPECT_TRUE(channel_manager()->LookupChannel(kChannelId));

  // Check that the decoded images are in the transfer cache.
  CheckTransferCacheEntries({SkISize::Make(100, 100), SkISize::Make(200, 200),
                             SkISize::Make(300, 300)});
}

// Tests the following flow: three decode requests are sent. The first decode
// fails which should trigger the destruction of the channel. The second
// succeeds and the third one fails. Regardless, the channel should still be
// destroyed and all sync tokens should be released.
TEST_F(ImageDecodeAcceleratorStubTest, FailedDecodes) {
  {
    InSequence call_sequence;
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(200, 200)))
        .Times(1);
    EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(300, 300)))
        .Times(1);
  }
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 1u /* release_count */,
      1u /* transfer_cache_entry_id */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 2u /* release_count */,
      2u /* transfer_cache_entry_id */);
  const SyncToken decode3_sync_token = SendDecodeRequest(
      gfx::Size(300, 300) /* output_size */, 3u /* release_count */,
      3u /* transfer_cache_entry_id */);

  // A decode sync token should not be released before a decode is finished.
  RunTasksUntilIdle();
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_FALSE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));
  image_decode_accelerator_worker_.FinishOneDecode(false);
  image_decode_accelerator_worker_.FinishOneDecode(true);
  image_decode_accelerator_worker_.FinishOneDecode(false);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode3_sync_token));

  // We expect no entries in the transfer cache.
  CheckTransferCacheEntries({});
}

TEST_F(ImageDecodeAcceleratorStubTest, OutOfOrderSyncTokens) {
  EXPECT_CALL(image_decode_accelerator_worker_, DoDecode(gfx::Size(100, 100)))
      .Times(1);
  const SyncToken decode1_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 2u /* release_count */,
      1u /* transfer_cache_entry_id */);
  const SyncToken decode2_sync_token = SendDecodeRequest(
      gfx::Size(200, 200) /* output_size */, 1u /* release_count */,
      2u /* transfer_cache_entry_id */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode1_sync_token));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode2_sync_token));

  // We expect no entries in the transfer cache.
  CheckTransferCacheEntries({});
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroReleaseCountSyncToken) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 100) /* output_size */, 0u /* release_count */,
      1u /* transfer_cache_entry_id */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // We expect no entries in the transfer cache.
  CheckTransferCacheEntries({});
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroWidthOutputSize) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(0, 100) /* output_size */, 1u /* release_count */,
      1u /* transfer_cache_entry_id */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // We expect no entries in the transfer cache.
  CheckTransferCacheEntries({});
}

TEST_F(ImageDecodeAcceleratorStubTest, ZeroHeightOutputSize) {
  const SyncToken decode_sync_token = SendDecodeRequest(
      gfx::Size(100, 0) /* output_size */, 1u /* release_count */,
      1u /* transfer_cache_entry_id */);

  // We expect the destruction of the ImageDecodeAcceleratorStub, which also
  // implies that all decode sync tokens should be released.
  RunTasksUntilIdle();
  EXPECT_FALSE(channel_manager()->LookupChannel(kChannelId));
  EXPECT_TRUE(sync_point_manager()->IsSyncTokenReleased(decode_sync_token));

  // We expect no entries in the transfer cache.
  CheckTransferCacheEntries({});
}

}  // namespace gpu
