#include "platform/context.h"

#include <ucontext.h>

namespace platform {

struct Context {
  // Parameters passed when proceeding to the next context.
  ucontext_t ucontext;
  void* data;

  // Parameters passed when returning from a context.
  void* stack_to_free;
  Context* previous_context;
};

namespace {
void OnReturnFromSwap(Context* context) {
  if (context->stack_to_free) {
    free(context->stack_to_free);
    context->stack_to_free = nullptr;
  }
}

struct NewContextParams {
  const FiberMain* fiber_main;
  Context* previous_context;
  void* stack_pointer;
};
thread_local NewContextParams tl_new_context_params;

void RunNewContext() {
  FiberMain fiber_main = std::move(*tl_new_context_params.fiber_main);
  void* stack_pointer = tl_new_context_params.stack_pointer;
  Context* next_context = fiber_main(tl_new_context_params.previous_context);

  // We cannot end a fiber naturally, a new context to switch to must be
  // specified.  Only the main original thread can terminate naturally.
  assert(next_context);

  // We're done, so tell the next fiber to free our stack after we perform
  // our final switch.
  next_context->stack_to_free = stack_pointer;
  next_context->previous_context = nullptr;
  if (setcontext(&next_context->ucontext) == -1) {
    assert(false);
  }
}
}  // namespace

Context* SwitchToContext(Context* destination_context,
                         void* context_data) {
  Context existing_context;
  existing_context.data = context_data;

  destination_context->stack_to_free = nullptr;
  destination_context->previous_context = &existing_context;
  if (swapcontext(
          &existing_context.ucontext, &destination_context->ucontext) == -1) {
    return nullptr;
  }

  OnReturnFromSwap(&existing_context);
  return existing_context.previous_context;
}

Context* SwitchToNewContext(
    size_t stack_size, void* context_data, const FiberMain& entry_point) {
  Context existing_context;
  existing_context.data = context_data;

  ucontext_t new_ucontext;
  if (getcontext(&new_ucontext) == -1) {
    return nullptr;
  }

  new_ucontext.uc_stack.ss_size = stack_size;
  new_ucontext.uc_stack.ss_sp = malloc(stack_size);
  if (new_ucontext.uc_stack.ss_sp == nullptr) {
    return nullptr;
  }

  // We pass parameters to the new context through a thread local variable.
  tl_new_context_params.fiber_main = &entry_point;
  tl_new_context_params.previous_context = &existing_context;
  tl_new_context_params.stack_pointer = new_ucontext.uc_stack.ss_sp;
  new_ucontext.uc_link = nullptr;

  makecontext(
      &new_ucontext, &RunNewContext, 0);
  if (swapcontext(&existing_context.ucontext, &new_ucontext) == -1) {
    free(new_ucontext.uc_stack.ss_sp);
    return nullptr;
  }
  OnReturnFromSwap(&existing_context);
  return existing_context.previous_context;
}

void* GetContextData(const Context* context) {
  return context->data;
}

}  // namespace platform
