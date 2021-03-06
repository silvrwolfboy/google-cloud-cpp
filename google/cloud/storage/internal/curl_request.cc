// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/curl_request.h"
#include <iostream>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {

extern "C" size_t CurlRequestOnWriteData(char* ptr, size_t size, size_t nmemb,
                                         void* userdata) {
  auto* request = reinterpret_cast<CurlRequest*>(userdata);
  return request->OnWriteData(ptr, size, nmemb);
}

extern "C" size_t CurlRequestOnHeaderData(char* contents, size_t size,
                                          size_t nitems, void* userdata) {
  auto* request = reinterpret_cast<CurlRequest*>(userdata);
  return request->OnHeaderData(contents, size, nitems);
}

StatusOr<HttpResponse> CurlRequest::MakeRequest(std::string const& payload) {
  response_payload_.clear();
  handle_.SetOption(CURLOPT_BUFFERSIZE, 128 * 1024L);
  handle_.SetOption(CURLOPT_URL, url_.c_str());
  handle_.SetOption(CURLOPT_HTTPHEADER, headers_.get());
  handle_.SetOption(CURLOPT_USERAGENT, user_agent_.c_str());
  handle_.SetOption(CURLOPT_NOSIGNAL, 1);
  handle_.SetOption(CURLOPT_UPLOAD, 0L);
  handle_.SetOption(CURLOPT_TCP_KEEPALIVE, 1L);
  handle_.EnableLogging(logging_enabled_);
  handle_.SetSocketCallback(socket_options_);
  handle_.SetOption(CURLOPT_WRITEFUNCTION, &CurlRequestOnWriteData);
  handle_.SetOption(CURLOPT_WRITEDATA, this);
  handle_.SetOption(CURLOPT_HEADERFUNCTION, &CurlRequestOnHeaderData);
  handle_.SetOption(CURLOPT_HEADERDATA, this);
  if (!payload.empty()) {
    handle_.SetOption(CURLOPT_POSTFIELDSIZE, payload.length());
    handle_.SetOption(CURLOPT_POSTFIELDS, payload.c_str());
  }
  auto status = handle_.EasyPerform();
  if (!status.ok()) {
    return status;
  }
  if (logging_enabled_) {
    handle_.FlushDebug(__func__);
  }
  auto code = handle_.GetResponseCode();
  if (!code.ok()) {
    return std::move(code).status();
  }
  return HttpResponse{code.value(), std::move(response_payload_),
                      std::move(received_headers_)};
}

std::size_t CurlRequest::OnWriteData(char* contents, std::size_t size,
                                     std::size_t nmemb) {
  response_payload_.append(contents, size * nmemb);
  return size * nmemb;
}

std::size_t CurlRequest::OnHeaderData(char* contents, std::size_t size,
                                      std::size_t nitems) {
  return CurlAppendHeaderData(received_headers_, contents, size * nitems);
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
