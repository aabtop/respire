#include "platform/context.h"

#include <windows.h>

namespace platform {

struct Context {
  void* fiber;
  void* data;

  void* fiber_to_delete;
  Context* previous_context;
};

thread_local void* tl_current_fiber = NULL;

namespace {

void EnsureThreadIsFiber() {
  if (!tl_current_fiber) {
    tl_current_fiber = ConvertThreadToFiber(NULL);
  } else {
    assert(tl_current_fiber);
  }
}

struct NewContextParams {
  const FiberMain* fiber_main;
  Context* previous_context;
};

void __stdcall FiberEntryPoint(void* param) {
  NewContextParams* params = reinterpret_cast<NewContextParams*>(param);

  Context* next_context = (*params->fiber_main)(params->previous_context);

  assert(next_context);

  next_context->previous_context = nullptr;
  next_context->fiber_to_delete = tl_current_fiber;

  tl_current_fiber = next_context->fiber;
  SwitchToFiber(next_context->fiber);
}

void OnReturnFromSwap(Context* context) {
  if (context->fiber_to_delete) {
    DeleteFiber(context->fiber_to_delete);
    context->fiber_to_delete = NULL;
  }
}

}  // namespace

Context* SwitchToContext(Context* destination_context,
                         void* context_data) {
  EnsureThreadIsFiber();

  Context existing_context;
  existing_context.fiber = tl_current_fiber;
  existing_context.data = context_data;

  destination_context->fiber_to_delete = NULL;
  destination_context->previous_context = &existing_context;
  tl_current_fiber = destination_context->fiber;
  SwitchToFiber(destination_context->fiber);

  OnReturnFromSwap(&existing_context);
  return existing_context.previous_context;
}

Context* SwitchToNewContext(size_t stack_size, void* context_data,
                            const FiberMain& entry_point) {
  EnsureThreadIsFiber();

  Context existing_context;
  existing_context.fiber = tl_current_fiber;
  existing_context.data = context_data;

  NewContextParams new_context_params;
  new_context_params.fiber_main = &entry_point;
  new_context_params.previous_context = &existing_context;

  void* next_fiber = CreateFiber(stack_size, &FiberEntryPoint,
                                 &new_context_params);
  tl_current_fiber = next_fiber;
  SwitchToFiber(next_fiber);

  OnReturnFromSwap(&existing_context);
  return existing_context.previous_context;
}

void* GetContextData(const Context* context) {
  return context->data;
}

}  // namespace platform
