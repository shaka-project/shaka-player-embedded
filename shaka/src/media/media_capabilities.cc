// Copyright 2019 Google LLC
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

#include "shaka/media/media_capabilities.h"

#define DEFINE_SPECIAL_METHODS(Type)            \
  Type::~Type() {}                              \
  Type::Type(const Type&) = default;            \
  Type::Type(Type&&) = default;                 \
  Type& Type::operator=(const Type&) = default; \
  Type& Type::operator=(Type&&) = default

namespace shaka {
namespace media {

// \cond Doxygen_Skip
VideoConfiguration::VideoConfiguration()
    : width(0),
      height(0),
      bitrate(0),
      framerate(0),
      has_alpha_channel(false),
      hdr_metadata_type(HdrMetadataType::Unspecified),
      color_gamut(ColorGamut::Unspecified),
      transfer_function(TransferFunction::Unspecified) {}
DEFINE_SPECIAL_METHODS(VideoConfiguration);


AudioConfiguration::AudioConfiguration()
    : channels(0), bitrate(0), samplerate(0), spatial_rendering(false) {}
DEFINE_SPECIAL_METHODS(AudioConfiguration);


KeySystemTrackConfiguration::KeySystemTrackConfiguration() {}
DEFINE_SPECIAL_METHODS(KeySystemTrackConfiguration);


MediaCapabilitiesKeySystemConfiguration::
    MediaCapabilitiesKeySystemConfiguration()
    : init_data_type(eme::MediaKeyInitDataType::Cenc),
      distinctive_identifier(eme::MediaKeysRequirement::Optional),
      persistent_state(eme::MediaKeysRequirement::Optional) {}
DEFINE_SPECIAL_METHODS(MediaCapabilitiesKeySystemConfiguration);


MediaDecodingConfiguration::MediaDecodingConfiguration()
    : type(MediaDecodingType::MediaSource) {}
DEFINE_SPECIAL_METHODS(MediaDecodingConfiguration);


MediaCapabilitiesInfo::MediaCapabilitiesInfo()
    : supported(false), smooth(false), power_efficient(false) {}
DEFINE_SPECIAL_METHODS(MediaCapabilitiesInfo);
// \endcond Doxygen_Skip

}  // namespace media
}  // namespace shaka
