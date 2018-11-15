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

testGroup('Base64', function() {
  testGroup('Encoding', function() {
    test('CorrectResults', function() {
      expectEq(btoa('A cat.'), 'QSBjYXQu');
    });

    test('EmptyString', function() {
      expectEq(btoa(''), '');
    });

    test('OneBytePadding', function() {
      expectEq(btoa('XY'), 'WFk=');
    });

    test('TwoBytePadding', function() {
      expectEq(btoa('X'), 'WA==');
    });

    test('BinaryData', function() {
      expectEq(btoa('\x00\x01\x02\x03\x55\x56\xfa\xff\xff'), 'AAECA1VW+v//');
    });

    test('ThrowsIfBytesOutOfRange', function() {
      try {
        btoa('\u1234');
        fail('Should throw error');
      } catch (e) {
        expectTrue(e);
      }
    });
  });

  testGroup('Decoding', function() {
    test('CorrectResults', function() {
      expectEq(atob('QSBjYXQu'), 'A cat.');
    });

    test('EmptyString', function() {
      expectEq(atob(''), '');
    });

    test('OneBytePadding', function() {
      expectEq(atob('WFk='), 'XY');
    });

    test('TwoBytePadding', function() {
      expectEq(atob('WA=='), 'X');
    });

    test('PaddingIsMissing', function() {
      expectEq(atob('WA'), 'X');
    });

    test('BinaryData', function() {
      expectEq(atob('AAECA1VW+v//'), '\x00\x01\x02\x03\x55\x56\xfa\xff\xff');
    });

    testGroup('ThrowsErrors', function() {
      test('NonBase64Characters', function() {
        shouldThrow('\x00\xff');
      });

      test('PaddingInTheMiddle', function() {
        shouldThrow('AA=AA');
      });

      test('TooFewCharacters', function() {
        shouldThrow('A');
        shouldThrow('AAAAA');
      });

      function shouldThrow(string) {
        try {
          atob(string);
          fail('Should throw error');
        } catch (e) {
          expectTrue(e);
        }
      }
    });
  });
});
