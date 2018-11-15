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

// TODO(b/63342916): Enable tests once we get a stable, local web server.
testGroup('XMLHttpRequest', function() {
  // TODO: Consider using a different endpoint for testing.

  xtest('BasicGetRequests', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      xhr.open('GET', 'https://httpbin.org/get');
      xhr.onload = function() {
        expectInstanceOf(xhr.response, ArrayBuffer);
        expectInstanceOf(xhr.responseText, String);
        expectTrue(xhr.responseText);
        expectEq(xhr.status, 200);
        expectEq(xhr.statusText, 'OK');
        resolve();
      };
      xhr.send();
    });
  });

  xtest('ParsesResponseHeaders', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      xhr.open('GET',
               'https://httpbin.org/response-headers?' +
               'Content-Type=text/special&Extra-Header=foobar');
      xhr.onload = function() {
        expectEq(xhr.status, 200);
        expectEq(xhr.getResponseHeader('foobarbaz'), null);
        expectEq(xhr.getResponseHeader('content-type'),
                 'application/json, text/special');
        expectEq(xhr.getResponseHeader('extra-header'), 'foobar');
        expectEq(parseInt(xhr.getResponseHeader('content-length')),
                 xhr.response.byteLength);
        resolve();
      };
      xhr.send();
    });
  });

  xtest('SendsRequestHeaders', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      xhr.open('GET', 'https://httpbin.org/get');
      xhr.setRequestHeader('The-Answer', '42');
      xhr.setRequestHeader('Accept-Language', 'en');
      xhr.onload = function() {
        expectEq(xhr.status, 200);
        let obj = JSON.parse(xhr.responseText);
        expectEq(obj.headers['The-Answer'], '42');
        expectEq(obj.headers['Accept-Language'], 'en');
        resolve();
      };
      xhr.send();
    });
  });

  xtest('SupportsRedirects', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      var dest = 'https://httpbin.org/response-headers?' +
                 'Content-Type=text/special&Extra-Header=foobar';
      xhr.open(
          'GET',
          'https://httpbin.org/redirect-to?url=' + encodeURIComponent(dest));
      xhr.onload = function() {
        expectEq(xhr.status, 200);
        expectEq(xhr.responseURL, dest);
        expectEq(xhr.getResponseHeader('content-type'),
                 'application/json, text/special');
        expectEq(xhr.getResponseHeader('extra-header'), 'foobar');
        // We should not see the header from the redirect response.
        expectEq(xhr.getResponseHeader('location'), null);
        resolve();
      };
      xhr.send();
    });
  });

  xtest('SupportsCookies', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      let value = '' + Math.floor(Math.random() * 50);
      xhr.open('GET',
               'https://httpbin.org/cookies/set?key=value&name=' + value);
      xhr.onload = function() {
        expectEq(xhr.status, 200);

        // Send another request to see the stored cookies. Use another instance
        // so it has to load the cookie file.
        xhr = new XMLHttpRequest();
        xhr.open('GET', 'https://httpbin.org/cookies');
        xhr.onload = function() {
          let obj = JSON.parse(xhr.responseText);
          expectEq(obj.cookies['key'], 'value');
          expectEq(obj.cookies['name'], value);
          expectEq(obj.cookies['missing'], undefined);
          resolve();
        };
        xhr.send();
      };
      xhr.send();
    });
  });

  xtest('SupportsUsernamePassword', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();
      xhr.onabort = xhr.onerror = xhr.ontimeout = fail;

      xhr.open('GET', 'https://httpbin.org/basic-auth/user/pass', true, 'user',
               'pass');
      xhr.onload = function() {
        expectEq(xhr.status, 200);
        resolve();
      };
      xhr.send();
    });
  });

  xtest('WillTimeout', function() {
    return new Promise((resolve) => {
      let xhr = new XMLHttpRequest();

      // The server will wait 8 seconds to respond.
      var timeout = jasmine.createSpy('ontimeout');
      xhr.open('GET', 'https://httpbin.org/delay/8');
      xhr.timeout = 1;  // in milliseconds.
      xhr.ontimeout = timeout;
      xhr.onload = fail;
      xhr.onloadend = function() {
        expectToHaveBeenCalled(timeout);
        expectEq(xhr.status, 0);
        expectEq(xhr.readyState, 4);  // DONE
        resolve();
      };
      xhr.send();
    });
  });

  testGroup('abort', function() {
    xtest('Synchronously', function() {
      return new Promise((resolve) => {
        let xhr = new XMLHttpRequest();

        var abort = jasmine.createSpy('onabort');
        xhr.open('GET', 'https://httpbin.org/get');
        xhr.onabort = abort;
        xhr.onload = fail;
        xhr.onloadend = function() {
          expectToHaveBeenCalled(abort);
          expectEq(xhr.status, 0);
          expectEq(xhr.readyState, 4);  // DONE
        };
        xhr.send();
        xhr.abort();

        expectEq(xhr.readyState, 0);  // UNSENT
        // abort() should raise events synchronously.
        expectToHaveBeenCalled(abort);

        // Make sure we don't get any more events.
        abort.calls.reset();
        setTimeout(function() {
          expectNotToHaveBeenCalled(abort);
          resolve();
        }, 500);
      });
    });

    xtest('WhileWaiting', function() {
      return new Promise((resolve) => {
        let xhr = new XMLHttpRequest();

        // The server will wait 2 seconds to respond.
        let abort = jasmine.createSpy('onabort');
        xhr.open('GET', 'https://httpbin.org/delay/2');
        xhr.onabort = abort;
        xhr.onload = fail;
        xhr.onloadend = function() {
          expectToHaveBeenCalled(abort);
          expectEq(xhr.status, 0);
          expectEq(xhr.readyState, 4);  // DONE
        };
        xhr.send();

        setTimeout(function() {
          xhr.abort();

          expectEq(xhr.readyState, 0);  // UNSENT
          // abort() should raise events synchronously.
          expectToHaveBeenCalled(abort);

          // Make sure we don't get any more events.
          abort.calls.reset();
          setTimeout(function() {
            expectNotToHaveBeenCalled(abort);
            resolve();
          }, 500);
        }, 500);
      });
    });
  });

  testGroup('POST', function() {
    const postData = 'post data\0with&% extras;';

    xtest('string', function() {
      return sendPost(postData);
    });

    xtest('ArrayBuffer', function() {
      // Create an array of numbers of the char codes.
      var arr = postData.split('').map(function(s) { return s.charCodeAt(0); });
      return sendPost(new Uint8Array(arr).buffer);
    });

    xtest('ArrayBufferView', function() {
      // Create an array of numbers of the char codes.
      var arr = postData.split('').map(function(s) { return s.charCodeAt(0); });
      return sendPost(new Uint8Array(arr));
    });

    /**
     * @param {string|BufferSource} dataSend
     * @param {number=} opt_retriedTimes
     * @return {!Promise}
     */
    function sendPost(dataSend, opt_retriedTimes) {
      // TODO(b/63342916): Sometimes the server will respond with a 408 Request
      // Timeout even when it is given all the data quickly. Simply retry.
      var retriedTimes = opt_retriedTimes || 0;
      return new Promise(function(resolve, reject) {
        var xhr = new XMLHttpRequest();
        xhr.open('POST', 'https://httpbin.org/post');

        xhr.onabort = xhr.onerror = xhr.ontimeout = reject;
        xhr.onload = function() {
          if (xhr.status == 408 && retriedTimes < 3) {
            console.log('Request timed-out, retrying');
            sendPost(dataSend, retriedTimes + 1).then(resolve, reject);
          } else if (xhr.status >= 200 && xhr.status <= 299) {
            var obj = JSON.parse(xhr.responseText);
            expectEq(obj.data, postData);
            resolve();
          } else {
            reject('Bad status code ' + xhr.status);
          }
        };

        xhr.send(dataSend);
      });
    }
  });
});
