// Copyright 2018 Google LLC
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

testGroup('XML', function() {
  test('ParsesSimpleXml', function() {
    const text = '<top><item>1</item><other attr="2" /></top>';

    let document = new DOMParser().parseFromString(text, 'text/xml');
    expectTrue(document);

    let element = document.documentElement;
    expectTrue(element);
    expectInstanceOf(element, Element);
    expectEq(element.tagName, 'top');
    expectEq(element.childNodes.length, 2);

    let item1 = element.childNodes[0];
    expectInstanceOf(item1, Element);
    expectEq(item1.tagName, 'item');
    expectEq(item1.textContent, '1');
    expectEq(item1.hasAttributes(), false);

    let item2 = element.childNodes[1];
    expectInstanceOf(item2, Element);
    expectEq(item2.tagName, 'other');
    expectEq(item2.textContent, '');
    expectEq(item2.hasAttributes(), true);
    expectEq(item2.getAttribute('attr'), '2');
    expectEq(item2.getAttribute('other'), null);
  });

  test('ParsesNamespaceAwareXml', function() {
    const text = [
      '<top xmlns="https://example.com" xmlns:foo="https://example.com/foo">',
      '<first />',
      '<second xmlns="https://example.com/other" />',
      '<foo:third />',
      '<fourth attr="1" foo:qual="2" />',
      '<fifth xmlns="https://example.com/other" ',
      ' xmlns:bar="https://example.com/bar" attr="1" bar:qual="2" />',
      '</top>'
    ].join('');

    let document = new DOMParser().parseFromString(text, 'text/xml');
    expectTrue(document);
    let root = document.documentElement;
    expectInstanceOf(root, Element);

    expectElement(
        root.childNodes[0], 'first', 'first', null, 'https://example.com');
    expectElement(
        root.childNodes[1], 'second', 'second', null,
        'https://example.com/other');
    expectElement(
        root.childNodes[2], 'foo:third', 'third', 'foo',
        'https://example.com/foo');

    let fourth = root.childNodes[3];
    expectEq(fourth.getAttribute('attr'), '1');
    expectEq(fourth.getAttribute('foo:qual'), '2');
    expectEq(fourth.getAttributeNS('https://example.com', 'attr'), null);
    expectEq(fourth.getAttributeNS('https://example.com/foo', 'attr'), null);
    expectEq(fourth.getAttributeNS('https://example.com/bar', 'attr'), null);
    expectEq(fourth.getAttributeNS('https://example.com/foo', 'qual'), '2');
    expectEq(fourth.getAttributeNS('https://example.com/bar', 'qual'), null);
    expectEq(fourth.getAttributeNS('https://example.com', 'qual'), null);
    expectEq(fourth.getAttributeNS('https://example.com', 'missing'), null);
    expectEq(fourth.getAttributeNS('https://example.com/foo', 'missing'), null);

    let fifth = root.childNodes[4];
    expectEq(fifth.getAttribute('attr'), '1');
    expectEq(fifth.getAttribute('bar:qual'), '2');
    expectEq(fifth.getAttributeNS('https://example.com', 'attr'), null);
    expectEq(fifth.getAttributeNS('https://example.com/other', 'attr'), null);
    expectEq(fifth.getAttributeNS('https://example.com/bar', 'qual'), '2');
    expectEq(fifth.getAttributeNS('https://example.com/bar', 'attr'), null);
  });

  function expectElement(element, tag, localName, prefix, ns) {
    expectInstanceOf(element, Element);
    expectEq(element.tagName, tag);
    expectEq(element.localName, localName);
    expectEq(element.prefix, prefix);
    expectEq(element.namespaceURI, ns);
  }
});
