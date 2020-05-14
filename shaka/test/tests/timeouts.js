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

testGroup('timeouts', function() {
  test('InvokesAfterDelay', async function() {
    return new Promise((resolve) => {
      let start = Date.now();
      setTimeout(function() {
        var end = Date.now();
        expectTrue(end - start >= 49);
        resolve();
      }, 50);
    });
  });

  test('ReturnsNumber', async function() {
    return new Promise((resolve) => {
      let x = setTimeout(resolve, 0);
      expectEq(typeof x, 'number');
    });
  });

  test('ClearsTimeoutsSynchronously', async function() {
    return new Promise((resolve) => {
      let handle = setTimeout(fail, 50);
      clearTimeout(handle);

      setTimeout(resolve, 100);
    });
  });

  test('ClearsTimeoutsAsynchronously', async function() {
    return new Promise((resolve) => {
      let handle = setTimeout(fail, 100);
      setTimeout(function() {
        clearTimeout(handle);
      }, 25);

      setTimeout(resolve, 200);
    });
  });

  test('FiresShorterTimeoutsBeforeLongerOnes', async function() {
    if (!window.Debug) {
      testSkip();
      return;
    }

    return new Promise((resolve) => {
      // Even though this is registered first and will (probably) be available
      // to be fired after this function returns, it should pick the second one
      // since it has a lower timeout.
      var order = 0;
      setTimeout(function() {
        expectEq(order, 1);
        order = 2;
      }, 12);
      setTimeout(function() {
        expectEq(order, 0);
        order = 1;
      }, 6);

      // Synchronously sleep so both are "ready" to be fired.
      Debug.sleep(15);

      setTimeout(function() {
        expectEq(order, 2);
        resolve();
      }, 50);
    });
  });

  test('CallsAndClearsIntervals', async function() {
    return new Promise((resolve) => {
      let count = 0;
      let handle = setInterval(function() {
        count++;
        if (count >= 3)
          clearInterval(handle);
      }, 5);

      setTimeout(function() {
        expectEq(count, 3);
        resolve();
      }, 50);
    });
  });

  test('WillKeepFunctionsAlive', async function() {
    if (!window.gc) {
      testSkip();
    } else {
      return new Promise((resolve) => {
        var call = function() { resolve(); };
        setTimeout(call, 10);
        call = null;

        // Would normally destroy |call|.
        gc();
      });
    }
  });
});
