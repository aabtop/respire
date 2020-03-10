#ifndef __RESPIRE_FUTURE_H__
#define __RESPIRE_FUTURE_H__

#include "ebbpp.h"
#include "stdext/variant.h"

namespace respire {

// A wrapper around ebb::Future which allows us to also trivially provide a
// future value instead of setting up channels and pull/pushes.
template <typename R>
class Future {
 public:
  Future(ebb::PushPullConsumer<R()>* node)
      : result_(stdext::in_place_type_t<ebb::Future<R>>(), node) {}

  template <typename U>
  Future(ebb::PushPullConsumer<R(U)>* node, const U& param)
      : result_(stdext::in_place_type_t<ebb::Future<R>>(),
                node, param) {}
  template <typename U>
  Future(ebb::PushPullConsumer<R(U)>* node, U&& param)
      : result_(stdext::in_place_type_t<ebb::Future<R>>(),
                node, std::move(param)) {}

  Future(const R& value) : result_(value) {}
  Future(R&& value) : result_(std::move(value)) {}

  Future(const Future&) = delete;
  Future(Future&&) = delete;

  R* GetValue() {
    if (stdext::holds_alternative<ebb::Future<R>>(result_)) {
      return stdext::get<ebb::Future<R>>(result_).GetValue();
    } else {
      return &stdext::get<R>(result_);
    }
  }

 private:
  stdext::variant<ebb::Future<R>, R> result_;
};

}  // namespace respire

#endif  // __RESPIRE_FUTURE_H__
