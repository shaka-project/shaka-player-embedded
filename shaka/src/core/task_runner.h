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

#ifndef SHAKA_EMBEDDED_CORE_TASK_RUNNER_H_
#define SHAKA_EMBEDDED_CORE_TASK_RUNNER_H_

#include <glog/logging.h>

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "src/core/ref_ptr.h"
#include "src/debug/mutex.h"
#include "src/debug/thread.h"
#include "src/debug/thread_event.h"
#include "src/util/clock.h"
#include "src/util/utils.h"

namespace shaka {

enum class TaskPriority {
  Timer,
  Internal,
  Events,
  Immediate,
};

namespace impl {

template <typename Func>
using RetOf = typename std::result_of<Func()>::type;

/** Defines a base class for a pending task. */
class PendingTaskBase {
 public:
  PendingTaskBase(const util::Clock* clock, TaskPriority priority,
                  uint64_t delay_ms, int id, bool loop);
  virtual ~PendingTaskBase();

  /** Performs the task. */
  virtual void Call() = 0;

  uint64_t start_ms;
  const uint64_t delay_ms;
  const TaskPriority priority;
  const int id;
  const bool loop;
  // Using an atomic ensures that writes from another thread are flushed to
  // the other threads immediately.
  std::atomic<bool> should_remove;

 private:
  PendingTaskBase(const PendingTaskBase&) = delete;
  PendingTaskBase(PendingTaskBase&&) = delete;
  PendingTaskBase& operator=(const PendingTaskBase&) = delete;
  PendingTaskBase& operator=(PendingTaskBase&&) = delete;
};

/**
 * The implementation of a pending task.  This stores a copy of the object that
 * will be called.  This will also trace the object so it remains alive.  For
 * example, this ensures that a BackingObject remains alive until an event is
 * raised.
 */
template <typename Func>
class PendingTask : public PendingTaskBase {
 public:
  static_assert(!std::is_base_of<memory::Traceable,
                                typename std::decay<Func>::type>::value,
                "Cannot pass Traceable objects to TaskRunner");
  using Ret = typename std::result_of<Func()>::type;

  PendingTask(const util::Clock* clock, Func&& callback,
              const std::string& name, TaskPriority priority, uint64_t delay_ms,
              int id, bool loop)
      : PendingTaskBase(clock, priority, delay_ms, id, loop),
        callback(std::forward<Func>(callback)),
        event(new ThreadEvent<Ret>(name)) {}

  void Call() override {
    // If this were C++17, we could use if-constexpr:
    //
    // if constexpr (std::is_same<Ret, void>::value) {
    //   callback();
    //   event->SignalAllIfNotSet();
    // } else {
    //   event->SignalAllIfNotSet(callback());
    // }

    SetHelper<Func, Ret>::Set(&callback, &event);
  }

  typename std::decay<Func>::type callback;
  std::shared_ptr<ThreadEvent<Ret>> event;

 private:
  template <typename F, typename R>
  struct SetHelper {
    static void Set(typename std::decay<F>::type* callback,
                    std::shared_ptr<ThreadEvent<R>>* event) {
      (*event)->SignalAllIfNotSet((*callback)());
    }
  };
  template <typename F>
  struct SetHelper<F, void> {
    static void Set(typename std::decay<F>::type* callback,
                    std::shared_ptr<ThreadEvent<void>>* event) {
      (*callback)();
      (*event)->SignalAllIfNotSet();
    }
  };
};

template <typename T>
struct FutureResolver {
  template <typename Func>
  static void CallAndResolve(Func&& callback, std::promise<T>* promise) {
    promise->set_value(callback());
  }
};
template <>
struct FutureResolver<void> {
  template <typename Func>
  static void CallAndResolve(Func&& callback, std::promise<void>* promise) {
    callback();
    promise->set_value();
  }
};

}  // namespace impl


/**
 * Schedules and manages tasks to be run on a worker thread.  This manages a
 * background thread to run the tasks.  It is safe to call all these methods
 * from any thread.
 */
class TaskRunner {
 public:
  using RunLoop = std::function<void()>;

  TaskRunner(std::function<void(RunLoop)> wrapper, const util::Clock* clock,
             bool is_worker);
  ~TaskRunner();

  /** @return Whether the background thread is running. */
  bool is_running() const {
    return running_;
  }

  /** @return Whether there are pending tasks. */
  bool HasPendingWork() const;

  /** @return Whether the calling code is running on the worker thread. */
  bool BelongsToCurrentThread() const;


  /**
   * Stops the worker thread.  Can only be called when running.  Will stop any
   * pending tasks and will block until the worker thread is stopped.
   */
  void Stop();

  /** Blocks the calling thread until the worker has no more work to do. */
  void WaitUntilFinished();


  /**
   * If called from the thread this manages, the given callback is invoked
   * synchronously; otherwise it is scheduled as an internal task.
   *
   * @param callback The callback object.
   * @return A future for when the task is completed.
   */
  template <typename Func>
  std::shared_future<impl::RetOf<Func>> InvokeOrSchedule(Func&& callback) {
    using Ret = impl::RetOf<Func>;
    if (BelongsToCurrentThread()) {
      std::promise<Ret> promise;
      impl::FutureResolver<Ret>::CallAndResolve(std::forward<Func>(callback),
                                                &promise);
      return promise.get_future().share();
    } else {
      return AddInternalTask(TaskPriority::Internal, "",
                             std::forward<Func>(callback))
          ->future();
    }
  }

  /**
   * Registers an internal task to be called on the worker thread.  This
   * callback will be given a higher priority than timers.
   *
   * @see AddTimer
   *
   * @param priority The priority of the task.  Higher priority tasks will run
   *   before lower priority tasks even if the higher task is registered later.
   * @param name The name of the new task, used for debugging.
   * @param callback The callback object.
   * @return The task ID and a future that will hold the results.
   */
  template <typename Func>
  std::shared_ptr<ThreadEvent<impl::RetOf<Func>>> AddInternalTask(
      TaskPriority priority, const std::string& name, Func&& callback) {
    DCHECK(priority != TaskPriority::Timer) << "Use AddTimer for timers";

    std::unique_lock<Mutex> lock(mutex_);
    const int id = ++next_id_;
    auto pending_task =
        new impl::PendingTask<Func>(clock_, std::forward<Func>(callback), name,
                                    priority, 0, id, /* loop */ false);
    tasks_.emplace_back(pending_task);
    pending_task->event->SetProvider(&worker_);

    return pending_task->event;
  }

  /**
   * Calls the given callback after the given delay on the worker thread.
   *
   * @param delay_ms The time to wait until the callback is called (in
   *   milliseconds).
   * @param callback The callback object.
   * @return The task ID.
   */
  template <typename Func>
  int AddTimer(uint64_t delay_ms, Func&& callback) {
    std::unique_lock<Mutex> lock(mutex_);
    const int id = ++next_id_;

    tasks_.emplace_back(
        new impl::PendingTask<Func>(clock_, std::forward<Func>(callback), "",
                                    TaskPriority::Timer, delay_ms, id,
                                    /* loop= */ false));

    return id;
  }

  /**
   * Calls the given callback every |delay_ms| milliseconds on the worker
   * thread.
   *
   * @see AddTimer
   *
   * @param delay_ms The time to wait until the callback is called (in
   *   milliseconds).
   * @param callback The callback object.
   * @return The task ID.
   */
  template <typename Func>
  int AddRepeatedTimer(uint64_t delay_ms, Func&& callback) {
    std::unique_lock<Mutex> lock(mutex_);
    const int id = ++next_id_;

    tasks_.emplace_back(
        new impl::PendingTask<Func>(clock_, std::forward<Func>(callback), "",
                                    TaskPriority::Timer, delay_ms, id,
                                    /* loop= */ true));

    return id;
  }

  /** Cancels a pending timer with the given ID. */
  void CancelTimer(int id);

 private:
  TaskRunner(const TaskRunner&) = delete;
  TaskRunner(TaskRunner&&) = delete;
  TaskRunner& operator=(const TaskRunner&) = delete;
  TaskRunner& operator=(TaskRunner&&) = delete;

  /**
   * Called from the worker thread.  Should loop until |running_| is false,
   * waiting for work to do.
   */
  void Run(std::function<void(RunLoop)> wrapper);

  /**
   * Called when there is no work to be done.  This should yield to other
   * threads and/or wait until more work is scheduled.
   */
  void OnIdle();

  /**
   * Pops a task from the queue and handles it.
   * @return True if there were any task in the queue, false otherwise.
   */
  bool HandleTask();


  // TODO: Consider a different data structure.
  std::list<std::unique_ptr<impl::PendingTaskBase>> tasks_;

  mutable Mutex mutex_;
  const util::Clock* clock_;
  ThreadEvent<void> waiting_;
  std::atomic<bool> running_;
  int next_id_;
  bool is_worker_;

  Thread worker_;
};

}  // namespace shaka

#endif  // SHAKA_EMBEDDED_CORE_TASK_RUNNER_H_
