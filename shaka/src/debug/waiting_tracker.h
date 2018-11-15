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

#ifndef SHAKA_EMBEDDED_DEBUG_WAITING_TRACKER_H_
#define SHAKA_EMBEDDED_DEBUG_WAITING_TRACKER_H_

namespace shaka {

class Thread;
class Waitable;

/**
 * Tracks which threads are waiting for which actions and prints debug info
 * about it.  This is primarily used to detect deadlocks and print out the
 * offending threads and operations.
 */
class WaitingTracker {
 public:
#ifdef DEBUG_DEADLOCKS
  struct TrackerScope {
    TrackerScope(const TrackerScope&) = delete;
    TrackerScope(TrackerScope&&);
    ~TrackerScope();

    TrackerScope& operator=(const TrackerScope&) = delete;
    TrackerScope& operator=(TrackerScope&&);

   private:
    friend WaitingTracker;
    TrackerScope();
    bool exec_;
  };

  static void AddThread(const Thread* thread);
  static void RemoveThread(const Thread* thread);
  static void RemoveWaitable(const Waitable* waiting_on);

  /**
   * Called when a provider thread exits.  This means that if a thread is
   * waiting for something this thread provides that it will wait forever.
   */
  static void ThreadExit();

  /**
   * Called when the provider of an event changes.  This ensures that once an
   * event gets a provider that we will detect any deadlocks caused by threads
   * that are already waiting on the event.
   */
  static void UpdateProvider(const Waitable* waiting_on);

  /**
   * Called when a thread begins waiting on the given mutex.  This returns a
   * scope object that should be destroyed once the thread has acquired the
   * mutex.
   */
  static TrackerScope ThreadWaiting(const Waitable* waiting_on);
#endif

 private:
  WaitingTracker() {}
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_DEBUG_WAITING_TRACKER_H_
