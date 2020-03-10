#include "fiber_condition_variable.h"

namespace ebb {

FiberConditionVariable::FiberConditionVariable(ThreadPool* thread_pool) :
    thread_pool_(thread_pool) {}

FiberConditionVariable::~FiberConditionVariable() {
  assert(wait_queue_.empty());
}

void FiberConditionVariable::notify_one() {
  if (wait_queue_.empty()) {
    return;
  }

  ThreadPool::ContextList::Node* next_context = wait_queue_.front();
  wait_queue_.pop_front();

  if (next_context->item()) {
    thread_pool_->WakeContext(next_context);
  } else {
    // The wait queue ContextNode not having a Context set on it is used as
    // a signal to indicate that the corresponding wait() was made to directly
    // to |internal_cond_|.
    internal_cond_->notify_one();
  } 
}

void FiberConditionVariable::wait(std::unique_lock<std::mutex>& lock) {
  assert(lock);
  std::mutex* mutex = lock.mutex();

  ThreadPool::ContextList::Node context_node(nullptr);
  wait_queue_.push_back(&context_node);

  if (thread_pool_->IsCurrentThreadInPool()) {
    // Stash this context away and look for a new one.
    thread_pool_->SleepCurrentContext(&context_node, std::move(lock));
    lock = std::unique_lock<std::mutex>(*mutex);
  } else {
    if (!internal_cond_.has_value()) {
      internal_cond_.emplace();
    }
    internal_cond_->wait(lock);
  }
}

}  // namespace ebb
