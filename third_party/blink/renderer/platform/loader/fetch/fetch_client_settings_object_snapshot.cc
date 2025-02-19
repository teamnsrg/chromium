// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

#include "third_party/blink/renderer/platform/heap/trace_traits.h"

namespace blink {

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const FetchClientSettingsObject& fetch_client_setting_object)
    : FetchClientSettingsObjectSnapshot(
          fetch_client_setting_object.BaseURL(),
          fetch_client_setting_object.GetSecurityOrigin(),
          fetch_client_setting_object.GetReferrerPolicy(),
          fetch_client_setting_object.GetOutgoingReferrer(),
          fetch_client_setting_object.GetHttpsState(),
          fetch_client_setting_object.MimeTypeCheckForClassicWorkerScript(),
          fetch_client_setting_object.GetAddressSpace()) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData> data)
    : FetchClientSettingsObjectSnapshot(
          data->base_url,
          data->security_origin,
          data->referrer_policy,
          data->outgoing_referrer,
          data->https_state,
          data->mime_type_check_for_classic_worker_script,
          data->address_space) {}

FetchClientSettingsObjectSnapshot::FetchClientSettingsObjectSnapshot(
    const KURL& base_url,
    const scoped_refptr<const SecurityOrigin> security_origin,
    network::mojom::ReferrerPolicy referrer_policy,
    const String& outgoing_referrer,
    HttpsState https_state,
    AllowedByNosniff::MimeTypeCheck mime_type_check_for_classic_worker_script,
    base::Optional<mojom::IPAddressSpace> address_space)
    : base_url_(base_url),
      security_origin_(std::move(security_origin)),
      referrer_policy_(referrer_policy),
      outgoing_referrer_(outgoing_referrer),
      https_state_(https_state),
      mime_type_check_for_classic_worker_script_(
          mime_type_check_for_classic_worker_script),
      address_space_(address_space) {}

}  // namespace blink
