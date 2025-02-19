// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context_base.h"

#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebGL2ComputeRenderingContextBase::WebGL2ComputeRenderingContextBase(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 using_gpu_compositing,
                                 requested_attributes,
                                 Platform::kWebGL2ComputeContextType) {}

void WebGL2ComputeRenderingContextBase::DestroyContext() {
  WebGL2RenderingContextBase::DestroyContext();
}

void WebGL2ComputeRenderingContextBase::InitializeNewContext() {
  DCHECK(!isContextLost());

  WebGL2RenderingContextBase::InitializeNewContext();
}

void WebGL2ComputeRenderingContextBase::dispatchCompute(GLuint numGroupsX,
                                                        GLuint numGroupsY,
                                                        GLuint numGroupsZ) {
  ContextGL()->DispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
}

void WebGL2ComputeRenderingContextBase::bindImageTexture(GLuint unit,
                                                         WebGLTexture* texture,
                                                         GLint level,
                                                         GLboolean layered,
                                                         GLint layer,
                                                         GLenum access,
                                                         GLenum format) {
  ContextGL()->BindImageTexture(unit, ObjectOrZero(texture), level, layered,
                                layer, access, format);
}

void WebGL2ComputeRenderingContextBase::memoryBarrier(GLbitfield barriers) {
  ContextGL()->MemoryBarrierEXT(barriers);
}

void WebGL2ComputeRenderingContextBase::memoryBarrierByRegion(
    GLbitfield barriers) {
  ContextGL()->MemoryBarrierByRegion(barriers);
}

ScriptValue WebGL2ComputeRenderingContextBase::getParameter(
    ScriptState* script_state,
    GLenum pname) {
  if (isContextLost())
    return ScriptValue::CreateNull(script_state);
  switch (pname) {
    case GL_SHADING_LANGUAGE_VERSION: {
      return WebGLAny(
          script_state,
          "WebGL GLSL ES 3.10 (" +
              String(ContextGL()->GetString(GL_SHADING_LANGUAGE_VERSION)) +
              ")");
    }
    case GL_VERSION: {
      return WebGLAny(script_state,
                      "WebGL 2.0 Compute (" +
                          String(ContextGL()->GetString(GL_VERSION)) + ")");
    }
    case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
    case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
    case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_COMBINED_ATOMIC_COUNTERS:
    case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
    case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
    case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
    case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
    case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
    case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
    case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
    case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
    case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
    case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
    case GL_MAX_VERTEX_ATOMIC_COUNTERS:
    case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
      return GetIntParameter(script_state, pname);
    case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
      return GetInt64Parameter(script_state, pname);

    default:
      return WebGL2RenderingContextBase::getParameter(script_state, pname);
  }
}

void WebGL2ComputeRenderingContextBase::Trace(blink::Visitor* visitor) {
  WebGL2RenderingContextBase::Trace(visitor);
}

}  // namespace blink
