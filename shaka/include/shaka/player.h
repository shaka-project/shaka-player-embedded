// Copyright 2016 Google LLC
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

#ifndef SHAKA_EMBEDDED_PLAYER_H_
#define SHAKA_EMBEDDED_PLAYER_H_

#include <math.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "async_results.h"
#include "error.h"
#include "js_manager.h"
#include "macros.h"
#include "manifest.h"
#include "media/media_player.h"
#include "net.h"
#include "player_externs.h"
#include "stats.h"
#include "track.h"

namespace shaka {

/** @ingroup player */
struct DefaultValueType final {};

/**
 * A signal value used to set a configuration value to the default value.
 * @ingroup player
 */
extern SHAKA_EXPORT const DefaultValueType DefaultValue;

/**
 * Represents a JavaScript shaka.Player instance.  This handles loading
 * manifests and changing tracks.
 *
 * @ingroup player
 */
class SHAKA_EXPORT Player final {
 public:
  /**
   * Defines an interface for listening for Player events.  These callbacks are
   * invoked on a background thread by the Player object.
   */
  class Client {
   public:
    SHAKA_DECLARE_INTERFACE_METHODS(Client);

    /**
     * Called when an error occurs asynchronously.
     * @param error The error that occurred.
     */
    virtual void OnError(const Error& error);

    /**
     * Called when the current buffering state changes.
     * @param is_buffering Whether we are currently in a buffering state.
     */
    virtual void OnBuffering(bool is_buffering);
  };

  /**
   * Creates a new Player instance.
   * @param engine The JavaScript engine to use.
   */
  Player(JsManager* engine);
  Player(Player&&);
  ~Player();

  Player& operator=(Player&&);

  SHAKA_NON_COPYABLE_TYPE(Player);


  enum class LogLevel : uint8_t {
    // These have the same values as shaka.log.Level.
    None = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4,
    V1 = 5,
    V2 = 6,
  };

  /**
   * Sets the log level of the JavaScript Shaka Player.  This only works if the
   * Shaka Player JS file is a debug build.
   *
   * @param engine The JavaScript engine to use.
   * @param level The log level to set to.
   */
  static AsyncResults<void> SetLogLevel(JsManager* engine, LogLevel level);

  /**
   * Gets the log level of the Shaka Player JavaScript.
   *
   * @param engine The JavaScript engine to use.
   * @return A future to the log level.
   */
  static AsyncResults<LogLevel> GetLogLevel(JsManager* engine);

  /**
   * Gets the version string of the Shaka Player JavaScript.
   *
   * @param engine The JavaScript engine to use.
   * @return A future to the version string.
   */
  static AsyncResults<std::string> GetPlayerVersion(JsManager* engine);


  /**
   * Initializes the Player instance.  This must be called once before any other
   * method.
   *
   * This can be given a MediaPlayer to attach to immediately.  If not given,
   * you need to call Attach before loading content.
   *
   * @param client The client that handles asynchronous events.
   * @param player The MediaPlayer that controls playback.
   */
  AsyncResults<void> Initialize(Client* client,
                                media::MediaPlayer* player = nullptr);

  /**
   * Destroys the contained Player instance.  This is called automatically in
   * the destructor, but calling it explicitly allows for handling of possible
   * errors.
   */
  AsyncResults<void> Destroy();


  /** @return A future to whether the stream is currently audio-only. */
  AsyncResults<bool> IsAudioOnly() const;

  /** @return A future to whether the Player is in a buffering state. */
  AsyncResults<bool> IsBuffering() const;

  /** @return A future to whether the stream is an in-progress recording. */
  AsyncResults<bool> IsInProgress() const;

  /** @return A future to whether the stream is live. */
  AsyncResults<bool> IsLive() const;

  /** @return A future to whether the text track is visible. */
  AsyncResults<bool> IsTextTrackVisible() const;

  /** @return A future to whether we are using an embedded text tracks. */
  AsyncResults<bool> UsingEmbeddedTextTrack() const;


  /** @return A future to the manifest URI given to load(), or null. */
  AsyncResults<optional<std::string>> AssetUri() const;

  /**
   * @return A future to the DrmInfo used to initialize EME. This returns null
   *   when not using EME.
   */
  AsyncResults<optional<DrmInfo>> DrmInfo() const;

  /**
   * @return A future to list of audio language-role combinations available for
   *   the current Period.
   */
  AsyncResults<std::vector<LanguageRole>> GetAudioLanguagesAndRoles() const;

  /** @return A future to the current buffered ranges. */
  AsyncResults<BufferedInfo> GetBufferedInfo() const;

  /**
   * @return A future to the next known expiration time of any EME sessions.
   *   This returns Infinity if there are no sessions or they never expire.
   */
  AsyncResults<double> GetExpiration() const;

  /** Return playback and adaptation stats. */
  AsyncResults<Stats> GetStats() const;

  /**
   * Return a list of text tracks available for the current Period. If there are
   * multiple Periods, then you must seek to the Period before being able to
   * switch.
   */
  AsyncResults<std::vector<Track>> GetTextTracks() const;

  /**
   * Return a list of variant tracks available for the current Period. If there
   * are multiple Periods, then you must seek to the Period before being able to
   * switch.
   */
  AsyncResults<std::vector<Track>> GetVariantTracks() const;

  /**
   * @return A future to list of text language-role combinations available for
   *   the current Period.
   */
  AsyncResults<std::vector<LanguageRole>> GetTextLanguagesAndRoles() const;

  /**
   * @return A future to the key system name being used by EME, or the empty
   *   string if not using EME.
   */
  AsyncResults<std::string> KeySystem() const;

  /** @return A future to the currently seekable range. */
  AsyncResults<BufferedRange> SeekRange() const;


  /**
   * Loads the given manifest.  Returns a future that will resolve when the
   * load is complete.
   *
   * @param manifest_uri The URI of the manifest to load.
   * @param start_time The time to start playing at, in seconds.
   * @param mime_type The MIME type of the content being loaded.  If the MIME
   *   type can't be detected based on the extension, we'll make a HEAD request
   *   to the URL to figure it out.
   */
  AsyncResults<void> Load(const std::string& manifest_uri,
                          double start_time = NAN,
                          const std::string& mime_type = "");

  /** Unload the current manifest and make the Player available for re-use. */
  AsyncResults<void> Unload();

  //@{
  /**
   * Sets a configuration value on the Player instance.  This is simply
   * forwarded to the JavaScript instance.  No error is returned if the
   * requested configuration isn't present or is an invalid type, see the logs
   * for errors.  The path is a '.' separated list of names to reach the
   * configuration.  For example:
   *
   * 'abr.enabled' => {abr: {enabled: value}}
   *
   * @return A future to whether the configuration path was valid.
   */
  AsyncResults<bool> Configure(const std::string& name_path, DefaultValueType);
  AsyncResults<bool> Configure(const std::string& name_path, bool value);
  AsyncResults<bool> Configure(const std::string& name_path, double value);
  AsyncResults<bool> Configure(const std::string& name_path,
                               const std::string& value);
  template <typename T, typename = typename std::enable_if<
                            std::is_arithmetic<T>::value>::type>
  AsyncResults<bool> Configure(const std::string& name_path, T value) {
    // Since there are both bool and double overloads, any other number type
    // will cause an ambiguous call.  This disambiguates the calls.
    return Configure(name_path, static_cast<double>(value));
  }
  AsyncResults<bool> Configure(const std::string& name_path,
                               const char* value) {
    // For some reason, the compiler tries to use the bool overload over the
    // std::string one when passing a char pointer.
    return Configure(name_path, std::string(value));
  }
  //@}


  /** Reset configuration to default. */
  AsyncResults<void> ResetConfiguration();

  /**
   * Retry streaming after a failure. Does nothing if not in a failure state.
   */
  AsyncResults<void> RetryStreaming();

  /**
   * Sets currentAudioLanguage and currentVariantRole to the selected language
   * and role, and chooses a new variant if need be.
   */
  AsyncResults<void> SelectAudioLanguage(const std::string& language,
                                         optional<std::string> role = nullopt);

  /**
   * Use the embedded text for the current stream, if present. CEA 608/708
   * captions data is embedded inside the video stream.
   */
  AsyncResults<void> SelectEmbeddedTextTrack();

  /**
   * Sets currentTextLanguage and currentTextRole to the selected language and
   * role, and chooses a new text stream if need be.
   */
  AsyncResults<void> SelectTextLanguage(const std::string& language,
                                        optional<std::string> role = nullopt);


  /**
   * Select a specific text track. Note that AdaptationEvents are not fired for
   * manual track selections.
   */
  AsyncResults<void> SelectTextTrack(const Track& track);

  /**
   * Select a specific track. Note that AdaptationEvents are not fired for
   * manual track selections.
   */
  AsyncResults<void> SelectVariantTrack(const Track& track,
                                        bool clear_buffer = false);

  /**
   * Sets whether the text track should be visible or not, if any exists.
   *
   * @param visibility True if the text track should be visible.
   */
  AsyncResults<void> SetTextTrackVisibility(bool visibility);


  //@{
  /**
   * Gets a configuration value from the Player instance.  This is simply
   * forwarded to the JavaScript instance.  No error is returned if the
   * requested configuration isn't present or is an invalid type, see the logs
   * for errors.  The path is a '.' separated list of names to reach the
   * configuration.  For example:
   *
   * 'abr.enabled' => {abr: {enabled: value}}
   *
   * TODO: These will be replaced with a more general GetConfiguration() method
   * in the future.
   *
   * @return A future to the value.
   */
  AsyncResults<bool> GetConfigurationBool(const std::string& name_path);
  AsyncResults<double> GetConfigurationDouble(const std::string& name_path);
  AsyncResults<std::string> GetConfigurationString(
      const std::string& name_path);
  //@}

  /**
   * Adds the given text track to the current Period.  <code>Load()</code> must
   * resolve before calling.  The current Period or the presentation must have a
   * duration.
   * This returns a Promise that will resolve with the track that was created,
   * when that track can be switched to.
   */
  AsyncResults<Track> AddTextTrack(const std::string& uri,
                                   const std::string& language,
                                   const std::string& kind,
                                   const std::string& mime,
                                   const std::string& codec = "",
                                   const std::string& label = "");

  /**
   * Tells the Player to use the given MediaPlayer instance for media handling.
   * Once the returned results resolve, the old MediaPlayer won't be used
   * anymore and this one will be used.  If we are currently playing content,
   * this will unload the content first.
   */
  AsyncResults<void> Attach(media::MediaPlayer* player);

  /**
   * Tells the Player to stop using the current MediaPlayer instance.  Once the
   * returned results resolve, the old MediaPlayer won't be used anymore.  If
   * we are currently playing content, this will unload the content first.
   */
  AsyncResults<void> Detach();

  /*
   * Adds an object that is called when network requests happen.  These are
   * called in the order they are registered.
   * @param filters The object that receives the calls.
   */
  void AddNetworkFilters(NetworkFilters* filters);

  /** Stops the given object from receiving calls for network requests. */
  void RemoveNetworkFilters(NetworkFilters* filters);

  //@{
  /**
   * Configures the player with the given data buffer.
   */
  AsyncResults<bool> Configure(const std::string& name_path,
                               const std::vector<uint8_t>& data) {
    return Configure(name_path, data.data(), data.size());
  }
  AsyncResults<bool> Configure(const std::string& name_path,
                               const uint8_t* data, size_t data_size);
  //@}

 private:
  friend class Storage;
  void* GetRawJsValue();

  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_PLAYER_H_
