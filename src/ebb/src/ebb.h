#ifndef __EBB_EBB_H__
#define __EBB_EBB_H__

#include <cstdint>
#include <mutex>

#include "fiber_condition_variable.h"
#include "stdext/align.h"
#include "thread_pool.h"

struct EbbEnvironmentDescriptor {
  int32_t num_thread_pool_threads;
  size_t fiber_stack_sizes;

  // The scheduling policy determines how Ebb decides which tasks to run.
  using SchedulingPolicy = ebb::ThreadPool::SchedulingPolicy;
  SchedulingPolicy scheduling_policy;
};

struct EbbEnvironment {
  ebb::ThreadPool& thread_pool() {
    return *reinterpret_cast<ebb::ThreadPool*>(thread_pool_memory.get());
  }

  EbbEnvironmentDescriptor desc;
  stdext::aligned_memory<ebb::ThreadPool> thread_pool_memory;
};

struct EbbQueueDescriptor {
  void* memory;
  int64_t memory_size_in_bytes;
  int64_t item_size_in_bytes;
  int32_t item_alignment;
};

struct EbbPull;
struct EbbPush;
struct EbbConsumer;
struct EbbQueue {
  std::mutex& mutex() {
    return *reinterpret_cast<std::mutex*>(mutex_memory.get());
  }
  ebb::FiberConditionVariable& not_full_cond() {
    return *reinterpret_cast<ebb::FiberConditionVariable*>(
        not_full_cond_memory.get());
  }
  ebb::FiberConditionVariable& not_empty_cond() {
    return *reinterpret_cast<ebb::FiberConditionVariable*>(
        not_empty_cond_memory.get());
  }
  ebb::FiberConditionVariable& no_active_push_cond() {
    return *reinterpret_cast<ebb::FiberConditionVariable*>(
        no_active_push_cond_memory.get());
  }
  ebb::FiberConditionVariable& no_active_pull_cond() {
    return *reinterpret_cast<ebb::FiberConditionVariable*>(
        no_active_pull_cond_memory.get());
  }

  EbbEnvironment* env;
  EbbQueueDescriptor desc;
  stdext::aligned_memory<std::mutex> mutex_memory;
  stdext::aligned_memory<ebb::FiberConditionVariable>
      not_full_cond_memory;
  stdext::aligned_memory<ebb::FiberConditionVariable>
      not_empty_cond_memory;
  stdext::aligned_memory<ebb::FiberConditionVariable>
      no_active_push_cond_memory;
  stdext::aligned_memory<ebb::FiberConditionVariable>
      no_active_pull_cond_memory;

  int64_t begin;
  int64_t size;
  EbbPull* active_pull;
  EbbPush* active_push;

  EbbConsumer* consumer;
};

struct EbbPull {
  EbbQueue* queue;
  void* data;
};

struct EbbPush {
  EbbQueue* queue;
  void* data;
};

struct EbbConsumerDescriptor {
  void* member_data;
  void (*consume_function)(void*, void*);
  EbbQueue* queue;
};

struct EbbConsumer {
  ebb::ThreadPool::Task& drain_queue_task() {
    return *reinterpret_cast<ebb::ThreadPool::Task*>(
        drain_queue_task_memory.get());
  }
  ebb::FiberConditionVariable& queue_drained_cond() {
    return *reinterpret_cast<ebb::FiberConditionVariable*>(
        queue_drained_cond_memory.get());
  }

  EbbEnvironment* env;
  EbbConsumerDescriptor desc;
  stdext::aligned_memory<ebb::ThreadPool::Task> drain_queue_task_memory;
  stdext::aligned_memory<ebb::FiberConditionVariable>
      queue_drained_cond_memory;
  bool drain_queue_task_active;
};

#ifdef __cplusplus
extern "C" {
#endif

void EbbEnvironmentConstruct(
    const EbbEnvironmentDescriptor* desc, EbbEnvironment* env);
void EbbEnvironmentDestruct(EbbEnvironment* env);

void EbbConsumerConstruct(
    EbbEnvironment* env, const EbbConsumerDescriptor* desc,
    EbbConsumer* consumer);
// Will wait for queue to drain.
void EbbConsumerDestruct(EbbConsumer* consumer);

void EbbQueueConstruct(
    EbbEnvironment* env, const EbbQueueDescriptor* desc, EbbQueue* queue);
void EbbQueueDestruct(EbbQueue* queue);

void EbbQueueAcquirePush(EbbQueue* queue, EbbPush* push);
void* EbbPushGetData(EbbPush* push);
void EbbPushSubmit(EbbPush* push);

void EbbQueueAcquirePull(EbbQueue* queue, EbbPull* pull);
void* EbbPullGetData(EbbPull* pull);
void EbbPullSubmit(EbbPull* pull);

#ifdef __cplusplus
}
#endif

#endif  // __EBB_EBB_H__