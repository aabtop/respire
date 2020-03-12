#ifndef __RESPIRE_FILE_INFO_NODE_H__
#define __RESPIRE_FILE_INFO_NODE_H__

#include <chrono>

#include "error.h"
#include "future.h"
#include "lib/json_string_view.h"
#include "stdext/file_system.h"
#include "stdext/variant.h"

namespace respire {

struct FileInfo {
  FileInfo(ebb::lib::JSONPathStringView filename,
           const stdext::optional<std::chrono::system_clock::time_point>&
               last_modified_time,
           bool soft_output)
      : filename(filename), last_modified_time(last_modified_time),
        soft_output(soft_output) {}
  FileInfo(const FileInfo& other) = default;
  FileInfo(FileInfo&& other) = default;

  ebb::lib::JSONPathStringView filename;
  stdext::optional<std::chrono::system_clock::time_point>
      last_modified_time;
  // True if this represents a file output that *may not* be modified by
  // a FileInfoNode.
  bool soft_output;
};

class FileOutput {
 public:
  using Value = std::vector<FileInfo>;

  FileOutput(const FileOutput& rhs) : data_(rhs.data_) {};
  // The non-const copy constructor ensures that the variadic constructor does
  // not take preference.
  FileOutput(FileOutput& rhs)
      : FileOutput(const_cast<const FileOutput&>(rhs)) {}
  FileOutput(FileOutput&& rhs) = default;

  FileOutput(const Value& value) : data_(value) {}
  FileOutput(Value&& value) : data_(std::move(value)) {}
  FileOutput(const Error& error) : data_(error) {}
  FileOutput(Error&& error) : data_(std::move(error)) {}

  // Convenience function for creating a single-result entry.
  template <typename... U>
  FileOutput(U&&... u) : data_(Value(1, FileInfo(std::forward<U>(u)...))) {}

  const Error* error() const {
    return stdext::holds_alternative<Error>(data_) ?
               &stdext::get<Error>(data_) : nullptr;
  }

  const Value* value() const {
    return stdext::holds_alternative<Value>(data_) ?
               &stdext::get<Value>(data_) : nullptr;
  }

 private:
  stdext::variant<Value, Error> data_;
};

class FileInfoNode {
  public:
    // TODO: In C++17 this interface can be changed to return by value and then
    // rely on the RVO to avoid having a copy constructor called.  However,
    // until then, we'll have to use std::unique_ptr<Future> if we want to keep
    // this nice interface.  In that case, we should then be able to delete the
    // FuturePtr typedef, and just replace its use with Future.
    using Future = respire::Future<FileOutput>;
    using FuturePtr = std::unique_ptr<Future>;

    virtual ~FileInfoNode() {}

    virtual FuturePtr GetFileInfo(bool dry_run = false) = 0;

    // Clears and populates a vector with the list of output files produced by
    // this node, in the same order that they will appear in the FileInfo
    // structure returned by GetFileInfo().
    virtual void GetOrderedOutputPaths(
        std::vector<ebb::lib::JSONPathStringView>* paths) = 0;
};

// Helper structure to enable addressing of a specific output from a specific
// FileInfoNode.
struct FileInfoNodeOutput {
  FileInfoNodeOutput() {}

  FileInfoNodeOutput(FileInfoNode* node, size_t index)
      : node(node), index(index) {}
  FileInfoNode* node;
  // The index in the GetFileInfo() output vector this output can be found at
  // within a FileOutput.
  size_t index;
};

}  // namespace respire

#endif  // __RESPIRE_FILE_INFO_NODE_H__
