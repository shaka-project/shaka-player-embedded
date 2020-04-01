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

#ifndef SHAKA_EMBEDDED_MEDIA_DEMUXER_H_
#define SHAKA_EMBEDDED_MEDIA_DEMUXER_H_

#include <functional>
#include <memory>

#include "../eme/configuration.h"
#include "../macros.h"
#include "frames.h"

namespace shaka {
namespace media {

/**
 * This is given raw bytes from an input stream and produces EncodedFrame
 * objects.  This must do so synchronously and is called on a background thread.
 * This is only used from a single thread after being created.
 *
 * @ingroup media
 */
class SHAKA_EXPORT Demuxer {
 public:
  /**
   * Defines an interface for listening for demuxer events.  These callbacks are
   * invoked by the Demuxer when events happen.  These can be called on
   * any thread.
   */
  class SHAKA_EXPORT Client {
   public:
    SHAKA_DECLARE_INTERFACE_METHODS(Client);

    /**
     * Called after the first init segment has been processed.
     * @param duration The estimated duration of the stream, based on the init
     *   segment.  Will be Infinity if the duration is not known.
     */
    virtual void OnLoadedMetaData(double duration) = 0;

    /**
     * Called when a new encrypted init data is seen.  This should not be called
     * for init data that is given a second time.
     */
    virtual void OnEncrypted(eme::MediaKeyInitDataType type,
                             const uint8_t* data, size_t size) = 0;
  };

  SHAKA_DECLARE_INTERFACE_METHODS(Demuxer);

  /**
   * Switches to demux content of the given MIME type.  It is not required for
   * this to be supported; but if it is supported, this will be called before
   * changing containers.  This is not called for changing codec profiles or
   * adaptation.
   *
   * @param mime_type The MIME type to start demuxing.
   * @return True on success, false on error or unsupported.
   */
  virtual bool SwitchType(const std::string& mime_type);

  /**
   * Resets the demuxer to parse a new stream.  This may be called when
   * adapting before parsing a new stream.  This should reset any partial
   * reads and should prepare to read from a new stream.  This may not be called
   * during adaptation, so the demuxer should still handle getting a new init
   * segment without calling Reset first.
   */
  virtual void Reset() = 0;

  /**
   * Attempts to demux the given data into some number of encoded frames.
   *
   * If the data contains multiple streams  (i.e. multiplexed content), then
   * all frames will be given and they will be separated based on their
   * @a stream_info field.
   *
   * This may be given segments from a different source after starting.  This
   * will be first given the init segment for the new stream then the new
   * segments.  This should support this case and reinitialize the demxuer if
   * needed.  The resulting frames should have different @a stream_info fields
   * from before, even if they are of the same type and codec.
   *
   * @param timestamp_offset The current offset to add to the timestamps.
   * @param data The data to demux.
   * @param size The number of bytes in data.
   * @param frames [OUT] Where to insert newly created frames.
   * @return True on success, false on error.
   */
  virtual bool Demux(double timestamp_offset, const uint8_t* data, size_t size,
                     std::vector<std::shared_ptr<EncodedFrame>>* frames) = 0;
};

/**
 * Defines a factory used to create Demuxer instances and to query supported
 * content types.
 */
class SHAKA_EXPORT DemuxerFactory {
 public:
  SHAKA_DECLARE_INTERFACE_METHODS(DemuxerFactory);

  /** @return The current DemuxerFactory instance. */
  static const DemuxerFactory* GetFactory();

  /**
   * Sets the current DemuxerFactory instance.  This is used to query and create
   * all future Demuxer instances.  This can be changed at any time, but will
   * only affect new Demuxer instances.  Passing nullptr will reset to the
   * default factory.
   *
   * @param factory The new DemuxerFactory instance to use.
   */
  static void SetFactory(const DemuxerFactory* factory);


  /**
   * Determines whether the given MIME type can be demuxed.
   * @param mime_type The full MIME type to check.
   * @return True if the content can be demuxed, false otherwise.
   */
  virtual bool IsTypeSupported(const std::string& mime_type) const = 0;

  /**
   * Determines if the given codec string represents a video codec.  This is
   * only given a single codec, not a MIME type.  This is only called when
   * IsTypeSupported returns true.
   *
   * @param codec The codec to check.
   * @return True if the codec is for video, false for audio.
   */
  virtual bool IsCodecVideo(const std::string& codec) const = 0;

  /**
   * Determines whether you can call Demuxer::SwitchType to switch between the
   * given MIME types.
   *
   * @param old_mime_type The full MIME type the Demuxer was using before.
   * @param new_mime_type The full MIME type we are switching to.
   * @return True if this is supported, false if not supported.
   */
  virtual bool CanSwitchType(const std::string& old_mime_type,
                             const std::string& new_mime_type) const;

  /**
   * Creates a new Demuxer instance to initially read the given type of content.
   *
   * @param mime_type The full MIME type of the content being parsed.
   * @param client A client object to raise events to; this will live as long as
   *   the resulting Demuxer instance.
   * @return The resulting Demuxer instance, or nullptr on error.
   */
  virtual std::unique_ptr<Demuxer> Create(const std::string& mime_type,
                                          Demuxer::Client* client) const = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // SHAKA_EMBEDDED_MEDIA_DEMUXER_H_
