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


/**
 * @private {string}
 * Defines the "path" for the current test group.
 */
var curTestName = '';

/**
 * @private {!Array.<!Array.<function()>>}
 * Defines a stack of beforeEach methods.  Each element contains the beforeEach
 * methods for that nested testGroup.
 */
var curBeforeEach = [[]];
var curAfterEach = [[]];


/**
 * This is defined by the environment to mark an expectation failure for the
 * current test.  This does not stop the test.
 *
 * @param {string} message The error message to print.
 * @param {string} file The file name where the error occurred.
 * @param {number} line The line number where the error occurred.
 */
// function fail_(message, file, line) {}


/**
 * This is defined by the environment to define the test.  This should be given
 * the "full" test name, so the test() function will handle group names.
 *
 * @param {string} name The name of the test.
 * @param {function()} callback The function that defines the test.
 */
// function test_(name, callback) {}


/**
 * This is defined by the environment to mark the current test as skipped.  The
 * caller still needs to return early to skip the body.
 */
// function testSkip() {}


/**
 * Returns an object containing the call stack info for the caller of the caller
 * of this function.
 * @return {{file: string, line: number}}
 */
function getCodeLocation_() {
  try {
    throw new Error('XXX');
  } catch (e) {
    let stack = ((e || {}).stack || '').split('\n');
    if (stack[0].endsWith('XXX'))
      stack.shift();

    // V8 format with function name.
    const line = stack[2] || '';
    let m = /at [^(]+\(([^:]+):(\d+):\d+\)$/.exec(line);
    if (!m) {
      // V8 format without function name.
      m = /at ([^(:]+):(\d+):\d+$/.exec(line);
    }

    if (m) {
      return {file: m[1], line: Number(m[2])};
    } else {
      console.log(line);
      return {file: 'unknown', line: 0};
    }
  }
}


/**
 * Expects that the given value is "truthy".
 * @param {*} val The value to check.
 */
function expectTrue(val) {
  if (!val) {
    const stack = getCodeLocation_();
    fail_('Expected: ' + val + '\n   To be: truthy', stack.file, stack.line);
  }
}


/**
 * Expects that the given value is "falsy".
 * @param {*} val The value to check.
 */
function expectFalse(val) {
  if (val) {
    const stack = getCodeLocation_();
    fail_('Expected: ' + val + '\n   To be: falsy', stack.file, stack.line);
  }
}


/**
 * Expects that the given value is equal to the given expected value.  This does
 * value comparisons and compares objects recursively.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expected The expected value we want.
 */
function expectEq(actual, expected) {
  if (jasmine.matchersUtil.equals(actual, expected)) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('      Expected: ' + actual + '\n' +
        'To be equal to: ' + expected, stack.file, stack.line);
}


/**
 * Expects that the given value is not equal to the given expected value.  This
 * does value comparisons and compares objects recursively.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expected The expected value we don't want.
 */
function expectNotEq(actual, expected) {
  if (!jasmine.matchersUtil.equals(actual, expected)) {
    return;
  }

  const stack = getCodeLocation_();
  fail_(
      '          Expected: ' + actual + '\n' +
          'To not be equal to: ' + expected,
      stack.file, stack.line);
}


/**
 * Expects that the given value is exactly equal to the given expected value
 * using the JavaScript ===.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expected The expected value we want.
 */
function expectSame(actual, expected) {
  if (actual === expected) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('         Expected: ' + actual + '\n' +
        'To be the same as: ' + expected, stack.file, stack.line);
}


/**
 * Expects that the given value is not exactly equal to the given expected value
 * using the JavaScript ===.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expected The expected value we want.
 */
function expectNotSame(actual, expected) {
  if (actual !== expected) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('             Expected: ' + actual + '\n' +
        'To not be the same as: ' + expected, stack.file, stack.line);
}


/**
 * Expects that the given value is an instance of the given type.  If the
 * expected type is a primitive type (e.g. Number), then the primitive type will
 * also be accepted.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expectedType The type that the value should be.
 */
function expectInstanceOf(actual, expectedType) {
  const type = typeof actual;
  if ((expectedType == Number && type === 'number') ||
      (expectedType == String && type === 'string') ||
      (expectedType == Boolean && type === 'boolean')) {
    return;
  }

  if (!(actual instanceof expectedType)) {
    const stack = getCodeLocation_();
    fail_('            Expected: ' + actual + '\n' +
          'To be an instance of: ' + expectedType.name, stack.file, stack.line);
  }
}


/**
 * Expects that the given value is not an instance of the given type.  If the
 * expected type is a primitive type (e.g. Number), then the primitive type will
 * also be accepted.
 *
 * @param {*} actual The actual value to test.
 * @param {*} expectedType The type that the value should be.
 */
function expectNotInstanceOf(actual, expectedType) {
  const type = typeof actual;
  if ((expectedType == Number && type !== 'number') ||
      (expectedType == String && type !== 'string') ||
      (expectedType == Boolean && type !== 'boolean')) {
    return;
  }

  if (actual instanceof expectedType) {
    const stack = getCodeLocation_();
    fail_('                Expected: ' + actual + '\n' +
          'To not be an instance of: ' + expectedType.name, stack.file,
          stack.line);
  }
}


/**
 * Expects that the given spy has been called at least one time.
 *
 * @param {*} spy The spy to check.
 */
function expectToHaveBeenCalled(spy) {
  if (spy.calls.any()) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('Expected: ' + spy.and.identity() + '\n' +
        '      To: Have been called', stack.file, stack.line);
}


/**
 * Expects that the given spy has not been called at all.
 *
 * @param {*} spy The spy to check.
 */
function expectNotToHaveBeenCalled(spy) {
  if (!spy.calls.any()) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('Expected: ' + spy.and.identity() + '\n' +
        '      To: Not have been called', stack.file, stack.line);
}


/**
 * Expects that the given spy has been called the given number of times.
 *
 * @param {*} spy The spy to check.
 * @param {number} times The number of times it should have been called.
 */
function expectToHaveBeenCalledTimes(spy, times) {
  if (spy.calls.count() == times) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('                 Expected: ' + spy.and.identity() + '\n' +
        'To have been called times: ' + times, stack.file, stack.line);
}


/**
 * Expects that the given spy has been called at least once with the given
 * arguments.
 *
 * @param {*} spy The spy to check.
 * @param {...*} args The arguments to check for.
 */
function expectToHaveBeenCalledWith(spy) {
  let args = Array.prototype.slice.call(arguments, 1);
  if (jasmine.matchersUtil.contains(spy.calls.allArgs(), args)) {
    return;
  }

  const stack = getCodeLocation_();
  fail_('                Expected: ' + spy.and.identity() + '\n' +
        'To have been called with: [' + args.join(', ') + ']', stack.file,
        stack.line);
}


/**
 * Expects that the given function to throw an exception.
 *
 * @param {function()} callback The function that should throw.
 * @param {string=} name The name of the expected exception.
 */
function expectToThrow(callback, name) {
  const stack = getCodeLocation_();
  try {
    callback();
    fail_('Expected function to throw', stack.file, stack.line);
  } catch (e) {
    if (name) {
      if (e.name != name) {
        fail_(
            'Expected function to throw: ' + name + '\n' +
                '           But got instead: ' + e.name,
            stack.file, stack.line);
      }
    }
  }
}


/**
 * Fails the current test with the given message.
 *
 * @param {string} message The failure message.
 */
function fail(message) {
  const stack = getCodeLocation_();
  fail_('Failure: ' + message, stack.file, stack.line);
}


/**
 * Creates a group of tests.  The callback should define the tests that will
 * be inside this group.  You can call testGroup again in the callback to create
 * groups of groups.
 *
 * @param {string} name The name of the group.
 * @param {function()} callback The function that defines the group.
 */
function testGroup(name, callback) {
  let old = curTestName;
  if (curTestName) {
    curTestName += '.' + name;
  } else {
    curTestName = name
  }
  curBeforeEach.push([]);
  curAfterEach.push([]);

  callback();

  curTestName = old;
  curBeforeEach.pop();
  curAfterEach.pop();
}

/** Adds a beforeEach callback.  This is called before each test is run. */
function beforeEach(callback) {
  curBeforeEach[curBeforeEach.length - 1].push(callback);
}

/** Adds a afterEach callback.  This is called after each test is run. */
function afterEach(callback) {
  curAfterEach[curAfterEach.length - 1].push(callback);
}


/**
 * Defines a new test.  This function will be called when the test is run.
 *
 * @param {string} name The name of the test.
 * @param {function()} callback The function that defines the test.
 */
function test(name, callback) {
  if (curTestName) {
    name = curTestName + '.' + name;
  }

  const join = (arr) => arr.reduce((all, part) => all.concat(part), []);
  const beforeEach = join(curBeforeEach);
  const afterEach = join(curAfterEach);
  const wrapped = async () => {
    for (const cb of beforeEach) {
      await cb();
    }

    try {
      await callback();
    } finally {
      for (const cb of afterEach) {
        await cb();
      }
    }
  };
  test_(name, wrapped);
}


/**
 * Defines a disabled test.
 *
 * @param {string} name The name of the test.
 * @param {function()} callback The function that defines the test.
 */
function xtest(name, callback) {}


/**
 * Defines a disabled test group.
 *
 * @param {string} name The name of the test.
 * @param {function()} callback The function that defines the test.
 */
function xtestGroup(name, callback) {}
