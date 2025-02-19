// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SERVICE_DIRECTORY_H_
#define BASE_FUCHSIA_SERVICE_DIRECTORY_H_

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/zx/channel.h>
#include <string>
#include <utility>

#include "base/base_export.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_checker.h"

typedef struct svc_dir svc_dir_t;

namespace base {
namespace fuchsia {

// Directory of FIDL services published for other processes to consume. Services
// published in this directory can be discovered from other processes by name.
// Normally this class should be used by creating a ScopedServiceBinding
// instance. This ensures that the service is unregistered when the
// implementation is destroyed. GetDefault() should be used to get the default
// ServiceDirectory for the current process. The default instance exports
// services via a channel supplied at process creation time.
//
// Not thread-safe. All methods must be called on the thread that created the
// object.
class BASE_EXPORT ServiceDirectory {
 public:
  // Responds to service requests over the supplied |request| channel.
  explicit ServiceDirectory(
      fidl::InterfaceRequest<::fuchsia::io::Directory> request);

  // TODO(https://crbug.com/920920): Clean up callers and remove this synonym.
  explicit ServiceDirectory(zx::channel request);

  ~ServiceDirectory();

  // Returns default ServiceDirectory instance for the current process. It
  // publishes services to the directory provided by the process creator.
  static ServiceDirectory* GetDefault();

  template <typename Interface>
  void AddService(RepeatingCallback<void(fidl::InterfaceRequest<Interface>)>
                      connect_callback) {
    AddServiceUnsafe(
        Interface::Name_,
        BindRepeating(
            [](decltype(connect_callback) callback, zx::channel request) {
              callback.Run(
                  fidl::InterfaceRequest<Interface>(std::move(request)));
            },
            connect_callback));
  }
  void RemoveService(StringPiece name);
  void RemoveAllServices();

  // Passes requests for |name| through to a generic |connect_callback|.
  // This is used only when proxying requests for interfaces not known at
  // compile-time. Use the type-safe APIs above whenever possible.
  void AddServiceUnsafe(StringPiece name,
                        RepeatingCallback<void(zx::channel)> connect_callback);

  // TODO(https://crbug.com/920920): Clean up callers and remove this synonym.
  void AddService(StringPiece name,
                  RepeatingCallback<void(zx::channel)> connect_callback) {
    AddServiceUnsafe(name, std::move(connect_callback));
  }

 private:
  // Called by |svc_dir_| to handle service requests.
  static void HandleConnectRequest(void* context,
                                   const char* service_name,
                                   zx_handle_t service_request);

  THREAD_CHECKER(thread_checker_);

  svc_dir_t* svc_dir_ = nullptr;
  flat_map<std::string, RepeatingCallback<void(zx::channel)>> services_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDirectory);
};

}  // namespace fuchsia
}  // namespace base

#endif  // BASE_FUCHSIA_SERVICE_DIRECTORY_H_
