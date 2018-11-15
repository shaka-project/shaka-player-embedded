#!/usr/bin/python
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A code-generator used for exposing types from JavaScript through the API.

This generates several types from the same IDL definition: (a) an internal
type used in the mapping framework, (b) a type exposed to the C++ API, and
(c) a type exposed in the Objective-C API.
"""

import argparse
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'webidl'))
import embed_utils
import webidl


def _MapCppType(t, other_types, is_public):
  """Returns a C++ type name for the given IDL type."""
  if t.element_type:
    assert t.name == 'sequence'
    ret = 'std::vector<%s>' % _MapCppType(
        t.element_type, other_types, is_public)
  elif t.name in other_types:
    if is_public:
      return 'shaka::' + t.name
    else:
      return 'shaka::js::' + t.name
  else:
    type_map = {
        # IDL   ->  C++
        'boolean': 'bool',
        'double': 'double',
        'DOMString': 'std::string',
    }
    assert t.name in type_map, 'Type %s not found' % t.name
    ret = type_map[t.name]

  return 'shaka::optional<%s>' % ret if t.nullable else ret


def _MapObjcType(t, other_types):
  """Returns an Objective-C type name for the given IDL type."""
  if t.element_type:
    assert t.name == 'sequence'
    return 'NSArray<%s>*' % _MapObjcType(t.element_type, other_types)
  elif t.name in other_types:
    return 'Shaka%s*' % t.name
  else:
    type_map = {
        # IDL   ->  Objective-C
        'boolean': 'BOOL',
        'double': 'double',
        'DOMString': 'NSString*',
    }
    assert t.name in type_map, 'Type %s not found' % t.name
    return type_map[t.name]


def _GetPublicFieldName(attr):
  """Converts a lowerCamelCase field name to lower_with_underscores."""
  return re.sub(r'([A-Z])', r'_\1', attr.name).lower()


def _GetObjcName(attr):
  """Returns the Objective-C name for the given attribute."""
  if attr.name.startswith('init'):
    return 'getI' + attr.name[1:]
  return attr.name


def _FormatDoc(doc, indent):
  """Formats a docstring for printing."""
  # Find how much the first part is indented.
  prefix = doc.split('/', 1)[0]
  lines = doc.split('\n')
  # Then remove that much from each next line so we preserve relative indenting.
  for i in range(len(lines)):
    assert lines[i].startswith(prefix)
    lines[i] = (' ' * indent) + lines[i][len(prefix):]
  return '\n'.join(lines).lstrip()


def _GetDefault(t):
  """Returns a string containing the default value of the given type."""
  if t.element_type or t.nullable:
    return None  # Arrays and optional<T> are default-initialized
  type_map = {
      'boolean': 'false',
      'double': '0',
      'DOMString': None,  # std::string are default-initialized.
  }
  assert t.name in type_map, 'Type %s not found' % t.name
  return type_map[t.name]


def _GenerateJsHeader(results, f, name, public_header):
  """Generates the header for the JavaScript mapping type."""
  other_types = [t.name for t in results.types]
  writer = embed_utils.CodeWriter(f)
  writer.Write('#ifndef SHAKA_EMBEDDED_INTERNAL_%s_H_', name.upper())
  writer.Write('#define SHAKA_EMBEDDED_INTERNAL_%s_H_', name.upper())
  writer.Write()
  writer.Write('#include <string>')
  writer.Write('#include <vector>')
  writer.Write()
  writer.Write('#include "shaka/optional.h"')
  writer.Write('#include "%s"', public_header)
  writer.Write('#include "src/mapping/convert_js.h"')
  writer.Write('#include "src/mapping/struct.h"')
  writer.Write()
  with writer.Namespace('shaka'):
    with writer.Namespace('js'):
      for t in results.types:
        with writer.Block('struct %s : Struct' % t.name, semicolon=True):
          with writer.Block('static std::string name()'):
            writer.Write('return "%s";', t.name)
          writer.Write()

          writer.Write('%s();', t.name)
          writer.Write('%s(const %s&);', t.name, t.name)
          writer.Write('%s(%s&&);', t.name, t.name)
          writer.Write('~%s();', t.name)
          writer.Write()
          writer.Write('%s& operator=(const %s&);', t.name, t.name)
          writer.Write('%s& operator=(%s&&);', t.name, t.name)
          writer.Write()

          for attr in t.attributes:
            writer.Write('ADD_DICT_FIELD(%s, %s);',
                         _MapCppType(attr.type, other_types, is_public=False),
                         attr.name)
        writer.Write()
    writer.Write()

    for t in results.types:
      writer.Write('template <>')
      with writer.Block(
          'struct impl::ConvertHelper<shaka::%s, void>' % t.name,
          semicolon=True):
        with writer.Block('static bool FromJsValue(Handle<JsValue> source, '
                          'shaka::%s* dest)' % t.name):
          writer.Write('shaka::js::%s temp;', t.name)
          with writer.Block('if (!ConvertHelper<shaka::js::%s>::FromJsValue('
                            'source, &temp))' % t.name):
            writer.Write('return false;')
          writer.Write('*dest = shaka::%s(temp);', t.name)
          writer.Write('return true;')
      writer.Write()

  writer.Write()
  writer.Write('#endif  // SHAKA_EMBEDDED_INTERNAL_%s_H_', name.upper())


def _GenerateJsSource(results, f, header):
  """Generates the source file for the JavaScript mapping type."""
  writer = embed_utils.CodeWriter(f)
  writer.Write('#include "%s"', header)
  writer.Write()
  with writer.Namespace('shaka'):
    with writer.Namespace('js'):
      for t in results.types:
        writer.Write('%s::%s() {}', t.name, t.name)
        writer.Write('%s::%s(const %s&) = default;', t.name, t.name, t.name)
        writer.Write('%s::%s(%s&&) = default;', t.name, t.name, t.name)
        writer.Write('%s::~%s() {}', t.name, t.name)
        writer.Write()
        writer.Write('%s& %s::operator=(const %s&) = default;', t.name, t.name,
                     t.name)
        writer.Write('%s& %s::operator=(%s&&) = default;', t.name, t.name,
                     t.name)
        writer.Write()
        writer.Write()


def _GeneratePublicHeader(results, f, name):
  """Generates the header for the public C++ type."""
  other_types = [t.name for t in results.types]
  writer = embed_utils.CodeWriter(f)
  writer.Write('#ifndef SHAKA_EMBEDDED_%s_H_', name.upper())
  writer.Write('#define SHAKA_EMBEDDED_%s_H_', name.upper())
  writer.Write()
  writer.Write('#include <memory>')
  writer.Write('#include <string>')
  writer.Write('#include <vector>')
  writer.Write()
  writer.Write('#include "macros.h"')
  writer.Write('#include "optional.h"')
  writer.Write()
  with writer.Namespace('shaka'):
    with writer.Namespace('js'):
      for t in results.types:
        writer.Write('struct %s;', t.name)
    writer.Write()

    for t in results.types:
      if t.doc:
        writer.Write(_FormatDoc(t.doc, indent=0))
      with writer.Block('class SHAKA_EXPORT %s final' % t.name, semicolon=True):
        writer.Write('public:', offset=-1)
        writer.Write('%s();', t.name)
        writer.Write('%s(const js::%s& internal);', t.name, t.name)
        writer.Write('%s(const %s&);', t.name, t.name)
        writer.Write('%s(%s&&);', t.name, t.name)
        writer.Write('~%s();', t.name)
        writer.Write()
        writer.Write('%s& operator=(const %s&);', t.name, t.name)
        writer.Write('%s& operator=(%s&&);', t.name, t.name)
        writer.Write()

        for attr in t.attributes:
          if attr.doc:
            writer.Write(_FormatDoc(attr.doc, indent=2))
          writer.Write('%s %s() const;',
                       _MapCppType(attr.type, other_types, is_public=True),
                       _GetPublicFieldName(attr))
        writer.Write()

        writer.Write('private:', offset=-1)
        writer.Write('friend class Player;')
        writer.Write()
        writer.Write('const js::%s& GetInternal() const;', t.name)
        writer.Write()
        writer.Write('class Impl;')
        writer.Write('std::shared_ptr<Impl> impl_;')
      writer.Write()

  writer.Write()
  writer.Write('#endif  // SHAKA_EMBEDDED_%s_H_', name.upper())


def _GeneratePublicSource(results, f, public_header, internal_header):
  """Generates the source for the public C++ type."""
  other_types = [t.name for t in results.types]
  writer = embed_utils.CodeWriter(f)
  writer.Write('#include "%s"', public_header)
  writer.Write()
  writer.Write('#include "%s"', internal_header)
  writer.Write()
  with writer.Namespace('shaka'):
    for t in results.types:
      with writer.Block('class %s::Impl' % t.name, semicolon=True):
        writer.Write('public:', offset=-1)
        writer.Write('const js::%s value;', t.name)
      writer.Write()

      writer.Write('%s::%s() : impl_(new Impl) {}', t.name, t.name)
      writer.Write(
          '%s::%s(const js::%s& internal) : impl_(new Impl{internal}) {}',
          t.name, t.name, t.name)
      writer.Write('%s::%s(const %s&) = default;', t.name, t.name, t.name)
      writer.Write('%s::%s(%s&&) = default;', t.name, t.name, t.name)
      writer.Write('%s::~%s() {}', t.name, t.name)
      writer.Write()
      writer.Write('%s& %s::operator=(const %s&) = default;', t.name, t.name,
                   t.name)
      writer.Write('%s& %s::operator=(%s&&) = default;', t.name, t.name, t.name)
      writer.Write()

      for attr in t.attributes:
        with writer.Block('%s %s::%s() const' %
                          (_MapCppType(attr.type, other_types, is_public=True),
                           t.name, _GetPublicFieldName(attr))):
          if attr.type.name == 'sequence':
            writer.Write(
                'return {impl_->value.%s.begin(), impl_->value.%s.end()};',
                attr.name, attr.name)
          else:
            writer.Write('return impl_->value.%s;', attr.name)
        writer.Write()
      writer.Write()

      with writer.Block('const js::%s& %s::GetInternal() const' %
                        (t.name, t.name)):
        writer.Write('return impl_->value;')
      writer.Write()
      writer.Write()


def _GenerateObjcHeader(results, f, name):
  """Generates the header for the public Objective-C type."""
  other_types = [t.name for t in results.types]
  writer = embed_utils.CodeWriter(f)
  writer.Write('#ifndef SHAKA_EMBEDDED_OBJC_%s_H_', name.upper())
  writer.Write('#define SHAKA_EMBEDDED_OBJC_%s_H_', name.upper())
  writer.Write()
  writer.Write('#import <Foundation/Foundation.h>')
  writer.Write()
  writer.Write('#include "macros.h"')
  writer.Write()
  for t in results.types:
    if t.doc:
      writer.Write(_FormatDoc(t.doc, indent=0))
    writer.Write('SHAKA_EXPORT')
    writer.Write('@interface Shaka%s : NSObject', t.name)
    writer.Write()

    for attr in t.attributes:
      if attr.doc:
        writer.Write(_FormatDoc(attr.doc, indent=0))
      writer.Write('@property (atomic, readonly) %s %s;',
                   _MapObjcType(attr.type, other_types), _GetObjcName(attr))
      writer.Write()

    writer.Write('@end')
    writer.Write()
    writer.Write()

  writer.Write('#endif  // SHAKA_EMBEDDED_OBJC_%s_H_', name.upper())


def _GenerateObjcSource(results, f, header, objc_internal_header):
  """Generates the source for the public Objective-C type."""
  other_types = [t.name for t in results.types]
  writer = embed_utils.CodeWriter(f)
  writer.Write('#import "%s"', header)
  writer.Write('#import "%s"', objc_internal_header)
  writer.Write('#import "src/util/objc_utils.h"')
  writer.Write()
  for t in results.types:
    writer.Write('@implementation Shaka%s', t.name)
    writer.Write()

    with writer.Block(
        '- (instancetype)initWithCpp:(const shaka::%s&)obj' % t.name):
      with writer.Block('if ((self = [super init]))'):
        writer.Write('self->value = obj;')
      writer.Write('return self;')
    writer.Write()

    with writer.Block('- (const shaka::%s&)toCpp' % t.name):
      writer.Write('return self->value;')
    writer.Write()

    for attr in t.attributes:
      with writer.Block('- (%s)%s' %
                        (_MapObjcType(attr.type, other_types),
                         _GetObjcName(attr))):
        writer.Write(
            'return shaka::util::ObjcConverter<%s>::ToObjc(self->value.%s());',
            _MapCppType(attr.type, other_types, is_public=True),
            _GetPublicFieldName(attr))
      writer.Write()

    writer.Write('@end')
    writer.Write()
    writer.Write()


def _GenerateObjcInternalHeader(results, f, name, public_header, objc_header):
  """Generates the header for the internal Objective-C type."""
  writer = embed_utils.CodeWriter(f)
  writer.Write('#ifndef SHAKA_EMBEDDED_OBJC_%s_INTERNAL_H_', name.upper())
  writer.Write('#define SHAKA_EMBEDDED_OBJC_%s_INTERNAL_H_', name.upper())
  writer.Write()
  writer.Write('#import "%s"', objc_header)
  writer.Write('#include "%s"', public_header)
  writer.Write('#include "src/util/objc_utils.h"')
  writer.Write()
  for t in results.types:
    with writer.Block('@interface Shaka%s()' % t.name):
      writer.Write('shaka::%s value;', t.name)
    writer.Write()
    writer.Write('- (instancetype)initWithCpp:(const shaka::%s&)obj;', t.name)
    writer.Write()
    writer.Write('- (const shaka::%s&)toCpp;', t.name)
    writer.Write()
    writer.Write('@end')
    writer.Write()
    writer.Write('template <>')
    with writer.Block('struct shaka::util::ObjcConverter<shaka::%s>' % t.name,
                      semicolon=True):
      with writer.Block('static Shaka%s* ToObjc(const shaka::%s& val)' %
                        (t.name, t.name)):
        writer.Write('return [[Shaka%s alloc] initWithCpp:val];', t.name)
      writer.Write()
    writer.Write()
    writer.Write()

  writer.Write('#endif  // SHAKA_EMBEDDED_OBJC_%s_INTERNAL_H_', name.upper())


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--internal-dir', dest='internal', required=True,
                      help='The directory to output internal files to.')
  parser.add_argument('--internal-rel-dir', dest='internal_rel', required=True,
                      help='The relative path for the internal directory, '
                           'used for #include.')
  parser.add_argument('--public-dir', dest='public', required=True,
                      help='The directory to output public files to.')
  parser.add_argument('input', help='The IDL file to parse.')

  ns = parser.parse_args(args)
  with open(ns.input, 'r') as f:
    results = webidl.IdlParser().parse_file(ns.input, f.read())

  if not os.path.exists(ns.internal):
    os.makedirs(ns.internal)

  # Remove the 'shaka/' prefix.
  internal_rel = ns.internal_rel[ns.internal_rel.index('/')+1:]

  name = os.path.splitext(os.path.basename(ns.input))[0]
  public_header = 'shaka/%s.h' % name
  internal_header = '%s/%s.h' % (internal_rel, name)
  objc_internal_header = '%s/%s+Internal.h' % (internal_rel, name)
  with open(os.path.join(ns.internal, name + '.h'), 'w+') as f:
    _GenerateJsHeader(results, f, name, public_header)
  with open(os.path.join(ns.internal, name + '_js.cc'), 'w+') as f:
    _GenerateJsSource(results, f, internal_header)
  with open(os.path.join(ns.public, name + '.h'), 'w+') as f:
    _GeneratePublicHeader(results, f, name)
  with open(os.path.join(ns.public, name + '.cc'), 'w+') as f:
    _GeneratePublicSource(results, f, public_header, internal_header)

  with open(os.path.join(ns.public, name + '_objc.h'), 'w+') as f:
    _GenerateObjcHeader(results, f, name)
  with open(os.path.join(ns.public, name + '_objc.mm'), 'w+') as f:
    _GenerateObjcSource(results, f, name + '_objc.h', objc_internal_header)
  with open(os.path.join(ns.internal, name + '+Internal.h'), 'w+') as f:
    _GenerateObjcInternalHeader(results, f, name, public_header,
                                'shaka/%s_objc.h' % name)


if __name__ == '__main__':
  main(sys.argv[1:])
