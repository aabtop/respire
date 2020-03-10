#ifndef __EBB_FIBER_CONDITION_VARIABLE_H__
#define __EBB_FIBER_CONDITION_VARIABLE_H__

#include <condition_variable>
#include <mutex>

#include "linked_list.h"
#include "thread_pool.h"
#include "platform/context.h"
#include "stdext/optional.h"

namespace ebb {

class FiberConditionVariable {
 public:
  FiberConditionVariable(ThreadPool* thread_pool);
  ~FiberConditionVariable();

  void notify_one();
  void wait(std::unique_lock<std::mutex>& lock);

 private:
  ThreadPool* thread_pool_;

  // The internal condition variable will only be initialized if it is needed,
  // which occurs when a non-thread pool thread waits on this.
  stdext::optional<std::condition_variable> internal_cond_;

  ThreadPool::ContextList wait_queue_;
};

}  // namespace ebb

#endif  // __EBB_FIBER_CONDITION_VARIABLE_H__
