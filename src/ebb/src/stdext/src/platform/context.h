#ifndef __PLATFORM_CONTEXT_H__
#define __PLATFORM_CONTEXT_H__

#include <cassert>
#include <functional>

namespace platform {

// The structure that stores a stored, dormant context.
struct Context;

// Switches to the context passed in as a parameter.  The return value will
// contain the context we switched back from, when we eventually switch back
// to this context.  The value |context_data| will be stored in the
// returned |Context| object and made available through the GetContextData()
// function.
Context* SwitchToContext(Context* destination_context,
                         void* context_data);

// Function called with the previous context we switched from passed in as
// a parameter.  The return value of this function is the context to switch to
// afterwards.  The value |context_data| will be stored in the created
// |Context| object and made available through the GetContextData() function.
typedef std::function<Context* (Context*)> FiberMain;
Context* SwitchToNewContext(size_t stack_size, void* context_data,
                            const FiberMain& entry_point);

// Extracts the context data stored with the Context, passed in through the
// |context_data| parameter by either the SwitchContext() function or the
// MakeContext() method.
void* GetContextData(const Context* context);

}  // namespace platform

#endif  // __PLATFORM_CONTEXT_H__
