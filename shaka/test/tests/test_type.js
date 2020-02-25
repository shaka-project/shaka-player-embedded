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

testGroup('TestType', function() {
  // These tests are used to test the behavior of the type converters and the
  // registering framework.  See TestType.

  // The expected values for the tests.  These must match the constants in
  // test_type.h.  These must exist in both C++ and JavaScript because that is
  // what we are testing; namely that the value of the variables are converted
  // correctly.
  const expectedInt = 123;
  const expectedNumberEnum = 2;
  const expectedStringEnum = 'other';
  // Used to verify that Unicode characters and embedded nulls are converted
  // correctly.
  const expectedString = 'ab\u2345_\0_\ud801\udc37!';
  const expectedStruct = {
    'string': expectedString,
    'boolean': true,
    any: undefined
  };
  const expectedArray = ['abc', '123', expectedString];
  const TestStringEnum = {
    EMPTY: '',
    AUTO: 'auto',
    OTHER: 'other'
  };


  if (!window.TestType) {
    console.log('Skipping TestType tests in release mode');
    return;
  }

  testGroup('BoxedPrimitives', function() {
    test('AcceptsNumbers', function() {
      let test = new TestType();
      test.acceptNumber(1234);
      test.acceptNumber(new Number(1234));

      expectToThrow(() => { test.acceptNumber(null); });
      expectToThrow(() => { test.acceptNumber(undefined); });
      expectToThrow(() => { test.acceptNumber('abc'); });
      expectToThrow(() => { test.acceptNumber({}); });
      expectToThrow(() => { test.acceptNumber(); });
    });

    test('AcceptsBooleans', function() {
      let test = new TestType();
      test.acceptBoolean(false);
      test.acceptBoolean(new Boolean(true));

      expectToThrow(() => { test.acceptBoolean(null); });
      expectToThrow(() => { test.acceptBoolean(undefined); });
      expectToThrow(() => { test.acceptBoolean('abc'); });
      expectToThrow(() => { test.acceptBoolean({}); });
      expectToThrow(() => { test.acceptBoolean(); });
    });
  });

  testGroup('strings', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptString('abc');
      test.acceptString(new String('abc'));
      test.acceptString(expectedString);

      expectToThrow(() => { test.acceptString(null); });
      expectToThrow(() => { test.acceptString(undefined); });
      expectToThrow(() => { test.acceptString(123); });
      expectToThrow(() => { test.acceptString({}); });
      expectToThrow(() => { test.acceptString(); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectFalse(test.isExpectedString('abc'));
      expectTrue(test.isExpectedString(expectedString));
      expectFalse(test.isExpectedString(new String('abc')));
      expectTrue(test.isExpectedString(new String(expectedString)));
    });

    test('ReturnValue', function() {
      let test = new TestType();
      expectEq(test.getString(), expectedString);
    });
  });

  testGroup('optional', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptOptionalString('abc');
      test.acceptOptionalString('');
      test.acceptOptionalString(null);
      test.acceptOptionalString(undefined);
      test.acceptOptionalString();
      expectToThrow(() => { test.acceptOptionalString(123); });
      expectToThrow(() => { test.acceptOptionalString({}); });

      test.acceptOptionalStruct({});
      test.acceptOptionalStruct(null);
      test.acceptOptionalStruct(undefined);
      test.acceptOptionalStruct();
      expectToThrow(() => { test.acceptOptionalStruct(123); });
      expectToThrow(() => { test.acceptOptionalStruct('abc'); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectTrue(test.isOptionalPresent('abc'));
      expectTrue(test.isOptionalPresent(''));
      expectFalse(test.isOptionalPresent(null));
      expectFalse(test.isOptionalPresent(undefined));
      expectFalse(test.isOptionalPresent());
    });

    test('ReturnValue', function() {
      let test = new TestType();
      expectEq(test.getOptionalString(false), null);
      expectEq(test.getOptionalString(true), expectedString);
    });

    test('TracesChildren', function() {
      let test = new TestType();
      test.optionalObject = {abc: 1};
      gc();
      expectEq(test.optionalObject.abc, 1);
    });
  });

  testGroup('or', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptIntOrStruct(123);
      test.acceptIntOrStruct({});
      expectToThrow(() => { test.acceptIntOrStruct(false); });
      expectToThrow(() => { test.acceptIntOrStruct('abc'); });
      expectToThrow(() => { test.acceptIntOrStruct(null); });
      expectToThrow(() => { test.acceptIntOrStruct(undefined); });
      expectToThrow(() => { test.acceptIntOrStruct(); });

      test.acceptStringEnumOrAnyNumber(TestStringEnum.AUTO);
      test.acceptStringEnumOrAnyNumber(123);
      expectToThrow(() => { test.acceptStringEnumOrAnyNumber('foobar'); });
      expectToThrow(() => { test.acceptStringEnumOrAnyNumber({}); });
      expectToThrow(() => { test.acceptStringEnumOrAnyNumber(); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectTrue(test.isExpectedIntWithOr(expectedInt));
      expectFalse(test.isExpectedIntWithOr(0));
      expectFalse(test.isExpectedIntWithOr({}));

      expectTrue(test.isExpectedStructWithOr(expectedStruct));
      expectFalse(test.isExpectedStructWithOr(0));
      expectFalse(test.isExpectedStructWithOr({}));
    });

    test('ReturnValue', function() {
      let test = new TestType();
      expectEq(test.getIntOrString(true), expectedInt);
      expectEq(test.getIntOrString(false), expectedString);
    });

    test('TracesChildren', function() {
      let test = new TestType();
      test.intOrObject = {abc: 1};
      gc();
      expectEq(test.intOrObject.abc, 1);
    });
  });

  testGroup('structs', function() {
    /**
     * The type of the C++ struct:
     *
     * @type {{
     *   string: string,
     *   boolean: boolean,
     *   any: *
     *  }}
     */

    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptStruct({});
      test.acceptStruct({abc: 123});
      test.acceptStruct({string: 'abc'});
      test.acceptStruct({string: 123});
      expectToThrow(() => { test.acceptStruct(null); });
      expectToThrow(() => { test.acceptStruct('abc'); });
      expectToThrow(() => { test.acceptStruct(); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectFalse(test.isExpectedConvertedStruct({}));
      expectTrue(test.isExpectedConvertedStruct(expectedStruct));
      expectTrue(test.isConvertedStructEmpty({}));
      expectFalse(test.isConvertedStructEmpty({string: 'abc'}));

      // Key 'abc' is not in the C++ type, so it is ignored.
      expectTrue(test.isConvertedStructEmpty({abc: '123'}));
      // 999 is not the correct type, so it is ignored.
      expectTrue(test.isConvertedStructEmpty({string: 999}));
    });

    test('ReturnValue', function() {
      let test = new TestType();
      var o = test.getStruct();
      expectEq(o, expectedStruct);
      expectNotSame(o, expectedStruct);
    });

    test('TracesChildren', function() {
      // Caution, setting a member on a struct directly won't work.
      // test.struct.any = {abc: 1};
      let test = new TestType();
      test.struct = {any: {abc: 1}};
      gc();
      expectEq(test.struct.any.abc, 1);
    });

    test('ReturnsSameObject', () => {
      const obj = {foo: 'x', string: 'y'};
      const ret = new TestType().changeStringField(obj);
      expectSame(ret, obj);
      expectEq(ret.foo, 'x');
      expectEq(ret.string, 'abc');
    });
  });

  testGroup('enums', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptNumberEnum(expectedNumberEnum);
      test.acceptNumberEnum(999);
      expectToThrow(() => { test.acceptNumberEnum(null); });
      expectToThrow(() => { test.acceptNumberEnum({}); });
      expectToThrow(() => { test.acceptNumberEnum(''); });
      expectToThrow(() => { test.acceptNumberEnum(); });

      test.acceptStringEnum(TestStringEnum.EMPTY);
      test.acceptStringEnum(expectedStringEnum);
      expectToThrow(() => { test.acceptStringEnum(12); });
      // Not a valid enum.
      expectToThrow(() => { test.acceptStringEnum('foobar'); });
      expectToThrow(() => { test.acceptStringEnum({}); });
      expectToThrow(() => { test.acceptStringEnum(); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectTrue(test.isExpectedNumberEnum(expectedNumberEnum));
      expectFalse(test.isExpectedNumberEnum(123));

      expectTrue(test.isExpectedStringEnum(expectedStringEnum));
      expectFalse(test.isExpectedStringEnum(''));
    });

    test('ReturnValue', function() {
      let test = new TestType();
      expectEq(test.getNumberEnum(), expectedNumberEnum);
      expectEq(test.getStringEnum(), expectedStringEnum);
    });
  });

  testGroup('arrays', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptArrayOfStrings([]);
      test.acceptArrayOfStrings(['x']);
      test.acceptArrayOfStrings(['a', 'b', 'c']);
      expectToThrow(() => { test.acceptArrayOfStrings(null); });
      expectToThrow(() => { test.acceptArrayOfStrings('abc'); });
      expectToThrow(() => { test.acceptArrayOfStrings([2]); });
      expectToThrow(() => {
        test.acceptArrayOfStrings(['a', 'b', undefined, 'd']);
      });
      expectToThrow(() => { test.acceptArrayOfStrings(); });

      expectToThrow(() => { test.acceptArrayOfStrings(new Array(2)); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectTrue(test.isExpectedArrayOfStrings(expectedArray));
      expectFalse(test.isExpectedArrayOfStrings([]));
      expectFalse(test.isExpectedArrayOfStrings(['a', 'b', 'c']));
    });

    test('ReturnValue', function() {
      let test = new TestType();
      let result = test.getArrayOfStrings();
      expectEq(result, expectedArray);
      expectNotSame(result, expectedArray);
    });

    test('TracesChildren', function() {
      let test = new TestType();
      test.array = [123, {abc: 1}];
      gc();
      expectEq(test.array, [123, {abc: 1}]);
    });
  });

  testGroup('maps', function() {
    test('ReturnValue', function() {
      let test = new TestType();
      let result = test.getMapOfStrings();
      expectTrue(result);
      expectEq(result['a'], '1');
      expectEq(result['b'], '2');
    });
  });

  testGroup('callbacks', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptCallback(function() {});
      test.acceptCallback(function(a) {});
      test.acceptCallback(fail);
      expectToThrow(() => { test.acceptCallback(null); });
      expectToThrow(() => { test.acceptCallback(12); });
      expectToThrow(() => { test.acceptCallback('abc'); });
      expectToThrow(() => { test.acceptCallback(); });
    });

    test('CanInvokeArgument', function() {
      let test = new TestType();
      let spy = jasmine.createSpy('callback');
      spy.and.callFake(function(a) { expectEq(a, expectedString); });
      test.invokeCallbackWithString(spy);
      expectToHaveBeenCalled(spy);
    });

    test('TracesChildren', function() {
      return new Promise(function(resolve) {
        let test = new TestType();
        test.callback = resolve;
        gc();
        expectEq(typeof test.callback, 'function');
        test.callback();
      });
    });
  });

  testGroup('anything', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptAnything({});
      test.acceptAnything([]);
      test.acceptAnything('str');
      test.acceptAnything(123);
      test.acceptAnything(true);
      test.acceptAnything(null);
      test.acceptAnything(undefined);
      // Note Anything is not optional.
      expectToThrow(() => { test.acceptAnything(); });
    });

    test('Arguments', function() {
      let test = new TestType();
      expectTrue(test.isExpectedStringWithAny(expectedString));
      expectFalse(test.isExpectedStringWithAny(''));
      expectFalse(test.isExpectedStringWithAny(1));
    });

    test('DeterminesTruthiness', function() {
      let test = new TestType();
      expectTrue(test.isTruthy(true));
      expectTrue(test.isTruthy(123));
      expectTrue(test.isTruthy(-10));
      expectTrue(test.isTruthy('foo'));
      expectTrue(test.isTruthy({}));
      expectTrue(test.isTruthy({a: 1}));
      expectTrue(test.isTruthy([]));
      expectTrue(test.isTruthy([1, 2]));

      expectFalse(test.isTruthy(false));
      expectFalse(test.isTruthy(0));
      expectFalse(test.isTruthy(NaN));
      expectFalse(test.isTruthy(''));
      expectFalse(test.isTruthy(null));
      expectFalse(test.isTruthy(undefined));
    });

    test('TracesChildren', function() {
      let test = new TestType();
      test.any = {abc: 1};
      gc();
      expectEq(test.any.abc, 1);

      // There was a bug where tracing a number caused V8 to crash.
      test.any = 123;
      gc();
      expectEq(test.any, 123);
    });
  });

  testGroup('ByteBuffer', function() {
    test('ThrowsForInvalidTypes', function() {
      let test = new TestType();
      test.acceptByteBuffer(new ArrayBuffer(10));
      test.acceptByteBuffer(new ArrayBuffer(0));
      test.acceptByteBuffer(new Uint8Array([]));

      expectToThrow(() => { test.acceptByteBuffer(12); });
      expectToThrow(() => { test.acceptByteBuffer({}); });
      expectToThrow(() => { test.acceptByteBuffer([]); });
      expectToThrow(() => { test.acceptByteBuffer(null); });
      expectToThrow(() => { test.acceptByteBuffer(); });
    });

    test('WorksWithFields', function() {
      let test = new TestType();
      var buf = new ArrayBuffer(10);
      test.buffer = buf;
      expectSame(test.buffer, buf);
    });

    test('KeepsDataAccrossGcRuns', function() {
      let test = new TestType();
      let arr = [1, 2, 3, 4, 5];
      test.storeByteBuffer(new Uint8Array(arr).buffer);
      gc();

      let buf = new Uint8Array(test.getByteBuffer());
      expectEq(buf.length, arr.length);
      for (let i = 0; i < buf.length; i++) {
        expectEq(buf[i], arr[i]);
      }
    });

    test('represents a common buffer', function() {
      let test = new TestType();
      let arr = [1, 2, 3, 4, 5];
      let buffer = new Uint8Array(arr);
      test.storeByteBuffer(buffer.buffer);

      // Mutate the common buffer.
      arr = [5, 4, 3, 2, 1];
      for (let i = 0; i < arr.length; i++) {
        buffer[i] = arr[i];
      }

      // The return value should contain the same buffer.
      let after = new Uint8Array(test.getByteBuffer());
      expectEq(after.length, buffer.length);
      for (let i = 0; i < buffer.length; i++) {
        expectEq(after[i], buffer[i]);
      }
    });
  });

  testGroup('Promises', function() {
    test('ReturnValue', async function() {
      let test = new TestType();
      let p = test.promiseResolveWith(1);
      expectTrue(p);
      expectInstanceOf(p, Promise);
      expectInstanceOf(p.then, Function);

      await p.then(function(value) {
        expectEq(value, 1);
      });
    });

    test('ReturnsRejectedPromiseForTypeErrors', async function() {
      try {
        let test = new TestType();
        await test.promiseAcceptString(123);
        fail('Should reject Promise');
      } catch(error) {
        expectInstanceOf(error, TypeError);
        expectEq(error.name, 'TypeError');
        expectTrue(error.message);
      }
    });

    test('WillResolveAsynchronously', async function() {
      let test = new TestType();
      var checked = false;
      setTimeout(function() {
        checked = true;
      }, 15);

      await test.promiseResolveAfter(250)
      expectTrue(checked);
    });
  });

  testGroup('toPrettyString', function() {
    test('ConvertsNulls', function() {
      let test = new TestType();
      expectEq(test.toPrettyString(undefined), 'undefined');
      expectEq(test.toPrettyString(null), 'null');
    });

    test('ConvertsNumbers', function() {
      let test = new TestType();
      expectEq(test.toPrettyString(0), '0');
      expectEq(test.toPrettyString(1.2), '1.2');
      expectEq(test.toPrettyString(2.0), '2');
    });

    test('ConvertsBooleans', function() {
      let test = new TestType();
      expectEq(test.toPrettyString(true), 'true');
      expectEq(test.toPrettyString(false), 'false');
    });

    test('ConvertsStrings', function() {
      let test = new TestType();
      expectEq(test.toPrettyString(''), '""');
      expectEq(test.toPrettyString('abc'), '"abc"');
      expectEq(test.toPrettyString('a\nb\nc'), '"a\\nb\\nc"');
      expectEq(test.toPrettyString('a\tb\tc'), '"a\\tb\\tc"');
    });

    test('ConvertsPrimitiveObjects', function() {
      let test = new TestType();
      expectEq(test.toPrettyString(new String('abc')), 'String("abc")');
      expectEq(test.toPrettyString(new Boolean(false)), 'Boolean(false)');
      expectEq(test.toPrettyString(new Number(123)), 'Number(123)');
    });

    test('ConvertsBuiltInObjects', function() {
      let test = new TestType();
      let promise = new Promise(()=>{});
      expectEq(test.toPrettyString(promise), '[object Promise]');

      let buffer = new ArrayBuffer();
      expectEq(test.toPrettyString(buffer), '[object ArrayBuffer]');

      let xhr = new XMLHttpRequest();
      expectEq(test.toPrettyString(xhr), '[object XMLHttpRequest]');
    });

    test('ConvertsArrays', function() {
      let test = new TestType();
      expectEq(test.toPrettyString([]), '[]');
      expectEq(test.toPrettyString([0, 1, 2]), '[0, 1, 2]');
      expectEq(test.toPrettyString([[], [], []]), '[[...], [...], [...]]');

      // Make sure that long arrays get "chopped-off".
      let longArray = [
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
        21, 22, 23, 24, 25
      ];
      let longArrayString =
          '[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, '+
          '11, 12, 13, 14, 15, 16, 17, 18, 19, 20, ' +
          '...]';
      expectEq(test.toPrettyString(longArray), longArrayString);
    });

    test('ConvertsObjects', function() {
      let test = new TestType();
      expectEq(test.toPrettyString({}), '{}');
      expectEq(test.toPrettyString({a:0}), '{a:0}');
      expectEq(test.toPrettyString({a:0, b:1}), '{a:0, b:1}');
      expectEq(test.toPrettyString({b:1, a:0}), '{a:0, b:1}');
      expectEq(test.toPrettyString({a: 'a'}), '{a:"a"}');
      expectEq(test.toPrettyString({a: []}), '{a:[...]}');
      expectEq(test.toPrettyString({a: {}}), '{a:{...}}');

      // Make sure that long objects get "chopped-off".
      let longObject = {
        a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10,
        k:11, l:12, m:13, n:14, o:15, p:16, q:17, r:18, s:19, t:20,
        u:21, v:22, w:23, x:24, y:25
      };
      let longObjectString =
          '{a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10, ' +
          'k:11, l:12, m:13, n:14, o:15, p:16, q:17, r:18, s:19, t:20, ' +
          '...}';
      expectEq(test.toPrettyString(longObject), longObjectString);

      function Ctor() {}
      let inst = new Ctor();
      inst.foo = 1;
      expectEq(test.toPrettyString(inst), '{foo:1}');
    });

    test('StillConvertsWithCustomToString', function() {
      let test = new TestType();
      let obj = {};
      obj.toString = function() { return '[object Promise]'; }
      expectEq(test.toPrettyString(obj), '{toString:function() {...}}');
    });
  });

  test('ThrowsExceptions', function() {
    try {
      let test = new TestType();
      test.throwException('Foo');
      fail('Should throw exception');
    } catch (e) {
      expectInstanceOf(e, Error);
      expectEq(e.message, 'Foo');
    }
  });
});
