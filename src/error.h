#ifndef __RESPIRE_ERROR_H__
#define __RESPIRE_ERROR_H__

#include <string>
#include "stdext/optional.h"

namespace respire {

class Error {
 public:
  Error(const std::string& str) : str_(str) {}
  Error(const Error& other) = default;
  const std::string& str() const { return str_; }

 private:
  std::string str_;
};

using OptionalError = stdext::optional<Error>;

}  // namespace respire

#endif  // __RESPIRE_ERROR_H__
