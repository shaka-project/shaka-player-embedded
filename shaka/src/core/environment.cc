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

#include "src/core/environment.h"

#include <glog/logging.h>

#include <atomic>
#include <string>

#include "shaka/eme/implementation_registry.h"
#include "src/core/js_manager_impl.h"
#include "src/js/base_64.h"
#include "src/js/console.h"
#include "src/js/debug.h"
#include "src/js/dom/attr.h"
#include "src/js/dom/character_data.h"
#include "src/js/dom/comment.h"
#include "src/js/dom/container_node.h"
#include "src/js/dom/document.h"
#include "src/js/dom/dom_exception.h"
#include "src/js/dom/dom_parser.h"
#include "src/js/dom/dom_string_list.h"
#include "src/js/dom/element.h"
#include "src/js/dom/node.h"
#include "src/js/dom/text.h"
#include "src/js/eme/media_key_session.h"
#include "src/js/eme/media_key_system_access.h"
#include "src/js/eme/media_keys.h"
#include "src/js/events/event.h"
#include "src/js/events/event_target.h"
#include "src/js/events/media_encrypted_event.h"
#include "src/js/events/media_key_message_event.h"
#include "src/js/events/progress_event.h"
#include "src/js/events/version_change_event.h"
#include "src/js/idb/cursor.h"
#include "src/js/idb/database.h"
#include "src/js/idb/idb_factory.h"
#include "src/js/idb/object_store.h"
#include "src/js/idb/open_db_request.h"
#include "src/js/idb/request.h"
#include "src/js/idb/transaction.h"
#include "src/js/location.h"
#include "src/js/mse/media_error.h"
#include "src/js/mse/media_source.h"
#include "src/js/mse/source_buffer.h"
#include "src/js/mse/text_track.h"
#include "src/js/mse/time_ranges.h"
#include "src/js/mse/video_element.h"
#include "src/js/navigator.h"
#include "src/js/test_type.h"
#include "src/js/timeouts.h"
#include "src/js/url.h"
#include "src/js/vtt_cue.h"
#include "src/js/xml_http_request.h"
#include "src/mapping/js_engine.h"
#include "src/mapping/js_wrappers.h"
#include "src/mapping/register_member.h"

namespace shaka {

// Defined in generated code by //shaka/tools/gen_eme_plugins.py
void RegisterDefaultKeySystems();

namespace {

void DummyMethod(const CallbackArguments& /* unused */) {}

template <typename T, typename Base>
void CreateInstance(const std::string& name,
                    BackingObjectFactory<T, Base>* factory) {
  LocalVar<JsValue> value(factory->WrapInstance(new T));
  SetMemberRaw(JsEngine::Instance()->global_handle(), name, value);
}

#if defined(USING_JSC) && !defined(NDEBUG)
void GC() {
  // A global JavaScript method that runs the garbage collector.  V8 defines its
  // own.
  JSGarbageCollect(JsEngine::Instance()->context());
}
#endif

}  // namespace

struct Environment::Impl {
  // NOTE: Any base types MUST appear above the derived types.

  js::events::EventTargetFactory event_target;

#ifndef NDEBUG
  js::DebugFactory debug;
  js::TestTypeFactory test_type;
#endif

  js::ConsoleFactory console;
  js::LocationFactory location;
  js::NavigatorFactory navigator;
  js::URLFactory url;
  js::VTTCueFactory vtt_cue;
  js::XMLHttpRequestFactory xml_http_request;

  js::events::EventFactory event;
  js::events::IDBVersionChangeEventFactory version_change_event;
  js::events::ProgressEventFactory progress_event;
  js::events::MediaEncryptedEventFactory media_encrypted_event;
  js::events::MediaKeyMessageEventFactory media_key_message_event;

  js::dom::NodeFactory node;
  js::dom::AttrFactory attr;
  js::dom::ContainerNodeFactory container_node;
  js::dom::CharacterDataFactory character_data;
  js::dom::ElementFactory element;
  js::dom::CommentFactory comment;
  js::dom::TextFactory text;
  js::dom::DocumentFactory document;
  js::dom::DOMExceptionFactory dom_exception;
  js::dom::DOMParserFactory dom_parser;
  js::dom::DOMStringListFactory dom_string_list;

  js::mse::MediaErrorFactory media_error;
  js::mse::MediaSourceFactory media_source;
  js::mse::SourceBufferFactory source_buffer;
  js::mse::TextTrackFactory text_track;
  js::mse::TimeRangesFactory time_ranges;
  js::mse::HTMLVideoElementFactory video_element;

  js::eme::MediaKeySessionFactory media_key_session;
  js::eme::MediaKeySystemAccessFactory media_key_system_access;
  js::eme::MediaKeysFactory media_keys;

  js::idb::IDBCursorFactory idb_cursor;
  js::idb::IDBDatabaseFactory idb_database;
  js::idb::IDBFactoryFactory idb_factory;
  js::idb::IDBObjectStoreFactory idb_object_store;
  js::idb::IDBRequestFactory idb_request;
  js::idb::IDBOpenDBRequestFactory idb_open_db_request;
  js::idb::IDBTransactionFactory idb_transaction;
};

Environment::Environment() {}
Environment::~Environment() {}


void Environment::Install() {
  RegisterDefaultKeySystems();

  impl_.reset(new Impl);

  JsEngine* engine = JsEngine::Instance();
  LocalVar<JsValue> window(engine->global_value());
  SetMemberRaw(engine->global_handle(), "window", window);

  // Shaka Player registers an "error" handler on "window".
  // TODO: Create a global object type so we can have window events.
  RegisterGlobalFunction("addEventListener", &DummyMethod);
  RegisterGlobalFunction("removeEventListener", &DummyMethod);

#if !defined(NDEBUG) && defined(USING_JSC)
  RegisterGlobalFunction("gc", &GC);
#endif

  LocalVar<JsValue> document(
      impl_->document.WrapInstance(js::dom::Document::CreateGlobalDocument()));
  SetMemberRaw(JsEngine::Instance()->global_handle(), "document", document);

  CreateInstance("console", &impl_->console);
  CreateInstance("location", &impl_->location);
  CreateInstance("navigator", &impl_->navigator);
  CreateInstance("indexedDB", &impl_->idb_factory);

  js::Base64::Install();
  js::Timeouts::Install();

  // Run the script directly since we are initializing, so this is
  // effectively the event thread.
  JsManagerImpl* manager = JsManagerImpl::Instance();
  CHECK(RunScript(manager->GetPathForStaticFile("shaka-player.compiled.js")));
}


#define ADD_GET_FACTORY(type, member)                             \
  BackingObjectFactoryBase* type::factory() const {               \
    return BackingObjectFactoryRegistry<type>::CheckedInstance(); \
  }

// \cond Doxygen_Skip
ADD_GET_FACTORY(js::Console, console);
ADD_GET_FACTORY(js::Debug, debug);
ADD_GET_FACTORY(js::Location, location);
ADD_GET_FACTORY(js::TestType, test_type);
ADD_GET_FACTORY(js::Navigator, navigator);
ADD_GET_FACTORY(js::URL, url);
ADD_GET_FACTORY(js::VTTCue, vtt_cue);
ADD_GET_FACTORY(js::XMLHttpRequest, xml_http_request);

ADD_GET_FACTORY(js::mse::MediaError, media_error);
ADD_GET_FACTORY(js::mse::MediaSource, media_source);
ADD_GET_FACTORY(js::mse::SourceBuffer, source_buffer);
ADD_GET_FACTORY(js::mse::TextTrack, text_track);
ADD_GET_FACTORY(js::mse::TimeRanges, time_ranges);
ADD_GET_FACTORY(js::mse::HTMLVideoElement, video_element);

ADD_GET_FACTORY(js::events::EventTarget, event_target);
ADD_GET_FACTORY(js::events::Event, event);
ADD_GET_FACTORY(js::events::IDBVersionChangeEvent, version_change_event);
ADD_GET_FACTORY(js::events::ProgressEvent, progress_event);
ADD_GET_FACTORY(js::events::MediaEncryptedEvent, media_encrypted_event);
ADD_GET_FACTORY(js::events::MediaKeyMessageEvent, media_key_message_event);

ADD_GET_FACTORY(js::dom::Attr, attr);
ADD_GET_FACTORY(js::dom::CharacterData, character_data);
ADD_GET_FACTORY(js::dom::Comment, comment);
ADD_GET_FACTORY(js::dom::ContainerNode, container_node);
ADD_GET_FACTORY(js::dom::Document, document);
ADD_GET_FACTORY(js::dom::DOMException, dom_exception);
ADD_GET_FACTORY(js::dom::DOMParser, dom_parser);
ADD_GET_FACTORY(js::dom::DOMStringList, dom_string_list);
ADD_GET_FACTORY(js::dom::Element, element);
ADD_GET_FACTORY(js::dom::Node, node);
ADD_GET_FACTORY(js::dom::Text, text);

ADD_GET_FACTORY(js::eme::MediaKeySession, media_key_session);
ADD_GET_FACTORY(js::eme::MediaKeySystemAccess, media_key_system_access);
ADD_GET_FACTORY(js::eme::MediaKeys, media_keys);

ADD_GET_FACTORY(js::idb::IDBCursor, idb_cursor);
ADD_GET_FACTORY(js::idb::IDBDatabase, idb_database);
ADD_GET_FACTORY(js::idb::IDBFactory, idb_factory);
ADD_GET_FACTORY(js::idb::IDBObjectStore, idb_object_store);
ADD_GET_FACTORY(js::idb::IDBRequest, idb_request);
ADD_GET_FACTORY(js::idb::IDBOpenDBRequest, idb_open_db_request);
ADD_GET_FACTORY(js::idb::IDBTransaction, idb_transaction);
// \endcond Doxygen_Skip

#undef ADD_GET_FACTORY

}  // namespace shaka
