// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/debugging_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace download {

void DebuggingClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<DownloadMetaData>& downloads) {}

void DebuggingClient::OnServiceUnavailable() {}

Client::ShouldDownload DebuggingClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  return Client::ShouldDownload::CONTINUE;
}

void DebuggingClient::OnDownloadUpdated(const std::string& guid,
                                        uint64_t bytes_uploaded,
                                        uint64_t bytes_downloaded) {}

void DebuggingClient::OnDownloadFailed(const std::string& guid,
                                       const CompletionInfo& completion_info,
                                       FailureReason reason) {}

void DebuggingClient::OnDownloadSucceeded(
    const std::string& guid,
    const CompletionInfo& completion_info) {
  // TODO(dtrainor): Automatically remove the downloaded file.  For now this
  // will happen after the timeout window in the service.
}

bool DebuggingClient::CanServiceRemoveDownloadedFile(const std::string& guid,
                                                     bool force_delete) {
  return true;
}

void DebuggingClient::GetUploadData(const std::string& guid,
                                    GetUploadDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

}  // namespace download
