// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/media/media_utils.h"

#include <algorithm>
#include <cstdlib>
#include <type_traits>
#include <utility>

#include "src/util/macros.h"

namespace shaka {
namespace media {

namespace {

struct StringMapping {
  const char* source;
  const char* dest;
};

const StringMapping kContainerMap[] = {
    {"mp4", "mov"},
    {"webm", "matroska"},
};
const StringMapping kCodecMap[] = {
    {"avc1", "h264"}, {"avc3", "h264"}, {"hev1", "hevc"},
    {"hvc1", "hevc"}, {"vp09", "vp9"},  {"mp4a", "aac"},
};

constexpr const char* kWhitespaceCharacters = " \f\n\r\t\v";

/** @returns Whether the given string is a MIME token. */
bool IsToken(const std::string& token) {
  return !token.empty() &&
         token.find_first_of(R"(()<>@,;:\"/[]?=)") == std::string::npos &&
         token.find_first_of(kWhitespaceCharacters) == std::string::npos;
}

std::string substr_end(const std::string& source, std::string::size_type start,
                       std::string::size_type end) {
  if (end == std::string::npos)
    return source.substr(start);

  return source.substr(start, end - start);
}

}  // namespace

bool ParseMimeType(const std::string& source, std::string* type,
                   std::string* subtype,
                   std::unordered_map<std::string, std::string>* params) {
  // Extract type.
  const auto type_end = source.find('/');
  if (type_end == std::string::npos)
    return false;
  std::string type_tmp =
      util::TrimAsciiWhitespace(substr_end(source, 0, type_end));
  if (!IsToken(type_tmp))
    return false;
  if (type)
    *type = std::move(type_tmp);

  // Extract subtype.
  const auto subtype_end = source.find(';', type_end);
  std::string subtype_tmp =
      util::TrimAsciiWhitespace(substr_end(source, type_end + 1, subtype_end));
  if (!IsToken(subtype_tmp))
    return false;
  if (subtype)
    *subtype = std::move(subtype_tmp);

  // Extract parameters.
  auto param_end = subtype_end;
  while (param_end != std::string::npos) {
    const auto name_end = source.find('=', param_end);
    if (name_end == std::string::npos)
      return false;

    const std::string param_name =
        util::TrimAsciiWhitespace(substr_end(source, param_end + 1, name_end));
    if (!IsToken(param_name))
      return false;

    const auto value_start =
        source.find_first_not_of(kWhitespaceCharacters, name_end + 1);
    std::string value;
    if (value_start == std::string::npos) {
      param_end = std::string::npos;
      value = "";
    } else if (source[value_start] == '"') {
      const auto str_end = source.find('"', value_start + 1);
      if (str_end == std::string::npos)
        return false;
      value = substr_end(source, value_start + 1, str_end);
      param_end = source.find(';', str_end);

      const std::string extra =
          util::TrimAsciiWhitespace(substr_end(source, str_end + 1, param_end));
      if (!extra.empty())
        return false;
    } else {
      param_end = source.find(';', name_end);
      value = util::TrimAsciiWhitespace(
          substr_end(source, name_end + 1, param_end));
      if (!IsToken(value))
        return false;
    }
    if (params)
      params->emplace(util::ToAsciiLower(param_name), value);
  }

  return true;
}

std::string NormalizeContainer(const std::string& container) {
  for (const auto& entry : kContainerMap)
    if (container == entry.source)
      return entry.dest;
  return container;
}

std::string NormalizeCodec(const std::string& codec) {
  const std::string simple_codec = codec.substr(0, codec.find('.'));
  for (const auto& entry : kCodecMap)
    if (simple_codec == entry.source)
      return entry.dest;
  return simple_codec;
}


BufferedRanges IntersectionOfBufferedRanges(
    const std::vector<BufferedRanges>& sources) {
  if (sources.empty())
    return BufferedRanges();

  BufferedRanges accumulated = sources[0];
  for (auto& source : sources) {
    BufferedRanges temp;
    size_t acc_i = 0, source_i = 0;

    while (acc_i < accumulated.size() && source_i < source.size()) {
      const double start =
          std::max(accumulated[acc_i].start, source[source_i].start);
      const double end = std::min(accumulated[acc_i].end, source[source_i].end);
      if (end > start)
        temp.emplace_back(start, end);

      if (accumulated[acc_i].end < source[source_i].end)
        acc_i++;
      else
        source_i++;
    }

    accumulated.swap(temp);
  }

  return accumulated;
}

MediaDecodingConfiguration ConvertMimeToDecodingConfiguration(
    const std::string& mime_type, MediaDecodingType type) {
  media::MediaDecodingConfiguration info;
  info.type = type;
  info.audio.content_type = info.video.content_type = mime_type;

  std::unordered_map<std::string, std::string> params;
  if (ParseMimeType(mime_type, nullptr, nullptr, &params)) {
    info.video.width = std::atol(params["width"].c_str());
    info.video.height = std::atol(params["height"].c_str());
    info.video.framerate = std::atof(params["framerate"].c_str());

    info.audio.channels =
        static_cast<uint16_t>(std::atoi(params["channels"].c_str()));

    info.audio.bitrate = info.video.bitrate =
        std::atoll(params["bitrate"].c_str());
  }

  return info;
}

#ifndef OS_IOS
std::pair<uint32_t, uint32_t> GetScreenResolution() {
  return {UINT32_MAX, UINT32_MAX};
}
#endif

}  // namespace media
}  // namespace shaka
