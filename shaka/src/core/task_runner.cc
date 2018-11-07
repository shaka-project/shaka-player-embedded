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

#include "src/core/task_runner.h"

#include <glog/logging.h>

#include <chrono>
#include <limits>

#include "src/mapping/js_wrappers.h"

namespace shaka {

namespace impl {

PendingTaskBase::PendingTaskBase(const util::Clock* clock,
                                 TaskPriority priority, uint64_t delay_ms,
                                 int id, bool loop)
    : start_ms(clock->GetMonotonicTime()),
      delay_ms(delay_ms),
      priority(priority),
      id(id),
      loop(loop),
      should_remove(false) {}

PendingTaskBase::~PendingTaskBase() {}

}  // namespace impl

TaskRunner::TaskRunner(std::function<void(RunLoop)> wrapper,
                       const util::Clock* clock, bool is_worker)
    : mutex_(is_worker ? "TaskRunner worker" : "TaskRunner main"),
      clock_(clock),
      waiting_("TaskRunner wait until finished"),
      running_(true),
      next_id_(0),
      is_worker_(is_worker),
      worker_(is_worker ? "JS Worker" : "JS Main Thread",
              std::bind(&TaskRunner::Run, this, std::move(wrapper))) {
  waiting_.SetProvider(&worker_);
}

TaskRunner::~TaskRunner() {
  Stop();
}

bool TaskRunner::HasPendingWork() const {
  std::unique_lock<Mutex> lock(mutex_);
  for (auto& task : tasks_) {
    if (!task->loop)
      return true;
  }
  return false;
}

bool TaskRunner::BelongsToCurrentThread() const {
  return running_ && std::this_thread::get_id() == worker_.get_id();
}

void TaskRunner::Stop() {
  bool join = false;
  {
    std::unique_lock<Mutex> lock(mutex_);
    if (running_) {
      running_ = false;
      join = true;
      waiting_.SignalAllIfNotSet();
    }
  }
  if (join) {
    worker_.join();
  }
}

void TaskRunner::WaitUntilFinished() {
  if (running_ && HasPendingWork()) {
    std::unique_lock<Mutex> lock(mutex_);
    waiting_.ResetAndWaitWhileUnlocked(lock);
  }
}

void TaskRunner::Trace(memory::HeapTracer* tracer) const {
  std::unique_lock<Mutex> lock(mutex_);
  for (auto& task : tasks_) {
    DCHECK(task);
    task->Trace(tracer);
  }
}

void TaskRunner::CancelTimer(int id) {
  std::unique_lock<Mutex> lock(mutex_);
  for (auto& task : tasks_) {
    if (task->id == id) {
      task->should_remove = true;
      return;
    }
  }
}

void TaskRunner::Run(std::function<void(RunLoop)> wrapper) {
  wrapper([this]() {
    while (running_) {
      // Handle a task.  This will only handle one task, then loop.
      if (HandleTask())
        continue;

      if (!HasPendingWork()) {
        waiting_.SignalAllIfNotSet();
      }

      // We don't have any work to do, wait for a while.
      OnIdle();
    }

    // If we stop early, delete any pending tasks.  This must be done on the
    // worker thread so we can delete JavaScript objects.
    tasks_.clear();
    waiting_.SignalAllIfNotSet();
  });
}

void TaskRunner::OnIdle() {
  // TODO: Since we have no work, we will never add work ourselves.  Consider
  // using signalling rather than polling.
  clock_->SleepSeconds(0.001);
}

bool TaskRunner::HandleTask() {
  // We need to be careful here because:
  // 1) We may be called from another thread to change tasks.
  // 2) The callback may change tasks (including its own).

  const uint64_t now = clock_->GetMonotonicTime();
  impl::PendingTaskBase* task = nullptr;
  {
    // Find the earliest timer we can finish.  If there are multiple with the
    // same time, pick the one registered earlier (lower ID).
    std::unique_lock<Mutex> lock(mutex_);
    uint64_t min_time = std::numeric_limits<uint64_t>::max();
    TaskPriority max_priority = TaskPriority::Timer;
    for (auto it = tasks_.begin(); it != tasks_.end();) {
      if ((*it)->should_remove) {
        it = tasks_.erase(it);
      } else {
        if ((*it)->priority > max_priority) {
          max_priority = (*it)->priority;
          task = it->get();
        } else if (max_priority == TaskPriority::Timer) {
          const uint64_t it_time = (*it)->start_ms + (*it)->delay_ms;
          if (it_time <= now && it_time < min_time) {
            min_time = it_time;
            task = it->get();
          }
        }
        ++it;
      }
    }
  }

  if (!task)
    return false;

#ifdef USING_V8
  if (!is_worker_) {
    // V8 attaches v8::Local<T> instances to the most recent v8::HandleScope
    // instance.  By having a scope here, the task can create local handles and
    // they will be freed after the task finishes.
    v8::HandleScope handles(GetIsolate());
    task->Call();
  } else {
    task->Call();
  }
#else
  // Other JavaScript engines use ref-counting for local handles.
  task->Call();
  (void)is_worker_;
#endif

  if (task->loop) {
    task->start_ms = now;
  } else {
    task->should_remove = true;
    // Will be removed in the next iteration.
  }
  return true;
}

}  // namespace shaka
