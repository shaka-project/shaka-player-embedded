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
        expectToThrow(() => clone(() => {}), 'DataCloneError');
        expectToThrow(() => clone({a: () => {}}), 'DataCloneError');
      });

      test('ThrowsForDate', () => {
        expectToThrow(() => clone(new Date(0)), 'DataCloneError');
        expectToThrow(() => clone({a: new Date(0)}), 'DataCloneError');
      });

      test('ThrowsForDuplicateObjects', () => {
        const obj = {};
        const source = {a: obj, b: {x: obj}};
        expectToThrow(() => clone(source), 'DataCloneError');
      });
    });

    function clone(value) {
      return window.indexedDB.cloneForTesting(value);
    }
  });

  testGroup('UpgradingDatabases', () => {
    test('FiresUpgradeNeededEvent', () => {
      return runUpgradeTest((request, evt) => {
        expectInstanceOf(request.result, IDBDatabase);
        expectInstanceOf(request.transaction, IDBTransaction);
        expectEq(request.transaction.mode, 'versionchange');
      });
    });

    test('AddObjectStore', () => {
      return runUpgradeTest((request) => {
        const db = request.result;
        expectEq(Array.from(db.objectStoreNames), []);

        const store = db.createObjectStore('foo', {autoIncrement: true});
        expectInstanceOf(store, IDBObjectStore);
        expectEq(store.name, 'foo');
        expectEq(store.keyPath, null);
        expectEq(store.transaction, request.transaction);
        expectEq(store.autoIncrement, true);
        expectEq(Array.from(db.objectStoreNames), ['foo']);
      });
    });

    test('NoKeyPathSupport', () => {
      return runUpgradeTest((request) => {
        const db = request.result;
        expectToThrow(() => db.createObjectStore('foo', {keyPath: 'bar'}));
      });
    });

    test('NoExternalKeySupport', () => {
      return runUpgradeTest((request) => {
        const db = request.result;
        expectToThrow(() => db.createObjectStore('foo'));
      });
    });

    test('AbortOnTransactionAborted', () => {
      return runAbortTest((request) => {
        request.transaction.abort();
      });
    });

    test('AbortOnException', () => {
      return runAbortTest((request) => {
        throw 'An error';
      });
    });

    async function runUpgradeTest(callback) {
      const request = indexedDB.openTestDb();

      let wasUpgraded = false;
      request.onupgradeneeded = (evt) => {
        wasUpgraded = true;
        expectTrue(evt);
        expectInstanceOf(evt, Event);

        expectEq(evt.target, request);
        expectEq(evt.oldVersion, 0);
        expectEq(evt.newVersion, 1);
        callback(request);
      };

      await promisify(request);
      expectEq(wasUpgraded, true);
      request.result.close();
    }

    function runAbortTest(callback) {
      return new Promise((resolve, reject) => {
        const request = indexedDB.openTestDb();

        const upgradeneeded = jasmine.createSpy('onupgradeneeded');
        upgradeneeded.and.callFake((evt) => {
          callback(request);
        });
        request.onupgradeneeded = upgradeneeded;

        const success = jasmine.createSpy('onsuccess');
        request.onsuccess = success;

        const error = jasmine.createSpy('onerror');
        error.and.callFake((evt) => {
          expectEq(request.error.name, 'AbortError');
        });
        request.onerror = error;

        setTimeout(() => {
          expectToHaveBeenCalled(upgradeneeded);
          expectNotToHaveBeenCalled(success);
          resolve();
        }, 5);
      });
    }
  });

  testGroup('Requests', () => {
    const storeName = 'foo';
    let db;

    beforeEach(async () => {
      const req = indexedDB.openTestDb();
      req.onupgradeneeded = () => {
        req.result.createObjectStore(storeName, {autoIncrement: true});
      };
      db = await promisify(req);
    });

    afterEach(() => {
      db.close();
    });

    for (const method of ['add', 'put']) {
      testGroup(method, () => {
        test('BasicFlow', async () => {
          const trans = db.transaction(storeName, 'readwrite');
          const store = trans.objectStore(storeName);

          const data = {value: 'abc'};
          const req1 = store[method](data);

          let didCheck = false;
          req1.onerror = fail;
          req1.onsuccess = (event) => {
            const key = req1.result;
            expectNotEq(key, undefined);

            const req2 = store.get(key);
            req2.onerror = fail;
            req2.onsuccess = () => {
              expectEq(req2.result, data);
              didCheck = true;
            };
          };

          await promisify(trans);
          expectEq(didCheck, true);
        });

        test('ThrowsForReadOnly', () => {
          const trans = db.transaction(storeName, 'readonly');
          const store = trans.objectStore(storeName);

          expectToThrow(() => store[method]({}), 'ReadOnlyError');
        });

        test('RethrowsWhenCloningObject', () => {
          const trans = db.transaction('foo', 'readwrite');
          const store = trans.objectStore('foo');

          const obj = {};
          Object.defineProperty(obj, 'value', {
            enumerable: true,
            get: () => {
              throw 'An error';
            },
          });
          expectToThrow(() => store[method](obj));
        });
      });
    }

    test('ReplacesExistingEntry', async () => {
      const trans = db.transaction(storeName, 'readwrite');
      const store = trans.objectStore(storeName);

      const oldData = {value: 'old', extra: 'data'};
      const newData = {value: 'new'};

      let didCheck = false;
      const add = store.add(oldData);
      add.onerror = fail;
      add.onsuccess = (event) => {
        const key = event.target.result;

        var update = store.put(newData, key);
        update.onerror = fail;
        update.onsuccess = () => {
          var get = store.get(key);
          get.onerror = fail;
          get.onsuccess = () => {
            expectEq(get.result, newData);
            didCheck = true;
          };
        };
      };

      await promisify(trans);
      expectEq(didCheck, true);
    });

    test('AbortsOnError', async () => {
      const trans = db.transaction(storeName, 'readwrite');
      const store = trans.objectStore(storeName);

      const get = store.get(0);
      get.onerror = fail;
      get.onsuccess = (event) => {
        throw new Error('Some error');
      };

      await new Promise((resolve, reject) => {
        trans.oncomplete = reject;
        trans.onabort = resolve;
      });
    });

    testGroup('Delete', () => {
      let key1, key2, key3;

      beforeEach(async () => {
        const trans = db.transaction(storeName, 'readwrite');
        const store = trans.objectStore(storeName);

        const p1 = promisify(store.add({data: 'a'}));
        const p2 = promisify(store.add({data: 'b'}));
        const p3 = promisify(store.add({data: 'c'}));

        const keys = await Promise.all([p1, p2, p3]);
        key1 = keys[0];
        key2 = keys[1];
        key3 = keys[2];
      });

      test('DeletesExistingEntry', () => {
        return testDeleteEntry(key2, [true, false, true]);
      });

      test('DoesNothingIfNotFound', () => {
        return testDeleteEntry(10, [true, true, true]);
      });

      test('ThrowsOnReadOnlyTransaction', () => {
        const trans = db.transaction(storeName, 'readonly');
        const store = trans.objectStore(storeName);
        expectToThrow(() => store.delete(2), 'ReadOnlyError');
      });

      async function testDeleteEntry(key, doesExist) {
        const trans = db.transaction('foo', 'readwrite');
        const store = trans.objectStore('foo');

        let exists1 = null, exists2 = null, exists3 = null;
        const del = store.delete(key);
        del.onerror = fail;
        del.onsuccess = () => {
          const get1 = store.get(key1);
          get1.onerror = fail;
          get1.onsuccess = () => {
            exists1 = get1.result !== undefined;
          };

          const get2 = store.get(key2);
          get2.onerror = fail;
          get2.onsuccess = () => {
            exists2 = get2.result !== undefined;
          };

          var get3 = store.get(key3);
          get3.onerror = fail;
          get3.onsuccess = () => {
            exists3 = get3.result !== undefined;
          };
        };

        await promisify(trans);
        expectEq(exists1, doesExist[0]);
        expectEq(exists2, doesExist[1]);
        expectEq(exists3, doesExist[2]);
      }
    });

    testGroup('OpenCursor', () => {
      let key1, key2, key3;

      beforeEach(async () => {
        const trans = db.transaction(storeName, 'readwrite');
        const store = trans.objectStore(storeName);

        const p1 = promisify(store.put({data: 'a'}));
        const p2 = promisify(store.put({data: 'b'}));
        const p3 = promisify(store.put({data: 'c'}));

        const keys = await Promise.all([p1, p2, p3]);
        key1 = keys[0];
        key2 = keys[1];
        key3 = keys[2];
      });

      test('IterateForwards', async () => {
        const trans = db.transaction(storeName);
        const store = trans.objectStore(storeName);

        const values = [];
        createCursor(store, values, 'next', true);

        await promisify(trans);
        expectEq(values, [key1, key2, key3, null]);
      });

      test('IterateBackwards', async () => {
        const trans = db.transaction(storeName);
        const store = trans.objectStore(storeName);

        const values = [];
        createCursor(store, values, 'prev', true);

        await promisify(trans);
        expectEq(values, [key3, key2, key1, null]);
      });

      test('OnlyIterateInContinue', async () => {
        const trans = db.transaction(storeName);
        const store = trans.objectStore(storeName);

        const values = [];
        createCursor(store, values, 'next', false);

        await promisify(trans);
        expectEq(values, [key1]);
      });

      test('CanDeleteEntries', async () => {
        const trans = db.transaction(storeName, 'readwrite');
        const store = trans.objectStore(storeName);

        const values = [];
        const req = store.openCursor();
        req.onsuccess = () => {
          if (req.result.key == key2) {
            req.result.delete();

            // This will create a new request to iterate this new cursor.  The
            // delete request should complete before this cursor iterates,
            // meaning it should see the delete.
            createCursor(store, values, 'next', true);
          } else {
            req.result.continue();
          }
        };

        await promisify(trans);
        expectEq(values, [key1, key3, null]);
      });

      test('DeleteWhenReadOnly', async () => {
        const trans = db.transaction(storeName);
        const store = trans.objectStore(storeName);

        const req = store.openCursor();
        req.onsuccess = () => {
          expectToThrow(() => req.result.delete(), 'ReadOnlyError');
        };

        await promisify(trans);
      });

      function createCursor(store, values, dir, willContinue) {
        const req = store.openCursor(null, dir);
        req.onerror = fail;
        req.onsuccess = () => {
          const cursor = req.result;
          if (cursor) {
            values.push(cursor.key);

            expectSame(cursor.source, store);
            expectEq(cursor.key, cursor.primaryKey);
            if (cursor.key == key1) {
              expectEq(cursor.value, {data: 'a'});
            } else if (cursor.key == key2) {
              expectEq(cursor.value, {data: 'b'});
            } else {
              expectEq(cursor.value, {data: 'c'});
            }

            if (willContinue) {
              cursor.continue();
            }
          } else {
            values.push(null);
          }
        };
      }
    });
  });

  testGroup('Transactions', () => {
    const storeName = 'foo';
    const storeName2 = 'other';
    let db;

    beforeEach(async () => {
      const req = indexedDB.openTestDb();
      req.onupgradeneeded = function() {
        req.result.createObjectStore(storeName, {autoIncrement: true});
        req.result.createObjectStore(storeName2, {autoIncrement: true});
      };
      db = await promisify(req);
    });

    afterEach(() => {
      db.close();
    });

    test('CreateTransactions', async () => {
      const trans = db.transaction(storeName);
      expectTrue(trans);
      expectInstanceOf(trans, IDBTransaction);
      expectSame(trans.db, db);
      expectEq(trans.mode, 'readonly');

      const store = trans.objectStore(storeName);
      expectTrue(store);
      expectInstanceOf(store, IDBObjectStore);
      expectEq(store.autoIncrement, true);
      expectEq(store.keyPath, null);
      expectEq(store.name, storeName);
      expectSame(store.transaction, trans);

      // Wait until transaction is complete just in case.
      await promisify(trans);
    });

    test('AbortTransactions', async () => {
      var trans = db.transaction(storeName, 'readwrite');
      var req = trans.objectStore(storeName).add({});
      req.onsuccess = fail;
      req.onerror = () => {
        expectEq(req.result, undefined);
        expectEq(req.error.name, 'AbortError');
      };
      trans.onabort = jasmine.createSpy('onabort');
      trans.oncomplete = jasmine.createSpy('oncomplete');
      trans.abort();

      return new Promise((resolve) => {
        setTimeout(() => {
          expectToHaveBeenCalled(trans.onabort);
          expectNotToHaveBeenCalled(trans.oncomplete);
          resolve();
        }, 5);
      });
    });

    test('MultipleConcurrentTransactions', async () => {
      const trans1 = db.transaction(storeName);
      trans1.objectStore(storeName).get(0);
      const trans2 = db.transaction(storeName);
      trans2.objectStore(storeName).get(0);

      await Promise.all([promisify(trans1), promisify(trans2)]);
    });

    test('OnlyIncludesTheGivenObjectStores', () => {
      const trans = db.transaction(storeName);
      expectToThrow(() => trans.objectStore(storeName2), 'NotFoundError');
    });

    test('ThrowsForMissingObjectStores', () => {
      expectToThrow(() => db.transaction(['missing']), 'NotFoundError');
      expectToThrow(() => db.transaction('missing'), 'NotFoundError');
      expectToThrow(
          () => db.transaction([storeName, 'missing']), 'NotFoundError');
    });

    test('throws for invalid mode', () => {
      expectToThrow(() => db.transaction('foo', 'invalid'), 'TypeError');
      expectToThrow(() => db.transaction('foo', 'versionchange'), 'TypeError');
    });
  });

  testGroup('Persistence', () => {
    const dbName = 'persistence_test';

    beforeEach(() => {
      return promisify(indexedDB.deleteDatabase(dbName));
    });

    test('CanPersist', async () => {
      const data = {abc: 123};
      let dataKey;

      {
        const create = indexedDB.open(dbName);
        create.onupgradeneeded = () => {
          create.result.createObjectStore('foo', {autoIncrement: true});
        };
        const db = await promisify(create);
        const trans = db.transaction('foo', 'readwrite');
        const store = trans.objectStore('foo');

        const add = store.add(data);
        dataKey = await promisify(add);

        await promisify(trans);
        db.close();
      }

      {
        const open = indexedDB.open(dbName);
        open.onupgradeneeded = fail
        const db = await promisify(open);
        const trans = db.transaction('foo');
        const store = trans.objectStore('foo');

        const newData = await promisify(store.get(dataKey));
        expectEq(newData, data);

        await promisify(trans);
        db.close();
      }
    });
  });

  function promisify(request) {
    let stack;
    try {
      throw new TypeError('Source');
    } catch (e) {
      stack = e.stack;
    }

    return new Promise((resolve, reject) => {
      if (request instanceof IDBTransaction) {
        request.onerror = () => reject(request.error + '\n' + stack);
        request.oncomplete = resolve;
      } else {
        request.onerror = () => reject(request.error + '\n' + stack);
        request.onsuccess = (event) => resolve(event.target.result);
      }
    });
  }
});
