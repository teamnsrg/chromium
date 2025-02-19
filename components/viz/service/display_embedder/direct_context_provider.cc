// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/direct_context_provider.h"

#include <stdint.h>

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/command_buffer_direct.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace viz {

DirectContextProvider::DirectContextProvider(
    scoped_refptr<gl::GLContext> gl_context,
    scoped_refptr<gl::GLSurface> gl_surface,
    bool supports_alpha,
    const gpu::GpuPreferences& gpu_preferences,
    gpu::gles2::FeatureInfo* feature_info)
    : translator_cache_(gpu_preferences) {
  DCHECK(gl_context->IsCurrent(gl_surface.get()));

  auto limits = gpu::SharedMemoryLimits::ForMailboxContext();
  auto group = base::MakeRefCounted<gpu::gles2::ContextGroup>(
      gpu_preferences, true, &mailbox_manager_, /*memory_tracker=*/nullptr,
      &translator_cache_, &completeness_cache_, feature_info, true,
      &image_manager_, /*image_factory=*/nullptr,
      /*progress_reporter=*/nullptr, gpu_feature_info_, &discardable_manager_,
      &passthrough_discardable_manager_, &shared_image_manager_);

  transfer_buffer_manager_ =
      std::make_unique<gpu::TransferBufferManager>(nullptr);
  auto command_buffer = std::make_unique<gpu::CommandBufferDirect>(
      transfer_buffer_manager_.get());

  std::unique_ptr<gpu::gles2::GLES2Decoder> decoder(
      gpu::gles2::GLES2Decoder::Create(command_buffer.get(),
                                       command_buffer->service(), &outputter_,
                                       group.get()));

  command_buffer->set_handler(decoder.get());

  gpu::ContextCreationAttribs attribs;
  attribs.alpha_size = supports_alpha ? 8 : 0;
  attribs.buffer_preserved = false;
  attribs.bind_generates_resource = true;
  attribs.fail_if_major_perf_caveat = false;
  attribs.lose_context_when_out_of_memory = true;
  attribs.context_type = gpu::CONTEXT_TYPE_OPENGLES2;

  context_result_ =
      decoder->Initialize(gl_surface, gl_context, gl_surface->IsOffscreen(),
                          gpu::gles2::DisallowedFeatures(), attribs);
  if (context_result_ != gpu::ContextResult::kSuccess)
    return;

  auto gles2_cmd_helper =
      std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer.get());
  context_result_ = gles2_cmd_helper->Initialize(limits.command_buffer_size);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    decoder->Destroy(true);
    return;
  }
  // Client side Capabilities queries return reference, service side return
  // value. Here two sides are joined together.
  capabilities_ = decoder->GetCapabilities();

  auto transfer_buffer =
      std::make_unique<gpu::TransferBuffer>(gles2_cmd_helper.get());

  gles2_cmd_helper_ = std::move(gles2_cmd_helper);
  transfer_buffer_ = std::move(transfer_buffer);
  command_buffer_ = std::move(command_buffer);
  decoder_ = std::move(decoder);
  gl_context_ = std::move(gl_context);

  gles2_implementation_ = std::make_unique<gpu::gles2::GLES2Implementation>(
      gles2_cmd_helper_.get(), nullptr, transfer_buffer_.get(),
      attribs.bind_generates_resource, attribs.lose_context_when_out_of_memory,
      /*kSupportClientSideArrays=*/false, this);

  context_result_ = gles2_implementation_->Initialize(limits);
  if (context_result_ != gpu::ContextResult::kSuccess) {
    Destroy();
    return;
  }

  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "viz::DirectContextProvider", base::ThreadTaskRunnerHandle::Get());
}

DirectContextProvider::~DirectContextProvider() {
  if (decoder_)
    Destroy();
}

void DirectContextProvider::Destroy() {
  DCHECK(decoder_);
  // The client gl interface might still be set to current global
  // interface. This will be cleaned up in ApplyContextReleased
  // with AutoCurrentContextRestore.
  gles2_implementation_.reset();
  gl_context_.reset();
  transfer_buffer_.reset();
  gles2_cmd_helper_.reset();
  command_buffer_.reset();

  bool have_context = !decoder_->WasContextLost();
  decoder_->Destroy(have_context);
  decoder_.reset();
}

void DirectContextProvider::SetGLRendererCopierRequiredState() {
  gles2_implementation_->BindFramebuffer(GL_FRAMEBUFFER, 0);
  gles2_implementation_->Disable(GL_SCISSOR_TEST);
  gles2_implementation_->Disable(GL_STENCIL_TEST);
  gles2_implementation_->Disable(GL_BLEND);
}

void DirectContextProvider::AddRef() const {
  base::RefCountedThreadSafe<DirectContextProvider>::AddRef();
}

void DirectContextProvider::Release() const {
  base::RefCountedThreadSafe<DirectContextProvider>::Release();
}

gpu::ContextResult DirectContextProvider::BindToCurrentThread() {
  return context_result_;
}

gpu::gles2::GLES2Interface* DirectContextProvider::ContextGL() {
  return gles2_implementation_.get();
}

gpu::ContextSupport* DirectContextProvider::ContextSupport() {
  return gles2_implementation_.get();
}

class GrContext* DirectContextProvider::GrContext() {
  NOTREACHED();
  return nullptr;
}

gpu::SharedImageInterface* DirectContextProvider::SharedImageInterface() {
  NOTREACHED();
  return nullptr;
}

ContextCacheController* DirectContextProvider::CacheController() {
  NOTREACHED();
  return nullptr;
}

base::Lock* DirectContextProvider::GetLock() {
  NOTREACHED();
  return nullptr;
}

const gpu::Capabilities& DirectContextProvider::ContextCapabilities() const {
  return capabilities_;
}

const gpu::GpuFeatureInfo& DirectContextProvider::GetGpuFeatureInfo() const {
  return gpu_feature_info_;
}

void DirectContextProvider::AddObserver(ContextLostObserver* obs) {
  observers_.AddObserver(obs);
}

void DirectContextProvider::RemoveObserver(ContextLostObserver* obs) {
  observers_.RemoveObserver(obs);
}

void DirectContextProvider::OnContextLost() {
  // TODO(https://crbug.com/927460): Instrument this with a context loss UMA
  // stat shared with SkiaRenderer.
  for (auto& observer : observers_)
    observer.OnContextLost();
}

bool DirectContextProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_EQ(context_result_, gpu::ContextResult::kSuccess);

  gles2_implementation_->OnMemoryDump(args, pmd);
  gles2_cmd_helper_->OnMemoryDump(args, pmd);

  return true;
}

void DirectContextProvider::SetGpuControlClient(gpu::GpuControlClient*) {
  // The client is not currently called, so don't store it.
}

const gpu::Capabilities& DirectContextProvider::GetCapabilities() const {
  return capabilities_;
}

int32_t DirectContextProvider::CreateImage(ClientBuffer buffer,
                                           size_t width,
                                           size_t height) {
  NOTREACHED();
  return -1;
}

void DirectContextProvider::DestroyImage(int32_t id) {
  NOTREACHED();
}

void DirectContextProvider::SignalQuery(uint32_t query,
                                        base::OnceClosure callback) {
  decoder_->SetQueryCallback(query, std::move(callback));
}

void DirectContextProvider::CreateGpuFence(uint32_t gpu_fence_id,
                                           ClientGpuFence source) {
  NOTREACHED();
}

void DirectContextProvider::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  NOTREACHED();
}

void DirectContextProvider::SetLock(base::Lock*) {
  NOTREACHED();
}

void DirectContextProvider::EnsureWorkVisible() {
  NOTREACHED();
}

gpu::CommandBufferNamespace DirectContextProvider::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::INVALID;
}

gpu::CommandBufferId DirectContextProvider::GetCommandBufferID() const {
  return gpu::CommandBufferId();
}

void DirectContextProvider::FlushPendingWork() {
  NOTREACHED();
}

uint64_t DirectContextProvider::GenerateFenceSyncRelease() {
  NOTREACHED();
  return 0;
}

bool DirectContextProvider::IsFenceSyncReleased(uint64_t release) {
  NOTREACHED();
  return false;
}

void DirectContextProvider::SignalSyncToken(const gpu::SyncToken& sync_token,
                                            base::OnceClosure callback) {
  NOTREACHED();
}

void DirectContextProvider::WaitSyncToken(const gpu::SyncToken& sync_token) {
  NOTREACHED();
}

bool DirectContextProvider::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken& sync_token) {
  return false;
}

}  // namespace viz
