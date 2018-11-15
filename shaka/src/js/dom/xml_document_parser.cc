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

#include "src/js/dom/xml_document_parser.h"

#include <libxml/parser.h>

#include <utility>

#include "src/js/dom/comment.h"
#include "src/js/dom/document.h"
#include "src/js/dom/element.h"
#include "src/js/dom/text.h"
#include "src/js/js_error.h"
#include "src/util/utils.h"

namespace shaka {
namespace js {
namespace dom {

// Read the following article for how the libxml SAX interface works:
// http://www.jamesh.id.au/articles/libxml-sax/libxml-sax.html

namespace {

// xmlChar == unsigned char.  We treat these as UTF-8 strings of chars.
static_assert(sizeof(xmlChar) == sizeof(char), "Must be raw characters");


XMLDocumentParser* GetParser(void* context) {
  return reinterpret_cast<XMLDocumentParser*>(context);
}

std::string ToString(const xmlChar* data) {
  return reinterpret_cast<const char*>(data);
}

std::string ToString(const xmlChar* data, int length) {
  auto* text = reinterpret_cast<const char*>(data);
  return std::string(text, text + length);
}


void SaxEndDocument(void* context) {
  GetParser(context)->EndDocument();
}

void SaxStartElementNS(void* context, const xmlChar* local_name,
                       const xmlChar* prefix, const xmlChar* namespace_uri,
                       int /* nb_namespaces */,
                       const xmlChar** /* namespaces */, int nb_attributes,
                       int /* nb_defaulted */, const xmlChar** attributes) {
  GetParser(context)->StartElement(
      ToString(local_name),
      namespace_uri ? optional<std::string>(ToString(namespace_uri)) : nullopt,
      prefix ? optional<std::string>(ToString(prefix)) : nullopt, nb_attributes,
      reinterpret_cast<const char**>(attributes));
}

void SaxEndElementNS(void* context, const xmlChar* /* localname */,
                     const xmlChar* /* prefix */, const xmlChar* /* URI */) {
  GetParser(context)->EndElement();
}

void SaxCharacters(void* context, const xmlChar* raw_data, int size) {
  GetParser(context)->Text(ToString(raw_data, size));
}

void SaxProcessingInstruction(void* context, const xmlChar* /* target */,
                              const xmlChar* /* data */) {
  GetParser(context)->SetException(JsError::DOMException(NotSupportedError));
}

void SaxComment(void* context, const xmlChar* raw_data) {
  GetParser(context)->Comment(ToString(raw_data));
}

PRINTF_FORMAT(2, 3)
void SaxWarning(  // NOLINT(cert-dcl50-cpp)
    void* /* context */, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = util::StringPrintfV(format, args);
  LOG(WARNING) << "XML parsing warning: " << message;
  va_end(args);
}

PRINTF_FORMAT(2, 3)
void SaxError(  // NOLINT(cert-dcl50-cpp)
    void* context, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string message = util::StringPrintfV(format, args);
  va_end(args);

  GetParser(context)->SetException(
      JsError::DOMException(UnknownError, message));
}

void SaxCdata(void* context, const xmlChar* value, int len) {
  // We do not have a separate CDATA type, so treat as text.
  GetParser(context)->Text(ToString(value, len));
}

}  // namespace

XMLDocumentParser::XMLDocumentParser(RefPtr<Document> document)
    : document_(document), current_node_(document) {}

XMLDocumentParser::~XMLDocumentParser() {}

ExceptionOr<RefPtr<Document>> XMLDocumentParser::Parse(
    const std::string& source) {
  // TODO: libxml says we should call xmlInitParser in case of multithreaded
  // programs; however it works without it.  We may not want to change global
  // state of libxml so embedders can use it without us changing it.

  xmlSAXHandler sax;
  memset(&sax, 0, sizeof(sax));
  sax.initialized = XML_SAX2_MAGIC;
  sax.endDocument = &SaxEndDocument;
  sax.startElementNs = &SaxStartElementNS;
  sax.endElementNs = &SaxEndElementNS;
  sax.characters = &SaxCharacters;
  sax.processingInstruction = &SaxProcessingInstruction;
  sax.comment = &SaxComment;
  sax.warning = &SaxWarning;
  sax.error = &SaxError;
  sax.fatalError = &SaxError;
  sax.cdataBlock = &SaxCdata;

  int code = xmlSAXUserParseMemory(&sax, this, source.c_str(), source.size());
  if (code < 0) {
    LOG(ERROR) << "Error parsing XML document, code=" << code;
    return error_ ? std::move(*error_) : JsError::DOMException(UnknownError);
  }
  if (error_)
    return std::move(*error_);

  return document_;
}

void XMLDocumentParser::EndDocument() {
  FinishTextNode();
}

void XMLDocumentParser::StartElement(const std::string& local_name,
                                     optional<std::string> namespace_uri,
                                     optional<std::string> namespace_prefix,
                                     size_t attribute_count,
                                     const char** attributes) {
  FinishTextNode();

  RefPtr<Element> child =
      new Element(document_, local_name, namespace_uri, namespace_prefix);
  for (size_t i = 0; i < attribute_count; i++) {
    // Each attribute has the following values in |attributes|.
    const char* local_name = attributes[i * 5];
    const char* namespace_prefix = attributes[i * 5 + 1];
    const char* namespace_uri = attributes[i * 5 + 2];
    const char* value_begin = attributes[i * 5 + 3];
    const char* value_end = attributes[i * 5 + 4];

    if (namespace_uri) {
      std::string qualified_name;
      if (namespace_prefix)
        qualified_name = std::string(namespace_prefix) + ":" + local_name;
      else
        qualified_name = local_name;

      child->SetAttributeNS(namespace_uri, qualified_name,
                            std::string(value_begin, value_end));
    } else {
      child->SetAttribute(local_name, std::string(value_begin, value_end));
    }
  }

  current_node_->AppendChild(child);
  current_node_ = child;
}

void XMLDocumentParser::EndElement() {
  FinishTextNode();
  current_node_ = current_node_->parent_node();
  DCHECK(!current_node_.empty());
}

void XMLDocumentParser::Text(const std::string& text) {
  current_text_.append(text);
}

void XMLDocumentParser::Comment(const std::string& text) {
  FinishTextNode();
  current_node_->AppendChild(document_->CreateComment(text));
}

void XMLDocumentParser::SetException(JsError error) {
  error_.reset(new JsError(std::move(error)));
}

void XMLDocumentParser::FinishTextNode() {
  if (!current_text_.empty()) {
    current_node_->AppendChild(document_->CreateTextNode(current_text_));
    current_text_.clear();
  }
}

}  // namespace dom
}  // namespace js
}  // namespace shaka
