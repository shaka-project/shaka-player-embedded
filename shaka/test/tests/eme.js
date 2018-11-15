// Copyright 2017 Google LLC
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

testGroup('EME', function() {
  const testKeySystem = 'org.w3.clearkey';

  testGroup('requestMediaKeySystemAccess', function() {
    testGroup('errors', function() {
      test('RejectsWhenArgumentsMissing', async function() {
        await expectAllFailedWith('TypeError', [
          navigator.requestMediaKeySystemAccess(),
          navigator.requestMediaKeySystemAccess(testKeySystem)
        ]);
      });

      test('RejectsForIncorrectTypes', async function() {
        await expectAllFailedWith('TypeError', [
          navigator.requestMediaKeySystemAccess(123, []),
          navigator.requestMediaKeySystemAccess(testKeySystem, ''),
          navigator.requestMediaKeySystemAccess(testKeySystem, 123)
        ]);
      });

      test('RejectsForEmptyKeySystem', async function() {
        await expectAllFailedWith('TypeError', [
          navigator.requestMediaKeySystemAccess('', [emptyConfig()])
        ]);
      });

      test('RejectsForEmptyConfig', async function() {
        await expectAllFailedWith('TypeError', [
          navigator.requestMediaKeySystemAccess(testKeySystem, [])
        ]);
      });

      test('RejectsForUnsupportedKeySystem', async function() {
        await expectAllFailedWith('NotSupportedError', [
          navigator.requestMediaKeySystemAccess('unsupported', [emptyConfig()])
        ]);
      });

      /**
       * @param {string} type
       * @param {!Array.<Promise>} promises
       * @return {!Promise}
       */
      function expectAllFailedWith(type, promises) {
        return Promise.all(promises.map(function(p) {
          expectInstanceOf(p, Promise);
          return p.then(function() {
            fail('Promise should be rejected');
          }).catch(function(error) {
            expectTrue(error);
            expectEq(error.name, type);
          });
        }));
      }
    });

    testGroup('success', function() {
      test('ResolvesPromiseWhenSupported', async function() {
        let access = await navigator.requestMediaKeySystemAccess(
            testKeySystem, [emptyConfig()]);
        expectTrue(access);
      });

      test('ContainsTheKeySystemName', async function() {
        let access = await navigator.requestMediaKeySystemAccess(
            testKeySystem, [emptyConfig()]);
        expectTrue(access);
        expectEq(access.keySystem, testKeySystem);
      });

      testGroup('getConfiguration', function() {
        test('ReturnsTheSupportedConfig', async function() {
          let config = emptyConfig();
          config.label = 'foobar';

          let access = await navigator.requestMediaKeySystemAccess(
              testKeySystem, [config]);
          expectTrue(access);
          expectEq(access.getConfiguration(), config);
        });
      });
    });
  });

  testGroup('ClearKey', function() {
    const keyIdBase64 = 'MTIzNDU2Nzg5MDEyMzQ1Ng';
    const keyId = makeBuffer('1234567890123456');
    const keyBase64 = 'OTk5OTk5OTk5OTk5OTk5OQ';

    const keyId2Base64 = 'AAAAAAAAAAAAAAAAAAAAAA';
    const psshBytes = [
      0x00, 0x00, 0x00, 0x44,  // box size
      0x70, 0x73, 0x73, 0x68,  // box type 'pssh'
      0x01, 0x00, 0x00, 0x00,  // version + flags
      0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,  // system ID
      0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,

      0x00, 0x00, 0x00, 0x02,  // key id count
      0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,  // key ID 0
      0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // key ID 1
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

      0x00, 0x00, 0x00, 0x00  // data size
    ];
    const otherPsshBytes = [
      0x00, 0x00, 0x00, 0x24,  // box size
      0x70, 0x73, 0x73, 0x68,  // box type 'pssh'
      0x00, 0x00, 0x00, 0x00,  // version + flags
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // system ID
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

      0x00, 0x00, 0x00, 0x04,  // data size
      0x01, 0x02, 0x03, 0x04,  // data
    ];

    test('BasicFlow', async function() {
      let access = await navigator.requestMediaKeySystemAccess(
          testKeySystem, [emptyConfig()]);
      let mediaKeys = await access.createMediaKeys();
      expectTrue(mediaKeys);
      expectInstanceOf(mediaKeys, MediaKeys);

      let session = mediaKeys.createSession();
      expectTrue(session);
      expectInstanceOf(session, MediaKeySession);

      let onMessage = jasmine.createSpy('onmessage');
      let onKeyChange = jasmine.createSpy('onkeystatuseschange');
      session.onmessage = onMessage;
      session.onkeystatuseschange = onKeyChange;

      const initData = makeBuffer('{"kids":["' + keyIdBase64 + '"]}');
      await session.generateRequest('keyids', initData);

      expectToHaveBeenCalledTimes(onMessage, 1);
      let msgEvent = onMessage.calls.argsFor(0)[0];
      expectEq(msgEvent.messageType, 'license-request');
      let requestObj = JSON.parse(makeString(msgEvent.message));
      expectEq(requestObj.type, 'temporary');
      expectEq(requestObj.kids, [keyIdBase64]);

      let responseObj = {
        keys: [{kty: 'oct', k: keyBase64, kid: keyIdBase64}],
        type: 'temporary',
      };
      let response = makeBuffer(JSON.stringify(responseObj));
      onKeyChange.calls.reset();
      await session.update(response);
      expectToHaveBeenCalled(onKeyChange);

      let keyStatuses = session.keyStatuses;
      expectTrue(keyStatuses);
      let callback = jasmine.createSpy('forEach').and.callFake((status, id) => {
        expectEq(makeString(id), makeString(keyId));
        expectEq(status, 'usable');
      });
      keyStatuses.forEach(callback);
      expectToHaveBeenCalledTimes(callback, 1);

      await session.close();
      await session.closed;
    });

    test('CencInitializationData', async function() {
      let access = await navigator.requestMediaKeySystemAccess(
          testKeySystem, [emptyConfig()]);
      let mediaKeys = await access.createMediaKeys();
      let session = mediaKeys.createSession();

      let onMessage = jasmine.createSpy('onmessage');
      session.onmessage = onMessage;

      await session.generateRequest('cenc', new Uint8Array(psshBytes));

      expectToHaveBeenCalledTimes(onMessage, 1);
      let msgEvent = onMessage.calls.argsFor(0)[0];
      expectEq(msgEvent.messageType, 'license-request');

      let obj = JSON.parse(makeString(msgEvent.message));
      expectEq(obj.kids, [keyIdBase64, keyId2Base64]);
    });

    test('CencWithMultiplePsshBoxes', async function() {
      let access = await navigator.requestMediaKeySystemAccess(
          testKeySystem, [emptyConfig()]);
      let mediaKeys = await access.createMediaKeys();
      let session = mediaKeys.createSession();

      let onMessage = jasmine.createSpy('onmessage');
      session.onmessage = onMessage;

      await session.generateRequest(
          'cenc', new Uint8Array(otherPsshBytes.concat(psshBytes)));

      expectToHaveBeenCalledTimes(onMessage, 1);
      let msgEvent = onMessage.calls.argsFor(0)[0];
      expectEq(msgEvent.messageType, 'license-request');

      let obj = JSON.parse(makeString(msgEvent.message));
      expectEq(obj.kids, [keyIdBase64, keyId2Base64]);
    });

    /**
     * @param {string} data
     * @return {!ArrayBuffer}
     */
    function makeBuffer(data) {
      let buffer = new Uint8Array(data.length);
      for (let i = 0; i < data.length; i++)
        buffer[i] = data.charCodeAt(i);
      return buffer.buffer;
    }

    function makeString(buffer) {
      let array = new Uint8Array(buffer);
      return String.fromCharCode.apply(null, array);
    }
  });

  function emptyConfig() {
    return {
      label: '',
      initDataTypes: ['keyids'],
      audioCapabilities: [],
      videoCapabilities: [{contentType: 'video/mp4', robustness: ''}],
      distinctiveIdentifier: 'not-allowed',
      persistentState: 'not-allowed',
      sessionTypes: ['temporary']
    };
  }
});
