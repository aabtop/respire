#include "ebb.h"

#ifdef __cplusplus
extern "C" {
#endif

void EbbEnvironmentConstruct(
    const EbbEnvironmentDescriptor* desc, EbbEnvironment* env) {
  env->desc = *desc;
  new (env->thread_pool_memory.get())
      ebb::ThreadPool(desc->num_thread_pool_threads, desc->fiber_stack_sizes,
                      desc->scheduling_policy);
}

void EbbEnvironmentDestruct(EbbEnvironment* env) {
  env->thread_pool().ebb::ThreadPool::~ThreadPool();
}

#ifdef __cplusplus
}
#endif
