#include "lib/file_reader.h"


#include <functional>

namespace ebb {
namespace lib {

FileReader::FileReader(
    Environment* env, stdext::file_system::PathStrRef path_ref,
    BatchedQueue<ErrorOrUInts>* output_queue)
    : env_(env), output_queue_(output_queue), path_(path_ref),
      consumer_(env, std::bind(&FileReader::Run, this)) {
  // Aaaaaaand now that everything's initialized, kick off the consumer_ task.
  Push<>(consumer_.queue());
}

void FileReader::Run() {
  FILE* file = fopen(path_.c_str(), "rb");
  if (!file) {
    Push<ErrorOrUInts>(output_queue_->queue(), -1);
    return;
  }

  int cur_buffer_index = 0;
  do {
    stdext::fixed_vector<uint8_t>* cur_buffer =
        output_queue_->buffers()[cur_buffer_index];
    cur_buffer->resize(cur_buffer->capacity());

    size_t bytes_read = fread(cur_buffer->data(), 1, cur_buffer->size(), file);
    assert(bytes_read <= cur_buffer->size());


    if (bytes_read > 0) {
      // Push whatever data we read into the output buffer.
      Push<ErrorOrUInts>(
          output_queue_->queue(),
          stdext::span<uint8_t>(cur_buffer->data(), bytes_read));
      cur_buffer_index =
          (cur_buffer_index + 1) % output_queue_->buffers().size();
    }

    if (bytes_read < cur_buffer->size()) {
      int error = ferror(file);
      if (error != 0) {
        // An error occurred, so push an error to the output queue.
        Push<ErrorOrUInts>(output_queue_->queue(), error);
        break;
      }

      int eof = feof(file);
      if (eof != 0) {
        // End-of-file means success, indicated by an "error" value of 0.
        Push<ErrorOrUInts>(output_queue_->queue(), 0);
        break;
      }
    }
  } while (true);

  fclose(file);
}

}  // namespace lib
}  // namespace ebb
