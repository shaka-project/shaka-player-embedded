// Copyright 2020 Google LLC
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

testGroup('TextCoders', () => {
  testGroup('TextDecoder', () => {
    test('ThrowsForUnknownEncoding', () => {
      // Unsupported.
      expectToThrow(() => new TextDecoder('windows-1250'));
      // Invalid.
      expectToThrow(() => new TextDecoder('foobar'));
    });

    test('DecodesNothing', () => {
      const data = new Uint8Array([]);
      expectEq(new TextDecoder().decode(data), '');
    });

    test('DecodesAscii', () => {
      const data = new Uint8Array([0x46, 0x6f, 0x6f, 0x62, 0x61, 0x72]);
      expectEq(new TextDecoder().decode(data), 'Foobar');
    });

    test('DecodesUnicode', () => {
      const data = new Uint8Array(
          [0x46, 0xe2, 0x82, 0xac, 0x20, 0xf0, 0x90, 0x8d, 0x88]);
      expectEq(new TextDecoder().decode(data), 'F\u20ac \ud800\udf48');
    });
  });

  testGroup('TextEncoder', () => {
    test('EncodesNothing', () => {
      const data = new TextEncoder().encode('');
      expectEq(data.byteLength, 0);
    });

    test('EncodesAscii', () => {
      const data = new TextEncoder().encode('Foobar');
      expectEq(new Uint8Array(data),
               new Uint8Array([0x46, 0x6f, 0x6f, 0x62, 0x61, 0x72]));
    });

    test('EncodesUnicode', () => {
      const data = new TextEncoder().encode('F\u20ac \ud800\udf48');
      expectEq(new Uint8Array(data),
               new Uint8Array(
                   [0x46, 0xe2, 0x82, 0xac, 0x20, 0xf0, 0x90, 0x8d, 0x88]));
    });
  });
});
