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
#include "src/memory/heap_tracer.h"
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
class PendingTaskBase : public memory::Traceable {
 public:
  PendingTaskBase(const util::Clock* clock, TaskPriority priority,
                  uint64_t delay_ms, int id, bool loop);
  ~PendingTaskBase() override;

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
  static_assert(std::is_base_of<memory::Traceable,
                                typename std::decay<Func>::type>::value,
                "Traceable callback object must be Traceable");
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

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&callback);
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

template <typename Func>
class PlainCallbackTaskImpl : public memory::Traceable {
 public:
  using func_type = typename std::decay<Func>::type;
  using return_type = decltype(std::declval<func_type>()());

  explicit PlainCallbackTaskImpl(Func&& callback)
      : callback_(std::forward<Func>(callback)) {}

  void Trace(memory::HeapTracer*) const override {}

  return_type operator()() {
    return callback_();
  }

 private:
  func_type callback_;
};

template <typename This, typename Member>
class MemberCallbackTaskImpl : public memory::Traceable {
 public:
  using return_type =
      decltype((std::declval<This*>()->*std::declval<Member>())());

  MemberCallbackTaskImpl(RefPtr<This> that, Member member)
      : that_(that), member_(member) {}

  void Trace(memory::HeapTracer* tracer) const override {
    tracer->Trace(&that_);
  }

  return_type operator()() {
    return (that_->*member_)();
  }

 private:
  ::shaka::Member<This> that_;
  Member member_;
};

}  // namespace impl

/**
 * Creates a task that is backed by a simple C++ callback function.  This should
 * be used when simply calling another function that doesn't need to be traced.
 * This will NOT keep things alive (except for the callback itself), so this
 * should not be used for JavaScript objects.
 */
template <typename Func>
impl::PlainCallbackTaskImpl<Func> PlainCallbackTask(Func&& callback) {
  // Use a function so we can use argument deduction to deduce |Func|; using the
  // constructor directly would require passing the type argument explicitly.
  return impl::PlainCallbackTaskImpl<Func>(std::forward<Func>(callback));
}

/**
 * Creates a task that traces the given object and then calls the given member
 * function on it.
 */
template <typename That, typename Member>
impl::MemberCallbackTaskImpl<That, Member> MemberCallbackTask(That* that,
                                                              Member member) {
  return impl::MemberCallbackTaskImpl<That, Member>(that, member);
}


/**
 * Schedules and manages tasks to be run on a worker thread.  This manages a
 * background thread to run the tasks.  It is safe to call all these methods
 * from any thread.
 *
 * This also tracks object lifetimes to make sure BackingObjects are not freed
 * after a callback has been scheduled.
 */
class TaskRunner : public memory::Traceable {
 public:
  using RunLoop = std::function<void()>;

  TaskRunner(std::function<void(RunLoop)> wrapper, const util::Clock* clock,
             bool is_worker);
  ~TaskRunner() override;

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

  void Trace(memory::HeapTracer* tracer) const override;


  /**
   * Registers an internal task to be called on the worker thread.  This
   * callback will be given a higher priority than timers.
   *
   * @see AddTimer
   *
   * @param priority The priority of the task.  Higher priority tasks will run
   *   before lower priority tasks even if the higher task is registered later.
   * @param name The name of the new task, used for debugging.
   * @param callback The Traceable callback object.
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
   * Calls the given callback after the given delay on the worker thread.  The
   * given callback must also be a Traceable object.  The callback will be
   * traced to keep any JavaScript objects alive.  So the |Func| type must
   * inherit from Traceable and must define a call operator.
   *
   * @param delay_ms The time to wait until the callback is called (in
   *   milliseconds).
   * @param callback The Traceable callback object.
   * @return The task ID.
   */
  template <typename Func>
  int AddTimer(uint64_t delay_ms, Func&& callback) {
    std::unique_lock<Mutex> lock(mutex_);
    const int id = ++next_id_;

    tasks_.emplace_back(
        new impl::PendingTask<Func>(clock_, std::forward<Func>(callback), "",
                                    TaskPriority::Timer, delay_ms, id,
                                    /* loop */ false));

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
   * @param callback The Traceable callback object.
   * @return The task ID.
   */
  template <typename Func>
  int AddRepeatedTimer(uint64_t delay_ms, Func&& callback) {
    std::unique_lock<Mutex> lock(mutex_);
    const int id = ++next_id_;

    tasks_.emplace_back(
        new impl::PendingTask<Func>(clock_, std::forward<Func>(callback), "",
                                    TaskPriority::Timer, delay_ms, id,
                                    /* loop */ true));

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
