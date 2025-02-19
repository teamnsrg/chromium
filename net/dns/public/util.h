// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_UTIL_H_
#define NET_DNS_PUBLIC_UTIL_H_

#include <string>

#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

// Basic utility functions for interaction with DNS, MDNS, and host resolution.
namespace dns_util {

// Returns true if the URI template is acceptable for sending requests via the
// given method. The template must be properly formatted, GET requests require
// the template to contain a "dns" variable, an expanded template must parse
// to a valid HTTPS URL, and the "dns" variable may not be part of the hostname.
NET_EXPORT bool IsValidDoHTemplate(const std::string& server_template,
                                   const std::string& server_method);

// Gets the endpoint for the multicast group a socket should join to receive
// MDNS messages. Such sockets should also bind to the endpoint from
// GetMdnsReceiveEndPoint().
//
// This is also the endpoint messages should be sent to to send MDNS messages.
NET_EXPORT IPEndPoint GetMdnsGroupEndPoint(AddressFamily address_family);

// Gets the endpoint sockets should be bound to to receive MDNS messages. Such
// sockets should also join the multicast group from GetMdnsGroupEndPoint().
NET_EXPORT IPEndPoint GetMdnsReceiveEndPoint(AddressFamily address_family);

}  // namespace dns_util
}  // namespace net

#endif  // NET_DNS_PUBLIC_UTIL_H_
