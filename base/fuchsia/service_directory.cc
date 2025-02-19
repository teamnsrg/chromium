// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/service_directory.h"

#include <lib/async/default.h>
#include <lib/svc/dir.h>
#include <lib/zx/channel.h>
#include <zircon/process.h>
#include <zircon/processargs.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"

namespace base {
namespace fuchsia {

ServiceDirectory::ServiceDirectory(
    fidl::InterfaceRequest<::fuchsia::io::Directory> request) {
  zx_status_t status =
      svc_dir_create(async_get_default_dispatcher(),
                     request.TakeChannel().release(), &svc_dir_);
  ZX_CHECK(status == ZX_OK, status);
}

ServiceDirectory::ServiceDirectory(zx::channel request)
    : ServiceDirectory(fidl::InterfaceRequest<::fuchsia::io::Directory>(
          std::move(request))) {}

ServiceDirectory::~ServiceDirectory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(services_.empty());

  zx_status_t status = svc_dir_destroy(svc_dir_);
  ZX_DCHECK(status == ZX_OK, status);
}

// static
ServiceDirectory* ServiceDirectory::GetDefault() {
  static NoDestructor<ServiceDirectory> directory(
      fidl::InterfaceRequest<::fuchsia::io::Directory>(
          zx::channel(zx_take_startup_handle(PA_DIRECTORY_REQUEST))));
  return directory.get();
}

void ServiceDirectory::AddServiceUnsafe(
    StringPiece name,
    RepeatingCallback<void(zx::channel)> connect_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(services_.find(name) == services_.end());

  std::string name_str = name.as_string();
  services_[name_str] = connect_callback;

  zx_status_t status =
      svc_dir_add_service(svc_dir_, "public", name_str.c_str(), this,
                          &ServiceDirectory::HandleConnectRequest);
  ZX_DCHECK(status == ZX_OK, status);

  // Publish to the legacy "flat" namespace, which is required by some clients.
  status = svc_dir_add_service(svc_dir_, nullptr, name_str.c_str(), this,
                               &ServiceDirectory::HandleConnectRequest);
  ZX_DCHECK(status == ZX_OK, status);
}

void ServiceDirectory::RemoveService(StringPiece name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::string name_str = name.as_string();

  auto it = services_.find(name_str);
  DCHECK(it != services_.end());
  services_.erase(it);

  zx_status_t status =
      svc_dir_remove_service(svc_dir_, "public", name_str.c_str());
  ZX_DCHECK(status == ZX_OK, status);

  // Unregister from the legacy "flat" namespace.
  status = svc_dir_remove_service(svc_dir_, nullptr, name_str.c_str());
  ZX_DCHECK(status == ZX_OK, status);
}

void ServiceDirectory::RemoveAllServices() {
  while (!services_.empty()) {
    RemoveService(services_.begin()->first);
  }
}

// static
void ServiceDirectory::HandleConnectRequest(void* context,
                                            const char* service_name,
                                            zx_handle_t service_request) {
  auto* directory = reinterpret_cast<ServiceDirectory*>(context);
  DCHECK_CALLED_ON_VALID_THREAD(directory->thread_checker_);

  auto it = directory->services_.find(service_name);

  // HandleConnectRequest() is expected to be called only for registered
  // services.
  DCHECK(it != directory->services_.end());

  it->second.Run(zx::channel(service_request));
}

}  // namespace fuchsia
}  // namespace base
