#ifndef __EBB_LIB_FILE_READER_H__
#define __EBB_LIB_FILE_READER_H__

#include "ebbpp.h"
#include "lib/batching.h"
#include "lib/types.h"
#include "stdext/span.h"
#include "stdext/file_system.h"
#include "stdext/variant.h"

#include <stdio.h>

namespace ebb {
namespace lib {

class FileReader {
 public:
  FileReader(Environment* env, stdext::file_system::PathStrRef path_ref,
             BatchedQueue<ErrorOrUInts>* output_queue);

  // TODO: Add a cancel() method...  Though this will need to involve making
  //       pushes cancelleable.

 private:
  void Run();

  Environment* env_;
  BatchedQueue<ErrorOrUInts>* output_queue_;
  const stdext::file_system::PathStrRef path_;
  ConsumerWithQueue<void, 1> consumer_;
};

}  // namespace lib
}  // namespace ebb

#endif  // __EBB_LIB_FILE_READER_H__
