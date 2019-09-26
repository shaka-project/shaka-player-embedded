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

#include "shaka/version.h"
#include "src/core/js_manager_impl.h"
#include "src/debug/thread_event.h"
#include "src/js/manifest.h"
#include "src/js/mse/video_element.h"
#include "src/js/player_externs.h"
#include "src/js/stats.h"
#include "src/js/track.h"
#include "src/mapping/any.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/struct.h"
#include "src/util/utils.h"

// Declared in version.h by generated code in //shaka/tools/version.py.
SHAKA_EXPORT extern "C" const char* GetShakaEmbeddedVersion() {
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

/**
 * A helper that converts a JavaScript value into the given return type as a
 * variant.  If the return type is |void|, this ignores the value.
 */
template <typename Ret>
struct Converter {
  using variant_type = typename AsyncResults<Ret>::variant_type;
  using future_type = std::shared_future<variant_type>;

  static variant_type Convert(const std::string& name, Handle<JsValue> result) {
    Ret ret;
    if (!FromJsValue(result, &ret)) {
      return Error("Invalid return value from " + name + "().");
    }

    return ret;
  }
};

template <>
struct Converter<void> {
  using variant_type = AsyncResults<void>::variant_type;
  using future_type = std::shared_future<variant_type>;

  static variant_type Convert(const std::string& /* name */,
                              Handle<JsValue> /* result */) {
    return monostate();
  }
};


Error ConvertError(Handle<JsValue> except) {
  if (!IsObject(except))
    return Error(ConvertToString(except));

  LocalVar<JsObject> obj = UnsafeJsCast<JsObject>(except);
  LocalVar<JsValue> message_member = GetMemberRaw(obj, "message");
  // Rather than checking for the type of the exception, assume that if it has
  // a 'name' field, then it is an exception.
  LocalVar<JsValue> name_member = GetMemberRaw(obj, "name");
  if (!IsNullOrUndefined(name_member)) {
    const std::string message =
        ConvertToString(name_member) + ": " + ConvertToString(message_member);
    return Error(message);
  }

  LocalVar<JsValue> code = GetMemberRaw(obj, "code");
  LocalVar<JsValue> category = GetMemberRaw(obj, "category");
  if (GetValueType(code) != proto::ValueType::Number ||
      GetValueType(category) != proto::ValueType::Number) {
    return Error(ConvertToString(except));
  }

  LocalVar<JsValue> severity_val = GetMemberRaw(obj, "severity");
  int severity = 0;
  if (GetValueType(severity_val) == proto::ValueType::Number)
    severity = static_cast<int>(NumberFromValue(severity_val));

  std::string message;
  if (IsNullOrUndefined(message_member)) {
    message = "Shaka Error, Category: " + ConvertToString(category) +
              ", Code: " + ConvertToString(code);
  } else {
    message = ConvertToString(message_member);
  }
  return Error(severity, static_cast<int>(NumberFromValue(category)),
               static_cast<int>(NumberFromValue(code)), message);
}

Converter<void>::variant_type CallMemberFunction(Handle<JsObject> that,
                                                 const std::string& name,
                                                 int argc,
                                                 LocalVar<JsValue>* argv,
                                                 LocalVar<JsValue>* result) {
  LocalVar<JsValue> member = GetMemberRaw(that, name);
  if (GetValueType(member) != proto::ValueType::Function) {
    return Error("The member '" + name + "' is not a function.");
  }

  LocalVar<JsValue> result_or_except;
  LocalVar<JsFunction> member_func = UnsafeJsCast<JsFunction>(member);
  if (!InvokeMethod(member_func, that, argc, argv, &result_or_except)) {
    return ConvertError(result_or_except);
  }

  if (result)
    *result = result_or_except;
  return monostate();
}

Converter<void>::variant_type AttachEventListener(
    Handle<JsObject> player, const std::string& name, Player::Client* client,
    std::function<void(Handle<JsObject> event)> handler) {
  const std::function<void(optional<Any>)> callback = [=](optional<Any> event) {
    // We can't accept or use events::Event since Shaka player raises fake
    // events.  So manually look for the properties.
    LocalVar<JsValue> event_val = ToJsValue(event);
    if (!IsObject(event_val)) {
      client->OnError(Error(ConvertToString(event_val)));
      return;
    }

    LocalVar<JsObject> event_obj = UnsafeJsCast<JsObject>(event_val);
    handler(event_obj);
  };
  LocalVar<JsFunction> callback_js = CreateStaticFunction("", "", callback);
  LocalVar<JsValue> arguments[] = {ToJsValue(name), RawToJsValue(callback_js)};
  return CallMemberFunction(player, "addEventListener", 2, arguments, nullptr);
}

}  // namespace

// Allow using shaka::ToJsValue() (from convert_js.h) for DefaultValue.
template <>
struct impl::ConvertHelper<DefaultValueType> {
  static ReturnVal<JsValue> ToJsValue(DefaultValueType /* unused */) {
    return JsUndefined();
  }
};


class Player::Impl {
 public:
  explicit Impl(JsManager* engine) {
    CHECK(engine) << "Must pass a JsManager instance";
  }
  ~Impl() {
    if (player_)
      CallPlayerPromiseMethod<void>("destroy").wait();
  }

  NON_COPYABLE_OR_MOVABLE_TYPE(Impl);

  Converter<void>::future_type Initialize(js::mse::HTMLVideoElement* video,
                                          Client* client) {
    // This function can be called immediately after the JsManager
    // constructor.  Since the Environment might not be setup yet, run this in
    // an internal task so we know it is ready.
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
      LocalVar<JsValue> args[] = {video->JsThis()};
      if (!InvokeConstructor(player_ctor_func, 1, args, &result_or_except)) {
        return ConvertError(result_or_except);
      }

      player_ = UnsafeJsCast<JsObject>(result_or_except);
      return AttachListeners(player_, client);
    };
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Player ctor",
                          PlainCallbackTask(callback))
        ->future();
  }

  /** Calls the given Player method and returns the result as a Ret type. */
  template <typename Ret, typename... Args>
  typename Converter<Ret>::future_type CallPlayerMethod(
      const std::string& name, const Args&... args) const {
    const auto callback = [=]() -> typename Converter<Ret>::variant_type {
      LocalVar<JsValue> result;
      LocalVar<JsValue> js_args[] = {ToJsValue(args)..., JsUndefined()};
      auto error =
          CallMemberFunction(player_, name, sizeof...(args), js_args, &result);
      if (holds_alternative<Error>(error))
        return get<Error>(error);

      return Converter<Ret>::Convert(name, result);
    };
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Player." + name,
                          PlainCallbackTask(callback))
        ->future();
  }

  /**
   * Calls the given Player method that should return a Promise to the given
   * type.  The resulting async results will complete when the Promise is
   * resolved/rejected.
   */
  template <typename Ret, typename... Args>
  typename Converter<Ret>::future_type CallPlayerPromiseMethod(
      const std::string& name, Args&&... args) {
    // TODO: Combine with CallPlayerMethod.  These are separate since this uses
    // a std::promise for the return value rather than the internal task future.
    auto promise =
        std::make_shared<std::promise<typename Converter<Ret>::variant_type>>();
    const auto callback = [this, promise, name, args...]() {
      LocalVar<JsValue> result;
      LocalVar<JsValue> js_args[] = {ToJsValue(args)..., JsUndefined()};
      auto error =
          CallMemberFunction(player_, name, sizeof...(args), js_args, &result);
      if (holds_alternative<Error>(error)) {
        promise->set_value(get<Error>(error));
        return;
      }

      auto js_promise = Converter<Promise>::Convert(name, result);
      if (holds_alternative<Error>(js_promise)) {
        promise->set_value(get<Error>(js_promise));
        return;
      }

      get<Promise>(js_promise)
          .Then(
              [promise, name](Any res) {
                LocalVar<JsValue> value = res.ToJsValue();
                promise->set_value(Converter<Ret>::Convert(name, value));
              },
              [promise](Any except) {
                LocalVar<JsValue> val = except.ToJsValue();
                promise->set_value(ConvertError(val));
              });
    };
    JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Player." + name,
                          PlainCallbackTask(callback))
        ->future();
    return promise->get_future().share();
  }

  template <typename T>
  typename Converter<T>::future_type GetConfigValue(
      const std::string& name_path) {
    const auto callback = [=]() -> typename Converter<T>::variant_type {
      LocalVar<JsValue> configuration;
      auto error = CallMemberFunction(player_, "getConfiguration", 0, nullptr,
                                      &configuration);
      if (holds_alternative<Error>(error))
        return get<Error>(error);

      // Split the name path on periods.
      std::vector<std::string> components = util::StringSplit(name_path, '.');

      // Navigate through the path.
      auto result =
          GetDescendant(UnsafeJsCast<JsObject>(configuration), components);

      return Converter<T>::Convert(name_path, result);
    };
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Player.getConfiguration",
                          PlainCallbackTask(callback))
        ->future();
  }

 private:
  Converter<void>::variant_type AttachListeners(Handle<JsObject> player,
                                                Client* client) {
#define ATTACH(name, call)                                            \
  do {                                                                \
    const auto ret = AttachEventListener(player, name, client, call); \
    if (holds_alternative<Error>(ret))                                \
      return get<Error>(ret);                                         \
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

  Global<JsObject> player_;
};

Player::Client::Client() {}
Player::Client::Client(const Client&) = default;
Player::Client::Client(Client&&) = default;
Player::Client::~Client() {}

Player::Client& Player::Client::operator=(const Client&) = default;
Player::Client& Player::Client::operator=(Client&&) = default;

void Player::Client::OnError(const Error& /* error */) {}
void Player::Client::OnBuffering(bool /* is_buffering */) {}


Player::Player(JsManager* engine) : impl_(new Impl(engine)) {}

Player::Player(Player&&) = default;

Player::~Player() {}

Player& Player::operator=(Player&&) = default;

AsyncResults<void> Player::SetLogLevel(JsManager* engine, LogLevel level) {
  DCHECK(engine);
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  const auto callback = [level]() -> Converter<void>::variant_type {
    LocalVar<JsValue> set_level = GetDescendant(
        JsEngine::Instance()->global_handle(), {"shaka", "log", "setLevel"});
    if (GetValueType(set_level) != proto::ValueType::Function) {
      LOG(ERROR) << "Cannot get 'shaka.log.setLevel' function; is "
                    "shaka-player.compiled.js a Release build?";
      return Error("The function 'shaka.log.setLevel' is not found.");
    }
    LocalVar<JsFunction> set_level_func = UnsafeJsCast<JsFunction>(set_level);

    LocalVar<JsValue> args[] = {ToJsValue(static_cast<int>(level))};
    LocalVar<JsValue> except;
    if (!InvokeMethod(set_level_func, JsEngine::Instance()->global_handle(), 1,
                      args, &except)) {
      return ConvertError(except);
    }

    return monostate();
  };
  return JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, "SetLogLevel",
                        PlainCallbackTask(callback))
      ->future();
}

AsyncResults<Player::LogLevel> Player::GetLogLevel(JsManager* engine) {
  DCHECK(engine);
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  const auto callback = []() -> Converter<Player::LogLevel>::variant_type {
    LocalVar<JsValue> current_level =
        GetDescendant(JsEngine::Instance()->global_handle(),
                      {"shaka", "log", "currentLevel"});
    if (GetValueType(current_level) != proto::ValueType::Number) {
      LOG(ERROR) << "Cannot get 'shaka.log.currentLevel'; is "
                    "shaka-player.compiled.js a Release build?";
      return Error("The variable 'shaka.log.currentLevel' is not found.");
    }
    return static_cast<LogLevel>(NumberFromValue(current_level));
  };
  return JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, "GetLogLevel",
                        PlainCallbackTask(callback))
      ->future();
}

AsyncResults<std::string> Player::GetPlayerVersion(JsManager* engine) {
  DCHECK(engine);
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  const auto callback = []() -> Converter<std::string>::variant_type {
    LocalVar<JsValue> version = GetDescendant(
        JsEngine::Instance()->global_handle(), {"shaka", "Player", "version"});
    std::string ret;
    if (!FromJsValue(version, &ret)) {
      return Error("The variable 'shaka.Player.version' is not found.");
    }
    return ret;
  };
  return JsManagerImpl::Instance()
      ->MainThread()
      ->AddInternalTask(TaskPriority::Internal, "GetVersion",
                        PlainCallbackTask(callback))
      ->future();
}


AsyncResults<void> Player::Initialize(Video* video, Client* client) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->Initialize(video->GetJavaScriptObject(), client);
}

AsyncResults<void> Player::Destroy() {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerPromiseMethod<void>("destroy");
}


AsyncResults<bool> Player::IsAudioOnly() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("isAudioOnly");
}

AsyncResults<bool> Player::IsBuffering() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("isBuffering");
}

AsyncResults<bool> Player::IsInProgress() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("isInProgress");
}

AsyncResults<bool> Player::IsLive() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("isLive");
}

AsyncResults<bool> Player::IsTextTrackVisible() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("isTextTrackVisible");
}

AsyncResults<bool> Player::UsingEmbeddedTextTrack() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("usingEmbeddedTextTrack");
}


AsyncResults<optional<std::string>> Player::AssetUri() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<optional<std::string>>("assetUri");
}

AsyncResults<optional<DrmInfo>> Player::DrmInfo() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<optional<shaka::DrmInfo>>("drmInfo");
}

AsyncResults<std::vector<LanguageRole>> Player::GetAudioLanguagesAndRoles()
    const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<std::vector<LanguageRole>>(
      "getAudioLanguagesAndRoles");
}

AsyncResults<BufferedInfo> Player::GetBufferedInfo() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<BufferedInfo>("getBufferedInfo");
}

AsyncResults<double> Player::GetExpiration() const {
  return impl_->CallPlayerMethod<double>("getExpiration");
}

AsyncResults<Stats> Player::GetStats() const {
  return impl_->CallPlayerMethod<Stats>("getStats");
}

AsyncResults<std::vector<Track>> Player::GetTextTracks() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<std::vector<Track>>("getTextTracks");
}

AsyncResults<std::vector<Track>> Player::GetVariantTracks() const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<std::vector<Track>>("getVariantTracks");
}

AsyncResults<std::vector<LanguageRole>> Player::GetTextLanguagesAndRoles()
    const {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<std::vector<LanguageRole>>(
      "getTextLanguagesAndRoles");
}

AsyncResults<std::string> Player::KeySystem() const {
  return impl_->CallPlayerMethod<std::string>("keySystem");
}

AsyncResults<BufferedRange> Player::SeekRange() const {
  return impl_->CallPlayerMethod<BufferedRange>("seekRange");
}


AsyncResults<void> Player::Load(const std::string& manifest_uri,
                                double start_time) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerPromiseMethod<void>("load", manifest_uri,
                                              LoadHelper(start_time));
}

AsyncResults<void> Player::Unload() {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerPromiseMethod<void>("unload");
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     DefaultValueType value) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::Configure(const std::string& name_path, bool value) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     double value) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::Configure(const std::string& name_path,
                                     const std::string& value) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Player::GetConfigurationBool(const std::string& name_path) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->GetConfigValue<bool>(name_path);
}

AsyncResults<double> Player::GetConfigurationDouble(
    const std::string& name_path) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->GetConfigValue<double>(name_path);
}

AsyncResults<std::string> Player::GetConfigurationString(
    const std::string& name_path) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->GetConfigValue<std::string>(name_path);
}


AsyncResults<void> Player::ResetConfiguration() {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("resetConfiguration");
}

AsyncResults<void> Player::RetryStreaming() {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("retryStreaming");
}

AsyncResults<void> Player::SelectAudioLanguage(const std::string& language,
                                               optional<std::string> role) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("selectAudioLanguage", language, role);
}

AsyncResults<void> Player::SelectEmbeddedTextTrack() {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("selectEmbeddedTextTrack");
}

AsyncResults<void> Player::SelectTextLanguage(const std::string& language,
                                              optional<std::string> role) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("selectTextLanguage", language, role);
}

AsyncResults<void> Player::SelectTextTrack(const Track& track) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("selectTextTrack", track.GetInternal());
}

AsyncResults<void> Player::SelectVariantTrack(const Track& track,
                                              bool clear_buffer) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("selectVariantTrack",
                                       track.GetInternal(), clear_buffer);
}

AsyncResults<void> Player::SetTextTrackVisibility(bool visibility) {
  DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
  return impl_->CallPlayerMethod<void>("setTextTrackVisibility", visibility);
}

}  // namespace shaka
