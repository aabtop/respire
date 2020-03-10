#include "thread_pool.h"
#include <iostream>

namespace ebb {

ThreadPool::Task::Task(
    ThreadPool* thread_pool, const std::function<void()>& function)
    : thread_pool_(thread_pool), function_(function), ready_queue_node_(this) {
  thread_pool->EnqueueReadyTask(this);
}

ThreadPool::ThreadPool(int num_threads, size_t stack_size,
                       SchedulingPolicy scheduling_policy)
    : quit_(false), stack_size_(stack_size),
      scheduling_policy_(scheduling_policy) {
  for (int i = 0; i < num_threads; ++i) {
    threads_.emplace_back(&ThreadPool::ThreadStart, this, i);
    thread_contexts_.emplace_back(nullptr);
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    quit_ = true;
    event_available_.notify_all();
  }

  for (auto& thread : threads_) {
    thread.join();
  }
}

namespace {
thread_local ThreadPool* tl_my_thread_pool = nullptr;

struct ExtraContextInfo {
  bool add_to_idle_queue;
  LinkedList<platform::Context>::Node* context_list_node;
  std::unique_lock<std::mutex>* lock;
};

}  // namespace

bool ThreadPool::IsCurrentThreadInPool() const {
  return tl_my_thread_pool == this;
}

void ThreadPool::WaitForEvent() {
  std::unique_lock<std::mutex> lock(mutex_);

  while (!quit_ && ready_queue_.empty() && resume_queue_.empty()) {
    event_available_.wait(lock);
  }
}

platform::Context* ThreadPool::RunLoop(RunLoopPolicy policy) {
  bool should_dequeue_idle_contexts = (policy == kRunLoopPolicy_Fiber);
  while(true) {
    WaitForEvent();

    // First check if there are any pending contexts that are ready to be
    // resumed.
    while (platform::Context* ready_context =
        DequeueReadyContext(should_dequeue_idle_contexts)) {
      if (policy == kRunLoopPolicy_Fiber) {
        // If we have a fiber entry point on the callstack, just return and let
        // the context.h API take care of switching to the returned context.
        return ready_context;
      } else {
        // If we have a thread entry point on the callstack, never return the
        // next context to run, instead just run it and put the current context
        // on the idle queue, where it will stay until a fiber context is ready
        // to switch with is.
        ContextList::Node context_list_node(nullptr);
        ExtraContextInfo this_context_info;
        this_context_info.add_to_idle_queue = true;
        this_context_info.context_list_node = &context_list_node;
        this_context_info.lock = nullptr;

        platform::Context* previous_context = SwitchToContext(
            ready_context, &this_context_info);
        OnFiberSuspended(previous_context);
      }
    }

    Task* task = DequeueReadyTask();

    if (task) {
      num_contexts_ += 1;
      num_simultaneous_hwm_ = num_contexts_ > num_simultaneous_hwm_ ?
                                  num_contexts_ : num_simultaneous_hwm_;

      task->function_();

      num_contexts_ -= 1;
    } else {
      if (quit_) {
        // All fibers should have been cleaned up by now.
        break;
      }
    }
  }

  if (policy == kRunLoopPolicy_Fiber) {
    return DequeueReadyContext(true);
  }

  return nullptr;
}

void ThreadPool::EnqueueReadyTask(Task* task) {
  std::lock_guard<std::mutex> lock(mutex_);

  ready_queue_.push_back(&task->ready_queue_node_);
  event_available_.notify_one();
}

ThreadPool::Task* ThreadPool::DequeueReadyTask() {
  std::unique_lock<std::mutex> lock(mutex_);

  if (!ready_queue_.empty()) {
    Task* task;
    switch(scheduling_policy_) {
      case SchedulingPolicy::FIFO: {
        task = ready_queue_.front()->item();
        ready_queue_.pop_front();
      } break;
      case SchedulingPolicy::LIFO: {
        task = ready_queue_.back()->item();
        ready_queue_.pop_back();
      }
    }
    return task;
  } else {
    return nullptr;
  }
}

platform::Context* ThreadPool::DequeueReadyContext(bool dequeue_idle_contexts) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!resume_queue_.empty()) {
    platform::Context* context = resume_queue_.front()->item();
    resume_queue_.pop_front();
    return context;
  } else if (dequeue_idle_contexts && !idle_queue_.empty()) {
    platform::Context* context = idle_queue_.front()->item();
    idle_queue_.pop_front();
    return context;
  }

  return nullptr;
}

void ThreadPool::ThreadStart(int local_thread_id) {
  tl_my_thread_pool = this;
  platform::Context* next_context = RunLoop(kRunLoopPolicy_Thread);
  assert(!next_context);

  ReturnThreadToOriginalContext(local_thread_id);
}

platform::Context* ThreadPool::FiberStart(platform::Context* previous_context) {
  OnFiberSuspended(previous_context);
  auto ret = RunLoop(kRunLoopPolicy_Fiber);
  return ret;
}

void ThreadPool::OnFiberSuspended(platform::Context* suspended_context) {
  if (!suspended_context) {
    return;
  }

  ExtraContextInfo* suspended_context_info =
      reinterpret_cast<ExtraContextInfo*>(GetContextData(suspended_context));
  suspended_context_info->context_list_node->set_item(suspended_context);
  if (suspended_context_info->add_to_idle_queue) {
    std::lock_guard<std::mutex> lock(mutex_);
    idle_queue_.push_back(suspended_context_info->context_list_node); 
  }
  if (suspended_context_info->lock) {
    std::unique_lock<std::mutex> lock(std::move(*suspended_context_info->lock));
    assert(lock);
  }
}

void ThreadPool::SleepCurrentContext(ContextList::Node* context_list_node,
                                     std::unique_lock<std::mutex>&& lock) {
  assert(lock);
  ExtraContextInfo this_context_info;
  this_context_info.add_to_idle_queue = false;
  this_context_info.context_list_node = context_list_node;
  this_context_info.lock = &lock;

  platform::Context* previous_context = nullptr;
  platform::Context* next_context = DequeueReadyContext(true);
  if (next_context) {
    previous_context = platform::SwitchToContext(
        next_context, &this_context_info); 
  } else {
    previous_context = platform::SwitchToNewContext(
        stack_size_, &this_context_info,
        [this](platform::Context* previous_context) {
          return FiberStart(previous_context);
        });
  }
  OnFiberSuspended(previous_context);
}

void ThreadPool::WakeContext(ContextList::Node* node_to_wake) {
  std::lock_guard<std::mutex> lock(mutex_);
  // Move the context in question onto the resume queue.
  resume_queue_.push_back(node_to_wake);

  // Let someone know that there is a new task available to process.
  event_available_.notify_one();
}

size_t ThreadPool::GetLocalThreadId() {
  for (size_t i = 0; i < threads_.size(); ++i) {
    if (threads_[i].get_id() == std::this_thread::get_id()) {
      return i;
    }
  }
  assert(false);
  return 0;
}

platform::Context* ThreadPool::ReturnThreadToOriginalContextSwap(
    int local_thread_id, std::unique_lock<std::mutex>&& lock,
    platform::Context* previous_context) {
  std::unique_lock<std::mutex> new_context_lock(std::move(lock));
  assert(thread_contexts_[local_thread_id] == nullptr);
  
  thread_contexts_[local_thread_id] = previous_context;
  thread_contexts_added_.notify_all();
  
  size_t my_local_thread_id = GetLocalThreadId();
  while(thread_contexts_[my_local_thread_id] == nullptr) {
    thread_contexts_added_.wait(new_context_lock);
  }
  return thread_contexts_[my_local_thread_id];
}

void ThreadPool::ReturnThreadToOriginalContext(int local_thread_id) {
  // Returns all thread contexts to their original threads.
  std::unique_lock<std::mutex> lock(mutex_);
  if (threads_[local_thread_id].get_id() != std::this_thread::get_id()) {
    platform::Context* context = platform::SwitchToNewContext(
        stack_size_, nullptr,
        [&](platform::Context* previous_context) {
          return ReturnThreadToOriginalContextSwap(
                     local_thread_id, std::move(lock), previous_context);
        });
    assert(context == nullptr);
  }
}

}  // namespace ebb
