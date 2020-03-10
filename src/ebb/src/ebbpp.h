#ifndef __EBB_INCLUDE_EBBPP_H__
#define __EBB_INCLUDE_EBBPP_H__

#include "ebb.h"
#include "stdext/align.h"

namespace ebb {

class Environment {
 public:
  using SchedulingPolicy = EbbEnvironmentDescriptor::SchedulingPolicy;
  Environment(
      int32_t num_thread_pool_threads, size_t fiber_stack_sizes,
      SchedulingPolicy scheduling_policy = SchedulingPolicy::FIFO) {
    EbbEnvironmentDescriptor env_desc = {0};
    env_desc.num_thread_pool_threads = num_thread_pool_threads;
    env_desc.fiber_stack_sizes = fiber_stack_sizes;
    env_desc.scheduling_policy = scheduling_policy;
    EbbEnvironmentConstruct(&env_desc, &env_);
  }

  ~Environment() {
    EbbEnvironmentDestruct(&env_);
  }

  EbbEnvironment* env() { return &env_; }

 private:
  EbbEnvironment env_;
};

namespace internal {
struct void_t {};

template <typename T>
struct data_storage_type {
  using type = T;
};

template <>
struct data_storage_type<void> {
  using type = void_t;
};

template<typename R, typename U>
struct consume_function_type {
  using type = std::function<R(U&&)>;
};

template<typename R>
struct consume_function_type<R, void> {
  using type = std::function<R()>;
};

template<typename R>
struct consume_function_type<R, void_t> {
  using type = std::function<R()>;
};

}  // namespace internal

template <typename T>
class Queue {
 public:
  EbbQueue* queue() { return &queue_; }

 private:
  EbbQueue queue_;
};

template <typename T>
class QueueGivenMemory : public Queue<T> {
 public:
  using DATA = typename internal::data_storage_type<T>::type;

  QueueGivenMemory(Environment* env, DATA* memory, size_t max_items) {
    EbbQueueDescriptor desc;
    desc.memory = memory;
    desc.memory_size_in_bytes = max_items * sizeof(DATA);
    desc.item_size_in_bytes = sizeof(DATA);
    desc.item_alignment = alignof(DATA);
    EbbQueueConstruct(env->env(), &desc, Queue<T>::queue());
  }
  ~QueueGivenMemory() {
    EbbQueueDestruct(Queue<T>::queue());
  }
};

template <typename T, size_t MAX_ITEMS>
class QueueWithMemory : public Queue<T> {
 public:
  using DATA = typename internal::data_storage_type<T>::type;

  QueueWithMemory(Environment* env) {
    EbbQueueDescriptor desc;
    desc.memory = queue_memory_.get();
    desc.memory_size_in_bytes = sizeof(queue_memory_);
    desc.item_size_in_bytes = sizeof(DATA);
    desc.item_alignment = alignof(DATA);
    EbbQueueConstruct(env->env(), &desc, Queue<T>::queue());
  }

  ~QueueWithMemory() {
    EbbQueueDestruct(Queue<T>::queue());
  }

 private:
  stdext::aligned_memory<DATA, MAX_ITEMS> queue_memory_;
};

template<typename T = void>
class Push {
 public:
  using DATA = typename internal::data_storage_type<T>::type;

  template <class... U>
  Push(Queue<T>* queue, U&&... u) {
    EbbQueueAcquirePush(queue->queue(), &push_);
    new (data()) DATA(std::forward<U>(u)...);
  }

  ~Push() {
    EbbPushSubmit(&push_);
  }

  DATA* data() { return reinterpret_cast<DATA*>(EbbPushGetData(&push_)); }

 private:
  EbbPush push_;
};

template<typename T = void>
class Pull {
 public:
  using DATA = typename internal::data_storage_type<T>::type;

  Pull(Queue<T>* queue) {
    EbbQueueAcquirePull(queue->queue(), &pull_);
  }

  ~Pull() {
    data()->~DATA();
    EbbPullSubmit(&pull_);
  }

  DATA* data() { return reinterpret_cast<DATA*>(EbbPullGetData(&pull_)); }

 private:
  EbbPull pull_;
};

namespace internal {
template <typename R, typename U>
class ConsumeFunction {
 public:
  static R Call(const std::function<R(U&&)>& consume_function, U&& data) {
    return consume_function(std::move(data));
  }
};
template <typename R>
class ConsumeFunction<R, typename internal::data_storage_type<void>::type> {
 public:
  static R Call(
      const std::function<R()>& consume_function,
      typename internal::data_storage_type<void>::type&& data) {
    return consume_function();
  }
};
}  // namespace internal

template <typename T>
class Consumer {
 public:
  using DATA = typename internal::data_storage_type<T>::type;
  using consume_function_t =
      typename internal::consume_function_type<void, T>::type;

  Consumer(Environment* env, Queue<T>* queue,
           const consume_function_t& consume_function)
      : consume_function_(consume_function) {
    EbbConsumerDescriptor consumer_desc;
    consumer_desc.member_data = &consume_function_;
    consumer_desc.consume_function =
        ([](void* member_data, void* data) {
      auto consume_function = reinterpret_cast<consume_function_t*>(
          member_data);
      internal::ConsumeFunction<void, DATA>::Call(
          *consume_function, std::move(*reinterpret_cast<DATA*>(data)));
    });
    consumer_desc.queue = queue->queue();
    EbbConsumerConstruct(env->env(), &consumer_desc, &consumer_);
  }

  ~Consumer() {
    EbbConsumerDestruct(&consumer_);
  }

 private:
  EbbConsumer consumer_;
  consume_function_t consume_function_;
};

template <typename T, size_t MAX_QUEUE_ITEMS>
class ConsumerWithQueue {
 public:
  using DATA = typename internal::data_storage_type<T>::type;
  using consume_function_t =
      typename internal::consume_function_type<void, T>::type;

  ConsumerWithQueue(
      Environment* env, const consume_function_t& consumer_function)
      : queue_(env), consumer_(env, &queue_, consumer_function) {}

  QueueWithMemory<T, MAX_QUEUE_ITEMS>* queue() { return &queue_; }

 private:
  QueueWithMemory<T, MAX_QUEUE_ITEMS> queue_;
  Consumer<T> consumer_;
};

template <typename R>
class Future;

template <typename F>
class PushPullConsumer {};

template <typename R, typename U>
class PushPullConsumer<R(U)> {
 public:
  using consume_function_t =
      typename internal::consume_function_type<R, U>::type;

  PushPullConsumer(Environment* env, const consume_function_t& function)
      : env_(env), function_(function),
        consumer_(env, [this](QueueItem&& t) {
          Push<R> push(t.response_queue,
                       internal::ConsumeFunction<R, U>::Call(
                           function_, std::move(t.data)));
        }) {}

  PushPullConsumer(const PushPullConsumer&) = delete;

 private:
  struct QueueItem {
    QueueItem(Queue<R>* response_queue, const U& data)
        : data(data), response_queue(response_queue) {}
    QueueItem(Queue<R>* response_queue, U&& data)
        : data(std::move(data)), response_queue(response_queue) {}
    QueueItem(Queue<R>* response_queue)
        : response_queue(response_queue) {}
    U data;
    Queue<R>* response_queue;
  };

  Environment* env_;
  consume_function_t function_;
  ConsumerWithQueue<QueueItem, 1> consumer_;

  friend class Future<R>;
};

template <typename R>
class PushPullConsumer<R()> : public PushPullConsumer<R(internal::void_t)> {
 public:
  PushPullConsumer(Environment* env, const std::function<R()>& function)
      : PushPullConsumer<R(internal::void_t)>(env, function) {}
};

template <typename R>
class Future {
 public:
  Future(PushPullConsumer<R()>* node)
      : response_queue_(node->env_) {
    Push<typename PushPullConsumer<R()>::QueueItem> push(
        node->consumer_.queue(), &response_queue_);
  }

  template <typename U>
  Future(PushPullConsumer<R(U)>* node, const U& param)
      : response_queue_(node->env_) {
    Push<typename PushPullConsumer<R(U)>::QueueItem> push(
        node->consumer_.queue(), &response_queue_, param);
  }

  template <typename U>
  Future(PushPullConsumer<R(U)>* node, U&& param)
      : response_queue_(node->env_) {
    Push<typename PushPullConsumer<R(U)>::QueueItem> push(
        node->consumer_.queue(), &response_queue_, std::forward<U>(param));
  }

  Future(const Future&) = delete;
  Future(Future&&) = delete;

  // Always ensure that a future completes before destroying it.
  ~Future() { GetValue(); }

  R* GetValue() {
    if (!pull_.has_value()) {
      pull_.emplace(&response_queue_);
    }
    return pull_->data();
  }

 private:
  QueueWithMemory<R, 1> response_queue_;
  stdext::optional<Pull<R>> pull_;
};

}  // namespace ebb

#endif  // __EBB_INCLUDE_EBB_H__
