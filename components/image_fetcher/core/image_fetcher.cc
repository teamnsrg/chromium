// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_fetcher.h"

namespace image_fetcher {

ImageFetcherParams::ImageFetcherParams(
    const net::NetworkTrafficAnnotationTag network_traffic_annotation_tag)
    : network_traffic_annotation_tag_(network_traffic_annotation_tag) {}

ImageFetcherParams::ImageFetcherParams(const ImageFetcherParams& params) =
    default;

ImageFetcherParams::ImageFetcherParams(ImageFetcherParams&& params) = default;

}  // namespace image_fetcher