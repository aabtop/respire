#include "ebb.h"

#include <cassert>

#ifdef __cplusplus
extern "C" {
#endif


void EbbQueueConstruct(
    EbbEnvironment* env, const EbbQueueDescriptor* desc,
    EbbQueue* queue) {
  queue->env = env;
  queue->desc = *desc;
  new (queue->mutex_memory.get()) std::mutex;
  new (queue->not_full_cond_memory.get())
      ebb::FiberConditionVariable(&env->thread_pool());
  new (queue->not_empty_cond_memory.get())
      ebb::FiberConditionVariable(&env->thread_pool());
  new (queue->no_active_push_cond_memory.get())
      ebb::FiberConditionVariable(&env->thread_pool());
  new (queue->no_active_pull_cond_memory.get())
      ebb::FiberConditionVariable(&env->thread_pool());

  assert(stdext::aligned(reinterpret_cast<uintptr_t>(desc->memory),
                               desc->item_alignment));
  assert(desc->item_size_in_bytes % desc->item_alignment == 0);
  assert(desc->memory_size_in_bytes % desc->item_size_in_bytes == 0);
  queue->begin = 0;
  queue->size = 0;

  // Make sure that at least one item can fit in the queue.
  assert(desc->memory_size_in_bytes - queue->begin >=
         desc->item_size_in_bytes);

  queue->active_pull = nullptr;
  queue->active_push = nullptr;
  queue->consumer = nullptr;
}

void EbbQueueDestruct(EbbQueue* queue) {
  assert(!queue->consumer);

  queue->no_active_pull_cond().
    ebb::FiberConditionVariable::~FiberConditionVariable();
  queue->no_active_push_cond().
    ebb::FiberConditionVariable::~FiberConditionVariable();
  queue->not_empty_cond().
    ebb::FiberConditionVariable::~FiberConditionVariable();
  queue->not_full_cond().ebb::FiberConditionVariable::~FiberConditionVariable();
  queue->mutex().std::mutex::~mutex();
}

namespace {
int64_t GetNextQueueItem(const EbbQueue* queue, int64_t position) {
  int64_t next_position = position + queue->desc.item_size_in_bytes;
  if (next_position >= queue->desc.memory_size_in_bytes) {
    return 0;
  } else {
    return next_position;
  }
}

int64_t GetQueueEndPosition(const EbbQueue* queue) {
  if (queue->begin + queue->size < queue->desc.memory_size_in_bytes) {
    return queue->begin + queue->size;
  } else {
    return queue->size - (queue->desc.memory_size_in_bytes - queue->begin);
  }
}
}  // namespace

void EbbQueueAcquirePush(EbbQueue* queue, EbbPush* push) {
  push->queue = queue;
  
  std::unique_lock<std::mutex> lock(queue->mutex());
  while (queue->active_push != nullptr) {
    queue->no_active_push_cond().wait(lock);  
  }
  queue->active_push = push;

  while (queue->size >= queue->desc.memory_size_in_bytes) {
    queue->not_full_cond().wait(lock);
  }

  push->data = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(queue->desc.memory) +
      GetQueueEndPosition(queue));
}

void* EbbPushGetData(EbbPush* push) {
  return push->data;
}

namespace {
bool InternalTryQueueAcquirePull(EbbQueue* queue, EbbPull* pull) {
  assert(pull->queue == queue);
  assert(queue->active_pull == pull);

  if (queue->size == 0) {
    return false;
  }

  pull->data = reinterpret_cast<void*>(
      reinterpret_cast<uintptr_t>(queue->desc.memory) + queue->begin);

  return true;
}

void ConsumerDrainQueue(EbbQueue* queue) {
  EbbConsumer* consumer = queue->consumer;
  while(true) {
    EbbPull pull;
    {
      std::lock_guard<std::mutex> lock(queue->mutex());
      assert(queue->active_pull == nullptr);
      queue->active_pull = &pull;
      pull.queue = queue;

      if (!InternalTryQueueAcquirePull(queue, &pull)) {
        queue->active_pull = nullptr;
        consumer->drain_queue_task_active = false;
        consumer->drain_queue_task().ebb::ThreadPool::Task::~Task();
        consumer->queue_drained_cond().notify_one();
        break;
      }
    }

    consumer->desc.consume_function(
        consumer->desc.member_data, EbbPullGetData(&pull));

    EbbPullSubmit(&pull);
  }

}
}  // namespace

void EbbPushSubmit(EbbPush* push) {
  EbbQueue* queue = push->queue;
  {
    std::lock_guard<std::mutex> lock(queue->mutex());
    assert(push == queue->active_push);
    
    bool was_empty = (queue->size == 0);
    queue->size += queue->desc.item_size_in_bytes;
    assert(queue->size <= queue->desc.memory_size_in_bytes);
    queue->not_empty_cond().notify_one();

    queue->active_push = nullptr;
    queue->no_active_push_cond().notify_one();

    if (was_empty && queue->consumer &&
        !queue->consumer->drain_queue_task_active) {
      queue->consumer->drain_queue_task_active = true;
      new (queue->consumer->drain_queue_task_memory.get())
          ebb::ThreadPool::Task(&queue->env->thread_pool(), [queue]() {
            ConsumerDrainQueue(queue);
          });
    }
  }

  queue = nullptr;
  push->data = nullptr;
}

void EbbQueueAcquirePull(EbbQueue* queue, EbbPull* pull) {
  pull->queue = queue;
  // Consumers should not be pulling through this public function.
  assert(!queue->consumer);

  std::unique_lock<std::mutex> lock(queue->mutex());
  while (queue->active_pull != nullptr) {
    queue->no_active_pull_cond().wait(lock);
  }
  queue->active_pull = pull;

  while (!InternalTryQueueAcquirePull(queue, pull)) {
    queue->not_empty_cond().wait(lock);
  }
}

void* EbbPullGetData(EbbPull* pull) {
  return pull->data;
}

void EbbPullSubmit(EbbPull* pull) {
  EbbQueue* queue = pull->queue;
  {
    std::lock_guard<std::mutex> lock(queue->mutex());
    assert(pull == queue->active_pull);
    
    queue->size -= queue->desc.item_size_in_bytes;
    assert(queue->size >= 0);
    queue->begin = GetNextQueueItem(queue, queue->begin);
    queue->not_full_cond().notify_one();

    queue->active_pull = nullptr;
    queue->no_active_pull_cond().notify_one();
  }

  queue = nullptr;
  pull->data = nullptr;
}

#ifdef __cplusplus
}
#endif
