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

#ifndef SHAKA_EMBEDDED_MEDIA_MEDIA_CAPABILITIES_H_
#define SHAKA_EMBEDDED_MEDIA_MEDIA_CAPABILITIES_H_

#include <string>
#include <vector>

#include "../eme/configuration.h"
#include "../macros.h"

namespace shaka {
namespace media {

/**
 * @defgroup mediaCapabilities Media Capabilities
 * @ingroup media
 * These types are based on the Media Capabilities API on the Web.
 * See https://w3c.github.io/media-capabilities/
 */
//@{

/** Defines possible media playback types for decoding. */
enum class MediaDecodingType : uint8_t {
  /** Direct-playback of files through src=. */
  File,

  /** Playback through MSE. */
  MediaSource,
};

/** Defines possible HDR metadata types. */
enum class HdrMetadataType : uint8_t {
  Unspecified,
  SmpteSt2086,
  SmpteSt2094_10,
  SmpteSt2094_40,
};

/** Defines possible color gamut values. */
enum class ColorGamut : uint8_t {
  Unspecified,

  /** Represents the sRGB color gamut. */
  SRGB,

  /** Represents the DCI P3 Color Space color gamut. */
  P3,

  /** Represents the ITU-R Recommendation BT.2020 color gamut. */
  REC2020,
};

/** Defines possible transfer function values. */
enum class TransferFunction : uint8_t {
  Unspecified,

  /** Represents the sRGB transfer function. */
  SRGB,

  /**
   * Represents the "Perceptual Quantizer" transfer function from SMPTE ST 2084.
   */
  PQ,

  /** Represents the "Hybrid Log Gamma" transfer function from BT.2100. */
  HLG,
};

/**
 * Defines the capabilities of the video decoder to query.  Many of these fields
 * may be unset for a query.  The only field that is required is
 * @a content_type.  If that is unset, the query is for audio-only content and
 * this object should be ignored.
 */
struct SHAKA_EXPORT VideoConfiguration final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(VideoConfiguration);

  /**
   * Contains the full MIME type that is being queried.  This can be the empty
   * string if we are querying audio-only content.
   */
  std::string content_type;

  /** The width of the video, in pixels. */
  uint32_t width;

  /** The height of the video, in pixels. */
  uint32_t height;

  /** The average bitrate of the video, in bits per second. */
  uint64_t bitrate;

  /** The framerate of the video, in frames per second. */
  double framerate;

  /** Whether the video frames have alpha channels in them. */
  bool has_alpha_channel;

  /** The type of HDR metadata that is used. */
  HdrMetadataType hdr_metadata_type;

  /** The set of colors that are intended to be displayed. */
  ColorGamut color_gamut;

  /** A transfer function to map decoded media colors to display colors. */
  TransferFunction transfer_function;
};

/**
 * Defines the capabilities of the audio decoder to query.  Many of these fields
 * may be unset for a query.  The only field that is required is
 * @a content_type.  If that is unset, the query is for video-only content and
 * this object should be ignored.
 */
struct SHAKA_EXPORT AudioConfiguration final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(AudioConfiguration);

  /**
   * Contains the full MIME type that is being queried.  This can be the empty
   * string if we are querying video-only content.
   */
  std::string content_type;

  /** The number of channels. */
  uint16_t channels;

  /** The average bitrate of the video, in bits per second. */
  uint64_t bitrate;

  /** The sample rate of the audio, in number of samples per second (Hz). */
  uint32_t samplerate;

  /** Whether spatial rendering of audio is required. */
  bool spatial_rendering;
};

struct SHAKA_EXPORT KeySystemTrackConfiguration final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(KeySystemTrackConfiguration);

  /** The EME robustness requirement. */
  std::string robustness;
};

/**
 * Defines capabilities of the key system required to play protected content.
 * If the @a key_system field is non-empty, the content will be encrypted and
 * the following settings will be used.
 *
 * In MediaPlayer, Decoder, and Demuxer, this object can usually be ignored.  A
 * valid EME implementation object will be passed in and support for EME will be
 * handled by other classes.
 */
struct SHAKA_EXPORT MediaCapabilitiesKeySystemConfiguration final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(MediaCapabilitiesKeySystemConfiguration);

  /** The EME key system ID the content is protected with. */
  std::string key_system;

  /** Type type of EME init data that will be used. */
  eme::MediaKeyInitDataType init_data_type;

  /** The requirements for distinctive identifiers. */
  eme::MediaKeysRequirement distinctive_identifier;

  /** The requirements for persistent state. */
  eme::MediaKeysRequirement persistent_state;

  /** The types of sessions that will be used. */
  std::vector<eme::MediaKeySessionType> session_types;

  /** The requirements for the audio track. */
  KeySystemTrackConfiguration audio;

  /** The requirements for the video track. */
  KeySystemTrackConfiguration video;
};

/** Defines a possible decoder configuration to query. */
struct SHAKA_EXPORT MediaDecodingConfiguration final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(MediaDecodingConfiguration);

  /** The type of playback that is requested. */
  MediaDecodingType type;

  /** The video configuration that is requested. */
  VideoConfiguration video;

  /** The audio configuration that is requested. */
  AudioConfiguration audio;

  /** The EME configuration that is requested. */
  MediaCapabilitiesKeySystemConfiguration key_system_configuration;
};

/** Defines the results of a media capabilities check. */
struct SHAKA_EXPORT MediaCapabilitiesInfo final {
  SHAKA_DECLARE_STRUCT_SPECIAL_METHODS(MediaCapabilitiesInfo);

  /** Whether the configuration is supported. */
  bool supported;

  /** Whether the configuration allows for smooth playback. */
  bool smooth;

  /** Whether the configuration is power efficient. */
  bool power_efficient;


  MediaCapabilitiesInfo operator&(const MediaCapabilitiesInfo& other) const {
    MediaCapabilitiesInfo ret;
    ret.supported = supported & other.supported;
    ret.smooth = supported & other.smooth;
    ret.power_efficient = supported & other.power_efficient;
    return ret;
  }
};

//@}

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_MEDIA_CAPABILITIES_H_
