#ifndef __EBB_LIB_TYPES_H__
#define __EBB_LIB_TYPES_H__

#include "stdext/span.h"
#include "stdext/variant.h"

namespace ebb {
namespace lib {

// Useful/common type to represent a bytestream with the possibility of error.
// Error in this case is interpreted such that 0 means success and anything else
// is an error.
using ErrorOrUInts = stdext::variant<int, stdext::span<uint8_t>>;

}  // namespace lib
}  // namespace ebb

#endif  // __EBB_LIB_TYPES_H__
