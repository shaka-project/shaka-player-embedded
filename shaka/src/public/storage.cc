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

#include "shaka/storage.h"

#include "src/core/js_object_wrapper.h"
#include "src/js/offline_externs.h"
#include "src/mapping/any.h"
#include "src/mapping/names.h"
#include "src/mapping/register_member.h"

namespace shaka {

template <>
struct TypeName<StoredContent, void> {
  static std::string name() {
    return "StoredContent";
  }
};


// \cond Doxygen_Skip
Storage::Client::Client() {}
Storage::Client::~Client() {}
// \endcond Doxygen_Skip

void Storage::Client::OnProgress(StoredContent /* content */,
                                 double /* progress */) {}


class Storage::Impl : public JsObjectWrapper {
 public:
  Impl(JsManager* engine, const Global<JsObject>* player) : player_(player) {
    CHECK(engine) << "Must pass a JsManager instance";
  }

  Converter<void>::future_type Initialize(Client* client) {
    // This function can be called immediately after the JsManager
    // constructor.  Since the Environment might not be setup yet, run this in
    // an internal task so we know it is ready.
    DCHECK(!JsManagerImpl::Instance()->MainThread()->BelongsToCurrentThread());
    const auto callback = [=]() -> Converter<void>::variant_type {
      LocalVar<JsValue> ctor =
          GetDescendant(JsEngine::Instance()->global_handle(),
                        {"shaka", "offline", "Storage"});
      if (GetValueType(ctor) != proto::ValueType::Function) {
        LOG(ERROR) << "Cannot get 'shaka.offline.Storage' object; is "
                      "shaka-player.compiled.js corrupted?";
        return Error("The constructor 'shaka.offline.Storage' is not found.");
      }
      LocalVar<JsFunction> ctor_func = UnsafeJsCast<JsFunction>(ctor);

      LocalVar<JsValue> result_or_except;
      std::vector<LocalVar<JsValue>> args;
      if (player_) {
        LocalVar<JsObject> player(*player_);
        args.emplace_back(RawToJsValue(player));
      }
      if (!InvokeConstructor(ctor_func, args.size(), args.data(),
                             &result_or_except)) {
        return ConvertError(result_or_except);
      }

      Init(UnsafeJsCast<JsObject>(result_or_except));

      if (client) {
        std::function<void(StoredContent, double)> cb =
            std::bind(&Client::OnProgress, client, std::placeholders::_1,
                      std::placeholders::_2);
        LocalVar<JsValue> args[] = {
            ToJsValue(std::string{"offline.progressCallback"}),
            CreateStaticFunction("Storage.Client", "OnProgress", cb),
        };
        LocalVar<JsValue> ignored;
        auto except =
            CallMemberFunction(object_, "configure", 2, args, &ignored);
        if (holds_alternative<Error>(except))
          return except;
      }

      return monostate();
    };
    return JsManagerImpl::Instance()
        ->MainThread()
        ->AddInternalTask(TaskPriority::Internal, "Storage ctor",
                          PlainCallbackTask(callback))
        ->future();
  }

 private:
  const Global<JsObject>* player_;
};

Storage::Storage(JsManager* engine, Player* player)
    : impl_(new Impl(engine, reinterpret_cast<const Global<JsObject>*>(
                                 player ? player->GetRawJsValue() : nullptr))) {
}
Storage::Storage(Storage&&) = default;
Storage::~Storage() {}

Storage& Storage::operator=(Storage&&) = default;

AsyncResults<bool> Storage::Support(JsManager* engine) {
  DCHECK(engine);
  return Impl::CallGlobalMethod<bool>(
      {"shaka", "offline", "Storage", "support"});
}

AsyncResults<void> Storage::DeleteAll(JsManager* engine) {
  DCHECK(engine);
  return Impl::CallGlobalMethod<void>(
      {"shaka", "offline", "Storage", "deleteAll"});
}

AsyncResults<void> Storage::Initialize(Client* client) {
  return impl_->Initialize(client);
}

AsyncResults<void> Storage::Destroy() {
  return impl_->CallMethod<void>("destroy");
}

AsyncResults<bool> Storage::GetStoreInProgress() {
  return impl_->CallMethod<bool>("getStoreInProgress");
}

AsyncResults<bool> Storage::Configure(const std::string& name_path,
                                      DefaultValueType) {
  return impl_->CallMethod<bool>("configure", name_path, Any());  // undefined
}

AsyncResults<bool> Storage::Configure(const std::string& name_path,
                                      bool value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Storage::Configure(const std::string& name_path,
                                      double value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<bool> Storage::Configure(const std::string& name_path,
                                      const std::string& value) {
  return impl_->CallMethod<bool>("configure", name_path, value);
}

AsyncResults<std::vector<StoredContent>> Storage::List() {
  return impl_->CallMethod<std::vector<StoredContent>>("list");
}

AsyncResults<void> Storage::Remove(const std::string& content_uri) {
  return impl_->CallMethod<void>("remove", content_uri);
}

AsyncResults<bool> Storage::RemoveEmeSessions() {
  return impl_->CallMethod<bool>("removeEmeSessions");
}

AsyncResults<StoredContent> Storage::Store(const std::string& uri) {
  return impl_->CallMethod<StoredContent>("store", uri);
}

AsyncResults<StoredContent> Storage::Store(
    const std::string& uri,
    const std::unordered_map<std::string, std::string>& app_metadata) {
  return impl_->CallMethod<StoredContent>("store", uri, app_metadata);
}

}  // namespace shaka
