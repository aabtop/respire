#ifndef __EBB_THREAD_POOL_H__
#define __EBB_THREAD_POOL_H__

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "linked_list.h"
#include "platform/context.h"

namespace ebb {

class ThreadPool {
 public:
  class Task {
   public:
    Task(ThreadPool* thread_pool, const std::function<void()>& function);

   private:
    friend class ThreadPool;

    ThreadPool* thread_pool_;
    std::function<void()> function_;

    LinkedList<Task>::Node ready_queue_node_;
  };

  enum class SchedulingPolicy {
    // The FIFO (first in first out) policy is the most fair, assigning CPU
    // resources to whichever task has been waiting the longest.
    FIFO,
    // The LIFO policy encourages a more depth-first approach to task resolution
    // which can potentially reduce the amount of tasks that are started but
    // not completed (because they have initiated other tasks that they depend
    // on).
    LIFO,
  };

  ThreadPool(int num_threads, size_t stack_size,
             SchedulingPolicy scheduling_policy);
  ~ThreadPool();

  bool IsCurrentThreadInPool() const;

 private:
  typedef LinkedList<platform::Context> ContextList;
  enum RunLoopPolicy {
    kRunLoopPolicy_Thread,
    kRunLoopPolicy_Fiber,
  };

  platform::Context* RunLoop(RunLoopPolicy policy);

  void ThreadStart(int local_thread_id);
  platform::Context* FiberStart(platform::Context* previous_context);

  void OnFiberSuspended(platform::Context* suspended_context);

  void WaitForEvent();

  void EnqueueReadyTask(Task* task);
  Task* DequeueReadyTask();

  // Dequeues a context from the resume queue and returns it.  Returns null if
  // the resume queue is empty.
  // If |dequeue_idle_contexts| is true, then the function will also check the
  // idle queue and return a context from there if the resume queue is empty.
  platform::Context* DequeueReadyContext(bool dequeue_idle_contexts);

  // Sleep current thread will put the current context to sleep and schedule
  // a new one to perform other tasks.  The |lock| parameter will be released
  // before putting the thread to sleep, and reaquired after waking it.
  void SleepCurrentContext(ContextList::Node* context_list_node,
                           std::unique_lock<std::mutex>&& lock);
  // Wakes a context that was put to sleep through SleepCurrentContext().
  void WakeContext(ContextList::Node* node_to_wake);

  // All threads, before quitting, are expected to call this function so that
  // if they ended up switching their contexts with another thread, they are
  // fixed back up into their original threads.  This helps the system's thread
  // shutdown logic.
  void ReturnThreadToOriginalContext(int local_thread_id);
  platform::Context* ReturnThreadToOriginalContextSwap(
      int local_thread_id, std::unique_lock<std::mutex>&& lock,
      platform::Context* previous_context);

  size_t GetLocalThreadId();

  static platform::Context* ExtractFromQueue(
      ContextList* queue, SchedulingPolicy scheduling_policy);

  // The quit flag that signals that all threads should now exit.
  bool quit_;

  const size_t stack_size_;
  const SchedulingPolicy scheduling_policy_;

  std::vector<std::thread> threads_;

  // Parallel to |threads_|, used during shutdown to return thread contexts
  // to their original threads.
  std::vector<platform::Context*> thread_contexts_;
  std::condition_variable thread_contexts_added_;

  std::mutex mutex_;
  std::condition_variable event_available_;

  LinkedList<Task> ready_queue_;

  // A list of suspended contexts/fibers that are now ready to be resumed.
  ContextList resume_queue_;
  // A list of suspended contexts who have thread entry points on the callstack,
  // and who are idle and ready to work.  We keep these separate because unlike
  // fiber contexts, thread contexts will stick around until the very end, even
  // if they are idle.
  ContextList idle_queue_;

  // Interesting statistics about how many contexts exist in parallel.

  // The high water mark for number of simultaneous contexts existing at one
  // time.
  int num_simultaneous_hwm_ = 0;
  // The number of contexts that exist at any given time.
  int num_contexts_ = 0;

  friend class FiberConditionVariable;
};

}  // namespace ebb

#endif  // __EBB_THREAD_POOL_H__
