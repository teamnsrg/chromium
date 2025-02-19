// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PERFETTO_TRACING_COORDINATOR_H_
#define SERVICES_TRACING_PERFETTO_PERFETTO_TRACING_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/coordinator.h"
#include "services/tracing/public/mojom/tracing.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace tracing {

// This provides an alternate implementation of a tracing
// coordinator to clients (like the content::TracingController)
// but which uses Perfetto rather than the TraceLog for
// collecting trace events behind the scenes.
class PerfettoTracingCoordinator : public Coordinator {
 public:
  PerfettoTracingCoordinator(AgentRegistry* agent_registry,
                             base::RepeatingClosure on_disconnect_callback);

  ~PerfettoTracingCoordinator() override;

  void BindCoordinatorRequest(
      mojom::CoordinatorRequest request,
      const service_manager::BindSourceInfo& source_info);

  // mojom::Coordinator implementation.
  // Called by the tracing controller.
  void StartTracing(const std::string& config) override;
  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    StopAndFlushCallback callback) override;
  void StopAndFlushAgent(mojo::ScopedDataPipeProducerHandle stream,
                         const std::string& agent_label,
                         StopAndFlushCallback callback) override;
  void IsTracing(IsTracingCallback callback) override;
  void RequestBufferUsage(RequestBufferUsageCallback callback) override;

 private:
  void BindOnSequence(mojom::CoordinatorRequest request);
  void OnTracingOverCallback();
  void OnClientConnectionError() override;

  mojo::Binding<mojom::Coordinator> binding_;

  class TracingSession;
  std::unique_ptr<TracingSession> tracing_session_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PerfettoTracingCoordinator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerfettoTracingCoordinator);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PERFETTO_TRACING_COORDINATOR_H_
