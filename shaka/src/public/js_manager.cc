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

#include "shaka/js_manager.h"

#include "shaka/error.h"
#include "src/core/js_manager_impl.h"
#include "src/core/js_object_wrapper.h"
#include "src/js/js_error.h"
#include "src/js/net.h"
#include "src/mapping/callback.h"
#include "src/mapping/convert_js.h"
#include "src/mapping/js_utils.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/promise.h"
#include "src/mapping/register_member.h"

namespace shaka {

namespace {

class ProgressClient : public SchemePlugin::Client {
 public:
  explicit ProgressClient(Callback on_progress)
      : on_progress_(MakeJsRef<Callback>(std::move(on_progress))) {}

  void OnProgress(double time, uint64_t bytes, uint64_t remaining) override {
    RefPtr<Callback> progress = on_progress_;
    auto cb = [=]() { (*progress)(time, bytes, remaining); };
    JsManagerImpl::Instance()->MainThread()->InvokeOrSchedule(
        PlainCallbackTask(std::move(cb)));
  }

 private:
  RefPtr<Callback> on_progress_;
};

}  // namespace

JsManager::JsManager() : impl_(new JsManagerImpl(StartupOptions())) {}
JsManager::JsManager(const StartupOptions& options)
    : impl_(new JsManagerImpl(options)) {}
JsManager::~JsManager() {}

JsManager::JsManager(JsManager&&) = default;
JsManager& JsManager::operator=(JsManager&&) = default;


void JsManager::Stop() {
  impl_->Stop();
}

void JsManager::WaitUntilFinished() {
  impl_->WaitUntilFinished();
}

AsyncResults<void> JsManager::RunScript(const std::string& path) {
  auto run_future = impl_->RunScript(path)->future();
  // This creates a std::future that will invoke the given method when the
  // wait()/get() method is called.  This exists to convert the
  // std::future<bool> that is returned into a
  // std::future<variant<monostate, Error>> that AsyncResults expects.
  // TODO: Find a better way to do this.
  auto future = std::async(std::launch ::deferred,
                           [run_future]() -> variant<monostate, Error> {
                             if (run_future.get())
                               return monostate();

                             return Error("Unknown script error");
                           });

  return future.share();
}

AsyncResults<void> JsManager::RegisterNetworkScheme(const std::string& scheme,
                                                    SchemePlugin* plugin) {
  auto js_scheme_plugin = [=](const std::string& uri, js::Request request,
                              RequestType type, Callback on_progress) {
    std::shared_ptr<Request> pub_req(new Request(std::move(request)));
    std::shared_ptr<Response> resp(new Response);
    std::shared_ptr<SchemePlugin::Client> client(
        new ProgressClient(std::move(on_progress)));
    Promise ret;
    auto future =
        plugin->OnNetworkRequest(uri, type, *pub_req, client.get(), resp.get());
    auto on_done = [pub_req, resp, client, ret]() {
      Promise copy = ret;  // By-value captures are const, so make a copy.
      copy.ResolveWith(resp->JsObject()->ToJsValue(),
                       /* raise_events= */ false);
    };
    HandleNetworkFuture(ret, std::move(future), std::move(on_done));
    return ret;
  };

  return JsObjectWrapper::CallGlobalMethod<void>(
      {"shaka", "net", "NetworkingEngine", "registerScheme"}, scheme,
      std::move(js_scheme_plugin));
}

AsyncResults<void> JsManager::UnregisterNetworkScheme(
    const std::string& scheme) {
  return JsObjectWrapper::CallGlobalMethod<void>(
      {"shaka", "net", "NetworkingEngine", "unregisterScheme"}, scheme);
}

}  // namespace shaka
