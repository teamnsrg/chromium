// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/direct_context_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/private/SkDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace viz {
namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

// FakeOnScreenSurface is used to force GLContextVirtual::MakeCurrent to set
// |surface| as the default framebuffer.
class FakeOnScreenSurface : public gl::GLSurfaceAdapter {
 public:
  explicit FakeOnScreenSurface(gl::GLSurface* surface)
      : gl::GLSurfaceAdapter(surface) {}

  bool IsOffscreen() override { return false; }

 private:
  ~FakeOnScreenSurface() override = default;
};

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    GpuServiceImpl* gpu_service) {
  auto* channel_manager = gpu_service->gpu_channel_manager();
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info());
}

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    gpu::CommandBufferTaskExecutor* task_executor) {
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      gpu::GpuDriverBugWorkarounds(
          task_executor->gpu_feature_info().enabled_gpu_driver_bug_workarounds),
      task_executor->gpu_feature_info());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    GpuServiceImpl* gpu_service) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return gpu_service->sync_point_manager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      gpu_service->skia_output_surface_sequence_id());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::CommandBufferTaskExecutor* task_executor,
    gpu::SequenceId sequence_id) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return task_executor->sync_point_manager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      sequence_id);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(GpuServiceImpl* gpu_service) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      gpu_service->shared_image_manager(), nullptr);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(
    gpu::CommandBufferTaskExecutor* task_executor) {
  return nullptr;
}

}  // namespace

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::~OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    const OffscreenSurface& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    const OffscreenSurface& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider::ScopedUseContextProvider(
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
    : impl_on_gpu_(impl_on_gpu) {
  if (!impl_on_gpu_->MakeCurrent()) {
    valid_ = false;
    return;
  }

  // GLRendererCopier uses context_provider_->ContextGL(), which caches GL state
  // and removes state setting calls that it considers redundant. To get to a
  // safe known GL state, we first call the client side to set the cached state,
  // then we make driver GL state consistent with that.
  impl_on_gpu_->context_provider_->SetGLRendererCopierRequiredState();
  auto* api = impl_on_gpu_->api_;
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
  api->glDisableFn(GL_SCISSOR_TEST);
  api->glDisableFn(GL_STENCIL_TEST);
  api->glDisableFn(GL_BLEND);
}

SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider::
    ~ScopedUseContextProvider() {
  if (valid_)
    impl_on_gpu_->gr_context()->resetContext();
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    gpu::SurfaceHandle surface_handle,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MailboxManager* mailbox_manager,
    scoped_refptr<gpu::SyncPointClientState> sync_point_client_state,
    std::unique_ptr<gpu::SharedImageRepresentationFactory> sir_factory,
    gpu::raster::GrShaderCache* gr_shader_cache,
    VulkanContextProvider* vulkan_context_provider,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : surface_handle_(surface_handle),
      feature_info_(std::move(feature_info)),
      mailbox_manager_(mailbox_manager),
      sync_point_client_state_(std::move(sync_point_client_state)),
      shared_image_representation_factory_(std::move(sir_factory)),
      gr_shader_cache_(gr_shader_cache),
      vulkan_context_provider_(vulkan_context_provider),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback),
      buffer_presented_callback_(buffer_presented_callback),
      context_lost_callback_(context_lost_callback),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    GpuServiceImpl* gpu_service,
    gpu::SurfaceHandle surface_handle,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : SkiaOutputSurfaceImplOnGpu(
          surface_handle,
          CreateFeatureInfo(gpu_service),
          gpu_service->mailbox_manager(),
          CreateSyncPointClientState(gpu_service),
          CreateSharedImageRepresentationFactory(gpu_service),
          gpu_service->gr_shader_cache(),
          gpu_service->vulkan_context_provider(),
          did_swap_buffer_complete_callback,
          buffer_presented_callback,
          context_lost_callback) {
#if defined(USE_OZONE)
  window_surface_ = ui::OzonePlatform::GetInstance()
                        ->GetSurfaceFactoryOzone()
                        ->CreatePlatformWindowSurface(surface_handle);
#endif

  if (is_using_vulkan())
    InitializeForVulkan(gpu_service);
  else
    InitializeForGLWithGpuService(gpu_service);
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    gpu::CommandBufferTaskExecutor* task_executor,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::SequenceId sequence_id,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : SkiaOutputSurfaceImplOnGpu(
          gpu::kNullSurfaceHandle,
          CreateFeatureInfo(task_executor),
          task_executor->mailbox_manager(),
          CreateSyncPointClientState(task_executor, sequence_id),
          CreateSharedImageRepresentationFactory(task_executor),
          nullptr /* gr_shader_cache */,
          nullptr /* vulkan_context_provider */,
          did_swap_buffer_complete_callback,
          buffer_presented_callback,
          context_lost_callback) {
  DCHECK(!is_using_vulkan());
  gl_surface_ = std::move(gl_surface);
  context_state_ = std::move(shared_context_state);
  InitializeForGL();
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // ~DirectContextProvider wants either the context to be lost or made current.
  MakeCurrent();

#if BUILDFLAG(ENABLE_VULKAN)
  if (vulkan_surface_) {
    vulkan_surface_->Destroy();
    vulkan_surface_ = nullptr;
  }
#endif
  sync_point_client_state_->Destroy();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::ScopedClosureRunner scoped_runner;
  if (event) {
    scoped_runner.ReplaceClosure(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!is_using_vulkan()) {
    if (!MakeCurrent())
      return;
    size_ = size;
    color_space_ = color_space;
    // Conversion to GLSurface's color space follows the same logic as in
    // gl::GetGLColorSpace().
    gl::GLSurface::ColorSpace surface_color_space =
        color_space.IsHDR() ? gl::GLSurface::ColorSpace::SCRGB_LINEAR
                            : gl::GLSurface::ColorSpace::UNSPECIFIED;
    if (!gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                             has_alpha)) {
      LOG(FATAL) << "Failed to resize.";
      // TODO(penghuang): Handle the failure.
    }
    DCHECK(gr_context());
    CreateSkSurfaceForGL();
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    gfx::AcceleratedWidget accelerated_widget = gfx::kNullAcceleratedWidget;
#if defined(OS_ANDROID)
    accelerated_widget =
        gpu::GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
            surface_handle_);
#else
    accelerated_widget = surface_handle_;
#endif
    if (!vulkan_surface_) {
      auto vulkan_surface = vulkan_context_provider_->GetVulkanImplementation()
                                ->CreateViewSurface(accelerated_widget);
      if (!vulkan_surface)
        LOG(FATAL) << "Failed to create vulkan surface.";
      if (!vulkan_surface->Initialize(
              vulkan_context_provider_->GetDeviceQueue(),
              gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
        LOG(FATAL) << "Failed to initialize vulkan surface.";
      }
      vulkan_surface_ = std::move(vulkan_surface);
    }
    auto old_size = vulkan_surface_->size();
    vulkan_surface_->SetSize(size);
    if (vulkan_surface_->size() != old_size) {
      // Size has been changed, we need to clear all surfaces which will be
      // recreated later.
      sk_surfaces_.clear();
      sk_surfaces_.resize(vulkan_surface_->GetSwapChain()->num_images());
    }
    CreateSkSurfaceForVulkan();
#else
    NOTREACHED();
#endif
  }

  if (characterization) {
    sk_surface_->characterize(characterization);
    DCHECK(characterization->isValid());
  }
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);
  DCHECK(sk_surface_);

  if (!MakeCurrent())
    return;

  PullTextureUpdates(std::move(sync_tokens));

  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }
    sk_surface_->draw(ddl.get());
    gr_context()->flush();
  }

  // Note that the ScopedCacheUse for GrShaderCache is scoped until the
  // ReleaseFenceSync call here since releasing the fence may schedule a
  // different decoder's stream which also uses the shader cache.
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);

  if (overdraw_ddl) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }

    sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
        gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
    overdraw_surface->draw(overdraw_ddl.get());

    SkPaint paint;
    sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

    sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
    paint.setColorFilter(colorFilter);
    // TODO(xing.xu): move below to the thread where skia record happens.
    sk_surface_->getCanvas()->drawImage(overdraw_image.get(), 0, 0, &paint);
    gr_context()->flush();
  }
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sk_surface_);
  base::TimeTicks swap_start, swap_end;
  if (!is_using_vulkan()) {
    if (!MakeCurrent())
      return;
    swap_start = base::TimeTicks::Now();
    OnSwapBuffers();
    gl_surface_->SwapBuffers(buffer_presented_callback_);
    swap_end = base::TimeTicks::Now();
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    swap_start = base::TimeTicks::Now();
    OnSwapBuffers();
    auto backend = sk_surface_->getBackendRenderTarget(
        SkSurface::kFlushRead_BackendHandleAccess);
    GrVkImageInfo vk_image_info;
    if (!backend.getVkImageInfo(&vk_image_info))
      NOTREACHED() << "Failed to get the image info.";
    vulkan_surface_->GetSwapChain()->SetCurrentImageLayout(
        vk_image_info.fImageLayout);

    gpu::SwapBuffersCompleteParams params;
    params.swap_response.swap_start = base::TimeTicks::Now();
    params.swap_response.result = vulkan_surface_->SwapBuffers();
    auto now = base::TimeTicks::Now();
    params.swap_response.swap_end = now;
    DidSwapBuffersComplete(params);

    buffer_presented_callback_.Run(
        gfx::PresentationFeedback(now, base::TimeDelta(), 0 /* flag */));

    CreateSkSurfaceForVulkan();
    swap_end = base::TimeTicks::Now();
#else
    NOTREACHED();
#endif
  }
  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, swap_start, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, swap_end, 1);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(frame.latency_info);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (!MakeCurrent())
    return;

  PullTextureUpdates(std::move(sync_tokens));

  auto& surface = offscreen_surfaces_[id].surface;
  SkSurfaceCharacterization characterization;
  // TODO(penghuang): Using characterization != ddl->characterization(), when
  // the SkSurfaceCharacterization::operator!= is implemented in Skia.
  if (!surface || !surface->characterize(&characterization) ||
      characterization != ddl->characterization()) {
    surface = SkSurface::MakeRenderTarget(gr_context(), ddl->characterization(),
                                          SkBudgeted::kNo);
    DCHECK(surface);
  }
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_)
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    surface->draw(ddl.get());
    gr_context()->flush();
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());
  for (const auto& id : ids) {
    auto it = offscreen_surfaces_.find(id);
    DCHECK(it != offscreen_surfaces_.end());
    offscreen_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(crbug.com/914502): Do this on the GPU instead of CPU with GL.
  // TODO(crbug.com/898595): Do this on the GPU instead of CPU with Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!MakeCurrent())
    return;

  DCHECK(!id || offscreen_surfaces_.find(id) != offscreen_surfaces_.end());
  auto* surface =
      id ? offscreen_surfaces_[id].surface.get() : sk_surface_.get();

  if (!is_using_vulkan()) {
    if (!context_provider_) {
      context_provider_ = base::MakeRefCounted<DirectContextProvider>(
          context_state_->context(), gl_surface_, supports_alpha_,
          gpu_preferences_, feature_info_.get());
      context_provider_->BindToCurrentThread();
    }
    ScopedUseContextProvider use_context_provider(this);

    // TODO(crbug.com/914502): Do this on the GPU instead of CPU with GL.
    // copier_->CopyFromTextureOrFramebuffer(
    //     std::move(request), output_rect,
    //     internal_format, gl_id, surface_size, flipped, color_space);

    // GLRendererCopier may have kicked off a glQuery.
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries())
      ScheduleDelayedWork();
  }

  SkBitmap bitmap;
  if (request->is_scaled()) {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.sampling_bounds.x(),
                        geometry.sampling_bounds.y());

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        bitmap, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.result_selection.width(), geometry.result_selection.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.readback_offset.x(),
                        geometry.readback_offset.y());
  }

  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to the
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  // Deliver the result. SkiaRenderer supports RGBA_BITMAP and I420_PLANES
  // only. For legacy reasons, if a RGBA_TEXTURE request is being made, clients
  // are prepared to accept RGBA_BITMAP results.
  //
  // TODO(crbug/754872): Get rid of the legacy behavior and send empty results
  // for RGBA_TEXTURE requests once tab capture is moved into VIZ.
  const CopyOutputResult::Format result_format =
      (request->result_format() == CopyOutputResult::Format::RGBA_TEXTURE)
          ? CopyOutputResult::Format::RGBA_BITMAP
          : request->result_format();
  // Note: The CopyOutputSkBitmapResult automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_format, geometry.result_selection, bitmap));
}

gpu::DecoderContext* SkiaOutputSurfaceImplOnGpu::decoder() {
  return context_provider_->decoder();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PerformDelayedWork,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(2));
}

void SkiaOutputSurfaceImplOnGpu::PerformDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedUseContextProvider use_context_provider(this);

  delayed_work_pending_ = false;
  if (MakeCurrent()) {
    decoder()->PerformIdleWork();
    decoder()->ProcessPendingQueries(false);
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries()) {
      ScheduleDelayedWork();
    }
  }
}

sk_sp<SkPromiseImageTexture> SkiaOutputSurfaceImplOnGpu::FulfillPromiseTexture(
    const gpu::MailboxHolder& mailbox_holder,
    const gfx::Size& size,
    const ResourceFormat resource_format,
    std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!shared_image_representation_factory_) {
    // TODO(https://crbug.com/900973): support shared image for Android
    // WebView.
  } else if (!*shared_image_out && mailbox_holder.mailbox.IsSharedImage()) {
    std::unique_ptr<gpu::SharedImageRepresentationSkia> shared_image =
        shared_image_representation_factory_->ProduceSkia(
            mailbox_holder.mailbox);
    if (!shared_image) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "mailbox not found in SharedImageManager.";
      return nullptr;
    }
    *shared_image_out = std::move(shared_image);
  }
  if (*shared_image_out) {
    auto promise_texture =
        (*shared_image_out)->BeginReadAccess(sk_surface_.get());
    DLOG_IF(ERROR, !promise_texture)
        << "Failed to begin read access for SharedImageRepresentationSkia";
    return promise_texture;
  }

  if (is_using_vulkan()) {
    // Probably this texture is created with wrong inteface (GLES2Interface).
    DLOG(ERROR) << "Failed to fulfill the promise texture whose backend is not "
                   "compitable with vulkan.";
    return nullptr;
  }

  auto* texture_base = mailbox_manager_->ConsumeTexture(mailbox_holder.mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    return nullptr;
  }
  BindOrCopyTextureIfNecessary(texture_base);
  GrBackendTexture backend_texture;
  gpu::GetGrBackendTexture(gl_version_info_, texture_base->target(), size,
                           texture_base->service_id(), resource_format,
                           &backend_texture);
  if (!backend_texture.isValid()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    return nullptr;
  }
  return SkPromiseImageTexture::Make(backend_texture);
}

sk_sp<SkPromiseImageTexture> SkiaOutputSurfaceImplOnGpu::FulfillPromiseTexture(
    const RenderPassId id,
    std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out) {
  DCHECK(!*shared_image_out);
  auto it = offscreen_surfaces_.find(id);
  DCHECK(it != offscreen_surfaces_.end());
  auto& surface = it->second.surface;
  auto& promise_texture = it->second.promise_texture;
  if (!promise_texture) {
    promise_texture = SkPromiseImageTexture::Make(
        surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess));
    if (!promise_texture) {
      DLOG(ERROR)
          << "Failed to fulfill the promise texture created from RenderPassId:"
          << id;
      return nullptr;
    }
  } else {
    surface->flush();
  }
  return promise_texture;
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context()->threadSafeProxy();
}

void SkiaOutputSurfaceImplOnGpu::DestroySkImages(
    std::vector<sk_sp<SkImage>>&& images,
    uint64_t sync_fence_release) {
  MakeCurrent();
  images.clear();
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

#if defined(OS_WIN)
void SkiaOutputSurfaceImplOnGpu::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}
#endif

void SkiaOutputSurfaceImplOnGpu::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params.swap_response.swap_id = pending_swap_completed_params_.front().first;
  gfx::Size pixel_size = pending_swap_completed_params_.front().second;
  pending_swap_completed_params_.pop_front();
  did_swap_buffer_complete_callback_.Run(params, pixel_size);
}

const gpu::gles2::FeatureInfo* SkiaOutputSurfaceImplOnGpu::GetFeatureInfo()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return feature_info_.get();
}

const gpu::GpuPreferences& SkiaOutputSurfaceImplOnGpu::GetGpuPreferences()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return gpu_preferences_;
}

void SkiaOutputSurfaceImplOnGpu::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaOutputSurfaceImplOnGpu::AddFilter(IPC::MessageFilter* message_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

int32_t SkiaOutputSurfaceImplOnGpu::GetRouteID() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  if (!MakeCurrent())
    return;

  auto* context = context_state_->real_context();
  auto* current_gl = context->GetCurrentGL();
  api_ = current_gl->Api;
  gl_version_info_ = context->GetVersionInfo();

  capabilities_.flipped_output_surface = gl_surface_->FlipsVertically();

  // Get alpha and stencil bits from the default frame buffer.
  api_->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
  gr_context()->resetContext(kRenderTarget_GrGLBackendState);
  const auto* version = current_gl->Version;
  GLint stencil_bits = 0;
  GLint alpha_bits = 0;
  if (version->is_desktop_core_profile) {
    api_->glGetFramebufferAttachmentParameterivEXTFn(
        GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
        &stencil_bits);
    api_->glGetFramebufferAttachmentParameterivEXTFn(
        GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
        &alpha_bits);
  } else {
    api_->glGetIntegervFn(GL_STENCIL_BITS, &stencil_bits);
    api_->glGetIntegervFn(GL_ALPHA_BITS, &alpha_bits);
  }
  CHECK_GL_ERROR();
  capabilities_.supports_stencil = stencil_bits > 0;
  supports_alpha_ = alpha_bits > 0;
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGLWithGpuService(
    GpuServiceImpl* gpu_service) {
  if (surface_handle_) {
    gl_surface_ = gpu::ImageTransportSurface::CreateNativeSurface(
        weak_ptr_factory_.GetWeakPtr(), surface_handle_, gl::GLSurfaceFormat());
  } else {
    // surface_ could be null for pixel tests. Use FakeOnScreenSurface so that
    // virtual contexts always render to the surface.
    scoped_refptr<gl::GLSurface> offscreen_surface =
        gl::init::CreateOffscreenGLSurface(gfx::Size(1, 1));
    gl_surface_ =
        base::MakeRefCounted<FakeOnScreenSurface>(offscreen_surface.get());
  }
  DCHECK(gl_surface_);

  context_state_ = gpu_service->GetContextStateForGLSurface(gl_surface_.get());
  if (!context_state_) {
    LOG(FATAL) << "Failed to create GrContext";
    // TODO(penghuang): handle the failure.
  }
  InitializeForGL();
}

void SkiaOutputSurfaceImplOnGpu::InitializeForVulkan(
    GpuServiceImpl* gpu_service) {
  context_state_ = gpu_service->GetContextStateForVulkan();
  DCHECK(context_state_);
  supports_alpha_ = true;
}

void SkiaOutputSurfaceImplOnGpu::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to bind a gl image to texture.";
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to copy a gl image to texture.";
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::OnSwapBuffers() {
  uint64_t swap_id = swap_id_++;
  gfx::Size pixel_size(sk_surface_->width(), sk_surface_->height());
  pending_swap_completed_params_.emplace_back(swap_id, pixel_size);
}

void SkiaOutputSurfaceImplOnGpu::CreateSkSurfaceForGL() {
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = gl_surface_->GetBackingFramebufferObject();
  if (supports_alpha_) {
    framebuffer_info.fFormat =
        gl_version_info_->is_es ? GL_BGRA8_EXT : GL_RGBA8;
  } else {
    framebuffer_info.fFormat = GL_RGB8_OES;
  }

  GrBackendRenderTarget render_target(size_.width(), size_.height(), 0, 8,
                                      framebuffer_info);

  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context(), render_target, kBottomLeft_GrSurfaceOrigin,
      FramebufferColorType(), color_space_.ToSkColorSpace(), &surface_props);
  DCHECK(sk_surface_);
}

void SkiaOutputSurfaceImplOnGpu::CreateSkSurfaceForVulkan() {
#if BUILDFLAG(ENABLE_VULKAN)
  auto* swap_chain = vulkan_surface_->GetSwapChain();
  auto index = swap_chain->current_image();
  auto& sk_surface = sk_surfaces_[index];
  if (!sk_surface) {
    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);
    VkImage vk_image = swap_chain->GetCurrentImage();
    VkImageLayout vk_image_layout = swap_chain->GetCurrentImageLayout();
    GrVkImageInfo vk_image_info;
    vk_image_info.fImage = vk_image;
    vk_image_info.fAlloc = {VK_NULL_HANDLE, 0, 0, 0};
    vk_image_info.fImageLayout = vk_image_layout;
    vk_image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
    vk_image_info.fFormat = VK_FORMAT_B8G8R8A8_UNORM;
    vk_image_info.fLevelCount = 1;
    GrBackendRenderTarget render_target(vulkan_surface_->size().width(),
                                        vulkan_surface_->size().height(), 0, 0,
                                        vk_image_info);
    sk_surface = SkSurface::MakeFromBackendRenderTarget(
        gr_context(), render_target, kTopLeft_GrSurfaceOrigin,
        FramebufferColorType(), nullptr, &surface_props);
    DCHECK(sk_surface);
  } else {
    auto backend = sk_surface->getBackendRenderTarget(
        SkSurface::kFlushRead_BackendHandleAccess);
    backend.setVkImageLayout(swap_chain->GetCurrentImageLayout());
  }

  sk_surface_ = sk_surface;
#endif
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent() {
  if (!is_using_vulkan()) {
    if (!context_state_->MakeCurrent(gl_surface_.get())) {
      LOG(ERROR) << "Failed to make current.";
      context_lost_callback_.Run();
      return false;
    }
    context_state_->set_need_context_state_reset(true);
  }
  return true;
}

void SkiaOutputSurfaceImplOnGpu::PullTextureUpdates(
    std::vector<gpu::SyncToken> sync_tokens) {
  if (mailbox_manager_->UsesSync()) {
    for (auto& sync_token : sync_tokens)
      mailbox_manager_->PullTextureUpdates(sync_token);
  }
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSyncAndPushTextureUpdates(
    uint64_t sync_fence_release) {
  if (mailbox_manager_->UsesSync()) {
    // If MailboxManagerSync is used, we are sharing textures between threads.
    // In this case, sync point can only guarantee GL commands are issued in
    // correct order across threads and GL contexts. However GPU driver may
    // execute GL commands out of the issuing order across GL contexts. So we
    // have to use PushTextureUpdates() and PullTextureUpdates() to ensure the
    // correct GL commands executing order.
    // PushTextureUpdates(token) will insert a GL fence into the current GL
    // context, PullTextureUpdates(token) will wait the GL fence associated with
    // the give token on the current GL context.
    // Reconstruct sync_token from sync_fence_release.
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
        command_buffer_id(), sync_fence_release);
    mailbox_manager_->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

}  // namespace viz
