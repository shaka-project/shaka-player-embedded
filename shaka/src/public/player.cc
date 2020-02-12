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

#include "shaka/player.h"

#include <functional>

#include "shaka/version.h"
#include "src/core/js_manager_impl.h"
#include "src/core/js_object_wrapper.h"
#include "src/js/dom/document.h"
#include "src/js/manifest.h"
#include "src/js/mse/video_element.h"
#include "src/js/player_externs.h"
#include "src/js/stats.h"
#include "src/js/track.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/js_wrappers.h"
#include "src/util/macros.h"
#include "src/util/utils.h"

// Declared in version.h by generated code in //shaka/tools/version.py.
extern "C" const char* GetShakaEmbeddedVersion(void) {
  return SHAKA_VERSION_STR;
}

namespace shaka {

namespace {

/**
 * A helper class that converts a number to the argument to load().  This
 * exists because we need to convert the C++ NaN into a JavaScript undefined.
 * This class allows the code below to be more general and avoids having to
 * have a special case for converting the argument for load().
 */
class LoadHelper : public GenericConverter {
 public:
  explicit LoadHelper(double value) : value_(value) {}

  bool TryConvert(Handle<JsValue> /* value */) override {
    LOG(FATAL) << "Not reached";
  }

  ReturnVal<JsValue> ToJsValue() const override {
    return !isnan(value_) ? ::shaka::ToJsValue(value_) : JsUndefined();
  }

 private:
  const double value_;
};

}  // namespace

template <>
struct impl::ConvertHelper<Player::LogLevel> {
  static bool FromJsValue(Handle<JsValue> source, Player::LogLevel* level) {
    int value;
    if (!shaka::FromJsValue(source, &value))
      return false;
    *level = static_cast<Player::LogLevel>(value);
    return true;
  }
};


class Player::Impl : public JsObjectWrapper {
 public:
  explicit Impl(JsManager* engine) {
    CHECK(engine) << "Must pass a JsManager instance";
  }
  ~Impl() {
    if (object_)
      CallMethod<void>("destroy").wait();
    if (video_)
      video_->Detach();
  }

  SHAKA_NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  Converter<void>::future_type Initialize(Client* client,
                                          media::MediaPlayer* player) {
    // This function can be called immediately after the JsManager
    // constructor.  Since the Environment might not be setup yet, run this in
    // an internal task so we know it is ready.
    DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
    const auto callback = [=]() -> Converter<void>::variant_type {
      LocalVar<JsValue> player_ctor = GetDescendant(
          JsEngine::Instance()->global_handle(), {"shaka", "Player"});
      if (GetValueType(player_ctor) != proto::ValueType::Function) {
        LOG(ERROR) << "Cannot get 'shaka.Player' object; is "
                      "shaka-player.compiled.js corrupted?";
        return Error("The constructor 'shaka.Player' is not found.");
      }
      LocalVar<JsFunction> player_ctor_func =
          UnsafeJsCast<JsFunction>(player_ctor);

      LocalVar<JsValue> result_or_except;
      std::vector<LocalVar<JsValue>> args;
      if (player) {
        video_ = new js::mse::HTMLVideoElement(
            js::dom::Document::EnsureGlobalDocument(), player);
        args.emplace_back(video_->JsThis());
      }
      if (!InvokeConstructor(player_ctor_func, args.size(), args.data(),
                             &result_or_except)) {
        return ConvertError(result_or_except);
      }

      Init(UnsafeJsCast<JsObject>(result_or_except));
      return AttachListeners(client);
    };
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Player ctor",
                          PlainCallbackTask(callback))
        ->future();
  }

  template <typename T>
  typename Converter<T>::future_type GetConfigValue(
      const std::string& name_path) {
    auto callback = std::bind(&Impl::GetConfigValueRaw<T>, this, name_path);
    return JsManagerImpl::Instance()->MainThread()->InvokeOrSchedule(
        PlainCallbackTask(std::move(callback)));
  }

  void* GetRawJsValue() {
    return &object_;
  }

 private:
  template <typename T>
  typename Converter<T>::variant_type GetConfigValueRaw(
      const std::string& name_path) {
    DCHECK(JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
    LocalVar<JsValue> configuration;
    auto error = CallMemberFunction(object_, "getConfiguration", 0, nullptr,
                                    &configuration);
    if (holds_alternative<Error>(error))
      return get<Error>(error);

    // Split the name path on periods.
    std::vector<std::string> components = util::StringSplit(name_path, '.');

    // Navigate through the path.
    auto result =
        GetDescendant(UnsafeJsCast<JsObject>(configuration), components);

    return Converter<T>::Convert(name_path, result);
  }

  Converter<void>::variant_type AttachListeners(Client* client) {
#define ATTACH(name, call)                                                \
  do {                                                                    \
    const auto ret = AttachEventListener(                                 \
        name, std::bind(&Client::OnError, client, std::placeholders::_1), \
        call);                                                            \
    if (holds_alternative<Error>(ret))                                    \
      return get<Error>(ret);                                             \
  } while (false)

    const auto on_error = [=](Handle<JsObject> event) {
      LocalVar<JsValue> detail = GetMemberRaw(event, "detail");
      client->OnError(ConvertError(detail));
    };
    ATTACH("error", on_error);

    const auto on_buffering = [=](Handle<JsObject> event) {
      LocalVar<JsValue> is_buffering = GetMemberRaw(event, "buffering");
      bool is_buffering_bool;
      if (FromJsValue(is_buffering, &is_buffering_bool)) {
        client->OnBuffering(is_buffering_bool);
      } else {
        client->OnError(Error("Bad 'buffering' event from JavaScript Player"));
      }
    };
    ATTACH("buffering", on_buffering);

#undef ATTACH
    return {};
  }

 private:
  RefPtr<js::mse::HTMLVideoElement> video_;
};

// \cond Doxygen_Skip
Player::Client::Client() {}
Player::Client::~Client() {}
// \endcond Doxygen_Skip

void Player::Client::OnError(const Error& /* error */) {}
void Player::Client::OnBuffering(bool /* is_buffering */) {}


Player::Player(JsManager* engine) : impl_(new Impl(engine)) {}

Player::Player(Player&&) = default;

Player::~Player() {}

Player& Player::operator=(Player&&) = default;

AsyncResults<void> Player::SetLogLevel(JsManager* engine, LogLevel level) {
  DCHECK(engine);
  return Impl::CallGlobalMethod<void>({"shaka", "log", "setLevel"},
                                      static_cast<int>(level));
}

AsyncResults<Player::LogLevel> Player::GetLogLevel(JsManager* engine) {
  DCHECK(engine);
  return Impl::GetGlobalField<Player::LogLevel>(
      {"shaka", "log", "currentLevel"});
}

AsyncResults<std::string> Player::GetPlayerVersion(JsManager* engine) {
  DCHECK(engine);
  return Impl::GetGlobalField<std::string>({"shaka", "Player", "version"});
}


AsyncResults<void> Player::Initialize(Client* client,
                                      media::MediaPlayer* player) {
  return impl_->Initialize(client, player);
}

AsyncResults<void> Player::Destroy() {
  return impl_->CallMethod<void>("destroy");
}


AsyncResults<bool> Player::IsAudioOnly() const {
  return impl_->CallMethod<bool>("isAudioOnly");
}

AsyncResults<bool> Player::IsBuffering() const {
  return impl_->CallMethod<bool>("isBuffering");
}

AsyncResults<bool> Player::IsInProgress() const {
  return impl_->CallMethod<bool>("isInProgress");
}

AsyncResults<bool> Player::IsLive() const {
  return impl_->CallMethod<bool>("isLive");
}

AsyncResults<bool> Player::IsTextTrackVisible() const {
  return impl_->CallMethod<bool>("isTextTrackVisible");
}

AsyncResults<bool> Player::UsingEmbeddedTextTrack() const {
  return impl_->CallMethod<bool>("usingEmbeddedTextTrack");
}


AsyncResults<optional<std::string>> Player::AssetUri() const {
  return impl_->CallMethod<optional<std::string>>("assetUri");
}

AsyncResults<optional<DrmInfo>> Player::DrmInfo() const {
  return impl_->CallMethod<optional<shaka::DrmInfo>>("drmInfo");
}

AsyncResults<std::vector<LanguageRole>> Player::GetAudioLanguagesAndRoles()
    const {
  return impl_->CallMethod<std::vector<LanguageRole>>(
      "getAudioLanguagesAndRoles");
}

AsyncResults<BufferedInfo> Player::GetBufferedInfo() const {
  return impl_->CallMethod<BufferedInfo>("getBufferedInfo");
}

AsyncResults<double> Player::GetExpiration() const {
  return impl_->CallMethod<double>("getExpiration");
}

AsyncResults<Stats> Player::GetStats() const {
  return impl_->CallMethod<Stats>("getStats");
}

AsyncResults<std::vector<Track>> Player::GetTextTracks() const {
  return impl_->CallMethod<std::vector<Track>>("getTextTracks");
}

AsyncResults<std::vector<Track>> Player::GetVariantTracks() const {
  return impl_->CallMethod<std::vector<Track>>("getVariantTracks");
}

AsyncResults<std::vector<LanguageRole>> Player::GetTextLanguagesAndRoles()
    const {
  return impl_->CallMethod<std::vector<LanguageRole>>(
      "getTextLanguagesAndRoles");
}

AsyncResults<std::string> Player::KeySystem() const {
  return impl_->CallMethod<std::string>("keySystem");
}

AsyncResults<BufferedRange> Player::SeekRange() const {
  return impl_->CallMethod<BufferedRange>("seekRange");
}


AsyncResults<void> Player::Load(const std::string& manifest_uri,
                                double start_time) {
  return impl_->CallMethod<void>("load", manifest_uri, LoadHelper(start_time));
}

AsyncResults<void> Player::Unload() {
  return impl_->CallMethod<void>("unload");
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     DefaultValueType /* value */) {
  return impl_->CallMethod<bool>("configure", name_path, Any());  // undefined
}

AsyncResults<bool> Player::Configure(const std::string& name_path, bool value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     double value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     const std::string& value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::GetConfigurationBool(const std::string& name_path) {
  return impl_->GetConfigValue<bool>(name_path);
}

AsyncResults<double> Player::GetConfigurationDouble(
    const std::string& name_path) {
  return impl_->GetConfigValue<double>(name_path);
}

AsyncResults<std::string> Player::GetConfigurationString(
    const std::string& name_path) {
  return impl_->GetConfigValue<std::string>(name_path);
}


AsyncResults<void> Player::ResetConfiguration() {
  return impl_->CallMethod<void>("resetConfiguration");
}

AsyncResults<void> Player::RetryStreaming() {
  return impl_->CallMethod<void>("retryStreaming");
}

AsyncResults<void> Player::SelectAudioLanguage(const std::string& language,
                                               optional<std::string> role) {
  return impl_->CallMethod<void>("selectAudioLanguage", language, role);
}

AsyncResults<void> Player::SelectEmbeddedTextTrack() {
  return impl_->CallMethod<void>("selectEmbeddedTextTrack");
}

AsyncResults<void> Player::SelectTextLanguage(const std::string& language,
                                              optional<std::string> role) {
  return impl_->CallMethod<void>("selectTextLanguage", language, role);
}

AsyncResults<void> Player::SelectTextTrack(const Track& track) {
  return impl_->CallMethod<void>("selectTextTrack", track.GetInternal());
}

AsyncResults<void> Player::SelectVariantTrack(const Track& track,
                                              bool clear_buffer) {
  return impl_->CallMethod<void>("selectVariantTrack", track.GetInternal(),
                                 clear_buffer);
}

AsyncResults<void> Player::SetTextTrackVisibility(bool visibility) {
  return impl_->CallMethod<void>("setTextTrackVisibility", visibility);
}

AsyncResults<Track> Player::AddTextTrack(const std::string& uri,
                                         const std::string& language,
                                         const std::string& kind,
                                         const std::string& mime,
                                         const std::string& codec,
                                         const std::string& label) {
  return impl_->CallMethod<Track>("addTextTrack", uri, language, kind, mime,
                                  codec, label);
}

void* Player::GetRawJsValue() {
  return impl_->GetRawJsValue();
}

}  // namespace shaka
