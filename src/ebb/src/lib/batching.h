#ifndef __EBB_LIB_BATCHING_H__
#define __EBB_LIB_BATCHING_H__

#include "ebbpp.h"
#include "stdext/variant.h"
#include "stdext/span.h"
#include "stdext/fixed_vector.h"

namespace ebb {
namespace lib {

template <typename SignalType, typename DataType>
using Batch = stdext::variant<SignalType, stdext::span<DataType>>;

template <typename T>
class BatchedQueue {};

template <typename S, typename T>
class BatchedQueue<Batch<S, T>> {
 public:
  using QueueElementType = Batch<S, T>;
  using QueueType = Queue<QueueElementType>;

  BatchedQueue(
      QueueType* queue, stdext::span<stdext::fixed_vector<T>*> buffers)
      : BatchedQueue() {
    Initialize(queue, buffers);
  }

  QueueType* queue() { return queue_; }
  stdext::span<stdext::fixed_vector<T>*> buffers() { return buffers_; }

 protected:
  BatchedQueue() : buffers_(nullptr, 0) {}
  void Initialize(
      QueueType* queue, stdext::span<stdext::fixed_vector<T>*> buffers) {
    queue_ = queue;
    buffers_ = buffers;
  }

 private:
  QueueType* queue_;
  stdext::span<stdext::fixed_vector<T>*> buffers_;
};

template <typename T, size_t BUFFER_SIZE, size_t NUM_BUFFERS = 2>
class BatchedQueueWithMemory;

template <typename S, typename T, size_t BUFFER_SIZE, size_t NUM_BUFFERS>
class BatchedQueueWithMemory<Batch<S, T>, BUFFER_SIZE, NUM_BUFFERS>
    : public BatchedQueue<Batch<S, T>> {
 private:
  using Super = BatchedQueue<Batch<S, T>>;

 public:
  BatchedQueueWithMemory(Environment* env)
      : queue_(env) {
    for (size_t i = 0; i < NUM_BUFFERS; ++i) {
      buffer_pointers_[i] = &buffers_[i];
    }
    Super::Initialize(
        &queue_, stdext::make_span(buffer_pointers_, NUM_BUFFERS));
  }

 private:
  QueueWithMemory<typename Super::QueueElementType, NUM_BUFFERS - 1> queue_;
  stdext::fixed_vector_with_memory<T, BUFFER_SIZE> buffers_[NUM_BUFFERS];
  stdext::fixed_vector<T>* buffer_pointers_[NUM_BUFFERS];
};

template <typename T>
class PushBatcher {};

// A helper class for pushing into a batched queue.  A batched queue is one
// whose element type matches the signature of the Batch type defined here.
// The definition of a batch involves a stream of data, and also the possibility
// of a signal event that can be used to indicate events such as errors or end
// of streams. 
template <typename S, typename T>
class PushBatcher<Batch<S, T>> {
 public:
  PushBatcher(BatchedQueue<Batch<S, T>>* batched_queue)
      : batched_queue_(batched_queue->queue(), batched_queue->buffers())
      , current_buffer_(0) {}

  ~PushBatcher() {
    if (!current_buffer()->empty()) {
      PushCurrentBuffer();
    }
  }

  // Push data into the batching buffer, and possibly push the filled buffer
  // batch into the actual queue.
  template <typename... U>
  void PushData(U&&... u) {
    current_buffer()->emplace_back(std::forward<U>(u)...);
    if (current_buffer()->size() == current_buffer()->capacity()) {
      PushCurrentBuffer();
    }
  }

  // Push a signal into the queue, though before doing so, flush any data
  // currently batched up.
  template <typename... U>
  void PushSignal(U&&... u) {
    if (!current_buffer()->empty()) {
      PushCurrentBuffer();
    }
    Push<Batch<S, T>>(batched_queue_.queue(), S(std::forward<U>(u)...));
  }

 private:
  stdext::fixed_vector<T>* current_buffer() {
    return batched_queue_.buffers()[current_buffer_];
  }

  void PushCurrentBuffer() {
    Push<Batch<S, T>>(
        batched_queue_.queue(),
        stdext::span<T>(current_buffer()->data(), current_buffer()->size()));
    current_buffer_ = (current_buffer_ + 1) % batched_queue_.buffers().size();
    current_buffer()->clear();
  }

  BatchedQueue<Batch<S, T>> batched_queue_;

  size_t current_buffer_;
};

}  // namespace lib
}  // namespace ebb

#endif  // __EBB_LIB_BATCHING_H__