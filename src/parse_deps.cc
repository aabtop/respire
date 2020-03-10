#include "parse_deps.h"

#include <fstream>
#include <string>

namespace respire {

stdext::optional<std::vector<FileInfoNodeOutput>> ParseDeps(
    Environment* env, LockedNodeStorage* locked_node_storage,
    FileInfoNodeOutput deps_node, ebb::lib::JSONPathStringView filename) {
  FileInfoNode::FuturePtr deps_node_future(deps_node.node->GetFileInfo());
  const FileOutput& deps_node_result = *deps_node_future->GetValue();

  if (deps_node_result.error()) {
    return stdext::nullopt;
  }

  std::ifstream in(filename.AsString());
  if (!in) {
    return stdext::nullopt;
  }

  std::vector<stdext::file_system::Path> dep_paths;
  while (true) {
    std::string line;
    if (!getline(in, line)) {
      break;
    }

    dep_paths.emplace_back(line);
  }
  in.close();

  std::vector<FileInfoNodeOutput> ret;
  ret.reserve(dep_paths.size());

  LockedNodeStorage::Access access(locked_node_storage);
  for (const auto& dep_path : dep_paths) {
    ret.emplace_back(access.LookupNodeOrMakeFileExistsNode(
        env, access.AddPathString(dep_path)));
  }

  return ret;
}

}  // namespace respire
