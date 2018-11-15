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

describe('events', function() {
  var event_target;
  var on_listener;
  var add_listener;

  beforeEach(function() {
    // Note: We use XMLHttpRequest as an event target since the EventTarget
    // interface is not exposed on window.
    event_target = new XMLHttpRequest();

    add_listener = jasmine.createSpy('addEventListener error');
    on_listener = jasmine.createSpy('onerror');
    event_target.onerror = on_listener;
    event_target.addEventListener('error', add_listener);
  });

  it('gives correct inheritance results', function() {
    var event = new Event('error');
    expect(event instanceof Event).toBe(true);
    expect(event instanceof ProgressEvent).toBe(false);

    event = new ProgressEvent('foobar');
    expect(event instanceof Event).toBe(true);
    expect(event instanceof ProgressEvent).toBe(true);
    expect(event instanceof XMLHttpRequest).toBe(false);
    expect(event.type).toBe('foobar');

    // Check that a subclass is accepted as an argument.
    event_target.dispatchEvent(event);
  });

  it('dispatches events', function() {
    expect(event_target.dispatchEvent(new Event('error'))).toBe(true);

    expect(on_listener).toHaveBeenCalled();
    expect(add_listener).toHaveBeenCalled();
  });

  it('only calls listeners for the raised events', function() {
    var extra_listener = jasmine.createSpy('extra');
    event_target.addEventListener('extra_event', extra_listener);

    event_target.dispatchEvent(new Event('error'));

    expect(add_listener).toHaveBeenCalled();
    expect(extra_listener).not.toHaveBeenCalled();

    add_listener.calls.reset();
    extra_listener.calls.reset();

    event_target.dispatchEvent(new Event('extra_event'));

    expect(add_listener).not.toHaveBeenCalled();
    expect(extra_listener).toHaveBeenCalled();
  });

  it('won\'t dispatch to on- listener when field is cleared', function() {
    event_target.onerror = null;

    event_target.dispatchEvent(new Event('error'));

    expect(on_listener).not.toHaveBeenCalled();
    expect(add_listener).toHaveBeenCalled();
  });

  it('won\'t dispatch to listeners added in listeners', function() {
    var extra_listener = jasmine.createSpy('extra');
    add_listener.and.callFake(function() {
      // Even though we have added this listener, it should not be called.
      event_target.addEventListener('error', extra_listener);
    });

    event_target.dispatchEvent(new Event('error'));

    expect(add_listener).toHaveBeenCalled();
    expect(extra_listener).not.toHaveBeenCalled();
  });

  it('won\'t dispatch to listeners removed in listeners', function() {
    var extra_listener = jasmine.createSpy('extra');
    event_target.addEventListener('error', extra_listener);
    add_listener.and.callFake(function() {
      expect(extra_listener).not.toHaveBeenCalled();
      event_target.removeEventListener('error', extra_listener);
    });

    event_target.dispatchEvent(new Event('error'));

    expect(add_listener).toHaveBeenCalled();
    // Even though it was added before, it was removed before it was called.
    expect(extra_listener).not.toHaveBeenCalled();
  });

  describe('stopImmediatePropagation', function() {
    it('stops remaining listeners', function() {
      add_listener.and.callFake(function(evt) {
        evt.stopImmediatePropagation();
      });
      var extra_listener = jasmine.createSpy('extra listener');
      event_target.addEventListener('error', extra_listener);

      event_target.dispatchEvent(new Event('error'));
      expect(add_listener).toHaveBeenCalled();
      expect(extra_listener).not.toHaveBeenCalled();
    });

    it('stops from on- listeners', function() {
      // We define on- listeners to fire before addEventListener listeners.
      // This may be different on some browsers.
      on_listener.and.callFake(function(evt) {
        evt.stopImmediatePropagation();
      });

      event_target.dispatchEvent(new Event('error'));
      expect(on_listener).toHaveBeenCalled();
      expect(add_listener).not.toHaveBeenCalled();
    });
  });

  describe('exceptions', function() {
    it('will throw if calling dispatchEvent from a listener', function() {
      add_listener.and.callFake(function() {
        try {
          // Calling dispatchEvent when the target is already dispatching events
          // is not allowed:
          // https://dom.spec.whatwg.org/#dom-eventtarget-dispatchevent
          event_target.dispatchEvent(new Event('other_event'));
          fail('Should throw an exception');
        } catch (e) {
          expect(e.name).toBe('InvalidStateError');
        }
      });
      event_target.dispatchEvent(new Event('error'));
      expect(add_listener).toHaveBeenCalled();
    });

    it('will swallow exceptions from listeners', function() {
      add_listener.and.callFake(function() {
        // This should cause log messages, but since it does not use
        // console.log, we have no way to verify that.
        throw 'An error';
      });

      var dispatch = function() {
        event_target.dispatchEvent(new Event('error'));
      };
      expect(dispatch).not.toThrow();
      expect(add_listener).toHaveBeenCalled();
    });
  });
});
