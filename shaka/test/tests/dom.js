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

testGroup('DOM', function() {
  test('CorrectTypesAndInheritance', function() {
    let type = typeof window.Node === 'function' ? 'function' : 'object';
    expectEq(typeof window.Node, type);
    expectEq(typeof window.Element, type);
    expectEq(typeof window.Document, type);
    expectEq(typeof window.Comment, type);
    expectEq(typeof window.Text, type);

    let elem = document.createElement('elem');
    expectTrue(elem);
    expectInstanceOf(elem, Node);
    expectInstanceOf(elem, Element);
    expectNotInstanceOf(elem, Document);
    expectNotInstanceOf(elem, Comment);
    expectNotInstanceOf(elem, Text);

    let text = document.createComment('text');
    expectTrue(text);
    expectInstanceOf(text, Node);
    expectNotInstanceOf(text, Element);
    expectNotInstanceOf(text, Document);
    expectInstanceOf(text, Comment);
    expectNotInstanceOf(text, Text);
  });

  test('CorrectStaticMembers', function() {
    // Don't check all of them since we don't use them.
    expectEq(Node.ELEMENT_NODE, 1);
    expectEq(Node.ATTRIBUTE_NODE, 2);
    expectEq(Node.TEXT_NODE, 3);
    expectEq(Node.CDATA_SECTION_NODE, 4);

    let x = document.createElement('test');
    expectEq(x.ELEMENT_NODE, 1);
  });

  testGroup('Element', function() {
    test('SetsFieldsCorrectly', function() {
      let elem = document.createElement('test');
      expectEq(elem.nodeType, Node.ELEMENT_NODE);
      expectEq(elem.nodeName, 'test');
      expectEq(elem.tagName, 'test');
      expectEq(elem.nodeValue, null);
      expectEq(elem.textContent, '');

      let elemWithText = document.createElement('other');
      elemWithText.appendChild(document.createTextNode('text'));
      expectEq(elemWithText.textContent, 'text');
    });

    test('CanSetAttributes', function() {
      let elem = document.createElement('test');
      elem.setAttribute('key', 'value');
      elem.setAttribute('abc', 'xyz');
      expectEq(elem.hasAttributes(), true);
      expectEq(elem.getAttribute('key'), 'value');
      expectEq(elem.getAttribute('abc'), 'xyz');
      expectEq(elem.getAttribute('missing'), null);

      elem.setAttribute('key', 'other');
      expectEq(elem.getAttribute('key'), 'other');
      expectEq(elem.getAttribute('abc'), 'xyz');
    });

    test('CanRemoveAttributes', function() {
      let elem = document.createElement('test');
      elem.setAttribute('key', 'value');
      elem.setAttribute('abc', 'xyz');
      expectTrue(elem.hasAttributes());
      expectEq(elem.getAttribute('key'), 'value');
      expectEq(elem.getAttribute('abc'), 'xyz');

      elem.removeAttribute('missing');
      expectTrue(elem.hasAttributes());
      expectEq(elem.getAttribute('key'), 'value');
      expectEq(elem.getAttribute('abc'), 'xyz');

      elem.removeAttribute('key');
      expectTrue(elem.hasAttributes());
      expectEq(elem.getAttribute('key'), null);
      expectEq(elem.getAttribute('abc'), 'xyz');

      elem.removeAttribute('abc');
      expectFalse(elem.hasAttributes());
      expectEq(elem.getAttribute('key'), null);
      expectEq(elem.getAttribute('abc'), null);
    });

    test('CanSetNamespaceAwareAttributes', function() {
      let elem = document.createElement('test');
      elem.setAttributeNS('foo-ns', 'foo:key', 'value');
      elem.setAttributeNS('bar-ns', 'bar:key', 'xyz');
      expectEq(elem.getAttribute('foo:key'), 'value');
      expectEq(elem.getAttribute('bar:key'), 'xyz');
      expectEq(elem.getAttribute('key'), null);
      expectEq(elem.getAttribute('baz:key'), null);
      expectEq(elem.getAttributeNS('foo-ns', 'key'), 'value');
      expectEq(elem.getAttributeNS('bar-ns', 'key'), 'xyz');
      expectEq(elem.getAttributeNS('other-ns', 'key'), null);
    });

    test('CanRemoveNamespaceAwareAttributes', function() {
      let elem = document.createElement('test');
      elem.setAttributeNS('foo-ns', 'foo:key', 'value');
      expectEq(elem.getAttributeNS('foo-ns', 'key'), 'value');
      expectEq(elem.getAttributeNS('foo-ns', 'abc'), null);
      expectEq(elem.getAttributeNS('bar-ns', 'key'), null);

      elem.removeAttributeNS('foo-ns', 'missing');
      elem.removeAttributeNS('bar-ns', 'key');
      expectTrue(elem.hasAttributes());
      expectEq(elem.getAttributeNS('foo-ns', 'key'), 'value');

      elem.removeAttributeNS('foo-ns', 'key');
      expectFalse(elem.hasAttributes());
      expectEq(elem.getAttribute('foo-ns', 'key'), null);
    });

    test('CanFilterChildrenByTagName', function() {
      let root = document.createElement('a');
      let child1 = document.createElement('a');
      let child1_1 = document.createElement('b');
      let child1_1_1 = document.createElement('a');
      let child1_2 = document.createElement('a');
      let child1_2_1 = document.createElement('b');
      let child2 = document.createElement('a');
      let child2_1 = document.createElement('a');
      let child3 = document.createElement('b');
      let child3_1 = document.createElement('a');
      let child3_2 = document.createElement('a');
      root.appendChild(child1);
      child1.appendChild(child1_1);
      child1_1.appendChild(child1_1_1);
      child1.appendChild(child1_2);
      child1_2.appendChild(child1_2_1);
      root.appendChild(child2);
      child2.appendChild(child2_1);
      root.appendChild(child3);
      child3.appendChild(child3_1);
      child3.appendChild(child3_2);

      let expected = [child1_1, child1_2_1, child3];
      let elements = root.getElementsByTagName('b');
      expectEq(elements.length, expected.length);
      for (var i = 0; i < elements.length; i++) {
        expectSame(elements[i], expected[i]);
      }
    });
  });
});
