#include "ebb.h"

#include <cassert>

#ifdef __cplusplus
extern "C" {
#endif

void EbbConsumerConstruct(
    EbbEnvironment* env, const EbbConsumerDescriptor* desc,
    EbbConsumer* consumer) {
  assert(desc->queue);
  assert(!desc->queue->consumer);

  consumer->desc = *desc;
  consumer->env = env;
  desc->queue->consumer = consumer;
  consumer->drain_queue_task_active = false;
  new (consumer->queue_drained_cond_memory.get())
      ebb::FiberConditionVariable(&env->thread_pool());
}

thread_local int consumer_number = 0;

void EbbConsumerDestruct(EbbConsumer* consumer) {
  // Wait for queue to drain.
  EbbQueue* queue = consumer->desc.queue;
  {
    std::unique_lock<std::mutex> lock(queue->mutex());
    while (consumer->drain_queue_task_active) {
      consumer->queue_drained_cond().wait(lock);
    }
  }

  assert(consumer->desc.queue->consumer == consumer);
  consumer->desc.queue->consumer = nullptr;

  consumer->queue_drained_cond().
      ebb::FiberConditionVariable::~FiberConditionVariable();
}

#ifdef __cplusplus
}
#endif
