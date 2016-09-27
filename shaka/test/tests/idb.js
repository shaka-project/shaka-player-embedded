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

testGroup('IndexedDB', () => {
  testGroup('StructuredObjectClone', () => {
    test('Primitives', () => {
      const testData = [
        123,
        -1,
        0,
        NaN,
        Infinity,
        'abc',
        '',
        '\u1234\u4312\0!',
        true,
        false,
        null,
        undefined,
      ];

      for (const data of testData) {
        const copy = clone(data);
        expectEq(typeof copy, typeof data);
        if (typeof data === 'number' && isNaN(data)) {
          expectTrue(isNaN(copy));
        } else {
          expectEq(copy, data);
        }
      }
    });

    test('PrimitiveObjects', () => {
      const testData = [
        new Number(123),
        new Number(-1),
        new Number(NaN),
        new Number(Infinity),
        new String('abc'),
        new String(),
        new Boolean(false),
        new Boolean(true),
      ];

      for (const data of testData) {
        const copy = clone(data);
        expectEq(typeof copy, 'object');
        expectEq(copy, data);
        expectNotSame(copy, data);
        expectEq(copy instanceof Number, data instanceof Number);
        expectEq(copy instanceof String, data instanceof String);
        expectEq(copy instanceof Boolean, data instanceof Boolean);
      }
    });

    testGroup('BinaryData', () => {
      test('ArrayBuffers', () => {
        const data = [1, 54, 255, 0, 42];
        const original = new Uint8Array(data).buffer;

        const copy = clone(original);
        expectEq(typeof copy, 'object');
        expectInstanceOf(copy, ArrayBuffer);
        expectNotSame(copy, original);
        expectEq(copy.byteLength, data.length);
        expectDataEquals(new Uint8Array(copy, data), data);
      });

      test('PartialViews', () => {
        const data = [0, 0, 0, 0, 1, 54, 255, 0, 42, 0, 0, 0];
        const start = 4;
        const length = 5;
        const buffer = new Uint8Array(data).buffer;
        const original = new Uint8Array(buffer, start, length);

        const copy = clone(original);
        expectEq(typeof copy, 'object');
        expectInstanceOf(copy, Uint8Array);
        expectEq(copy.byteLength, original.length);
        expectEq(copy.length, original.length);
        expectNotSame(copy, original);
        expectNotSame(copy.buffer, original.buffer);

        expectDataEquals(copy, data.slice(start, start + length));
      });

      for (const ctor
               of [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array,
                   Uint16Array, Int32Array, Uint32Array, Float32Array,
                   Float64Array]) {
        test('Entire' + ctor.name, () => {
          const data = [0, 1, 2, 3, 4, 5, 6, 7, 8];
          const original = new ctor(data);
          const copy = clone(original);

          expectEq(typeof copy, 'object');
          expectInstanceOf(copy, ctor);
          expectEq(copy.byteLength, original.byteLength);
          expectEq(copy.length, original.length);
          expectNotSame(copy, original);
          expectNotSame(copy.buffer, original.buffer);
          expectDataEquals(copy, data);
        });
      }

      /**
       * @param {!ArrayBufferView} copy
       * @param {!Array.<number>} data
       */
      function expectDataEquals(copy, data) {
        expectEq(copy.length, data.length);
        for (let i = 0; i < copy.length; i++) {
          expectEq(copy[i], data[i]);
        }
      }
    });

    testGroup('Objects', () => {
      test('Arrays', () => {
        const source = [1, 2, 'a', {a: 1}];
        source.foobar = 'baz';
        const copy = clone(source);
        expectEq(copy, source);
        expectNotSame(copy, source);
      });

      test('NestedObjects', () => {
        const source = {a: 1, b: 'abc', c: [1], d: {x: 2, y: {}}};
        const copy = clone(source);
        console.log(JSON.stringify(copy));
        expectEq(copy, source);
        expectNotSame(copy, source);
      });

      test('NotCopyNonEnumerableProperties', () => {
        const source = {a: 'other'};
        Object.defineProperty(source, 'test', {enumerable: false, value: 2});
        const copy = clone(source);
        expectNotSame(copy, source);
        expectEq(copy.test, undefined);
        expectEq(copy.a, 'other');
      });

      // TODO: Fix this test, it currently fails on JSC since there isn't a way
      // to check if it is an "own" property.
      xtest('NotCopyFromPrototype', () => {
        const prop = {key: 'value'};
        const obj = Object.create(prop);
        expectEq(obj.key, 'value');
        const copy = clone(obj);
        expectEq(copy.key, undefined);
      });

      test('NotCopyGettersSetters', () => {
        let value = 1;
        const source = {};
        Object.defineProperty(source, 'test', {
          enumerable: true,
          get: () => value,
          set: () => {},
        });
        const copy = clone(source);
        expectNotSame(copy, source);
        expectEq(copy.test, 1);
        value = 2;
        expectEq(source.test, 2);
        expectEq(copy.test, 1);

        const descSource = Object.getOwnPropertyDescriptor(source, 'test');
        const descCopy = Object.getOwnPropertyDescriptor(copy, 'test');
        expectTrue(descSource.set);
        expectTrue(descCopy.writable);
        expectEq(descCopy.set, undefined);
      });

      test('ThrowsForFunctions', () => {
        expectToThrow(() => clone(() => {}));
        expectToThrow(() => clone({a: () => {}}));
      });

      test('ThrowsForDate', () => {
        expectToThrow(() => clone(new Date(0)));
        expectToThrow(() => clone({a: new Date(0)}));
      });

      test('ThrowsForDuplicateObjects', () => {
        const obj = {};
        const source = {a: obj, b: {x: obj}};
        expectToThrow(() => clone(source));
      });
    });

    function clone(value) {
      return window.indexedDB.cloneForTesting(value);
    }
  });
});
