#include "build_targets.h"

#include <sstream>

#include "environment.h"
#include "error.h"
#include "file_exists_node.h"
#include "file_info_node.h"
#include "future.h"
#include "locked_node_storage.h"
#include "registry_node.h"
#include "stdext/file_system.h"
#include "stdext/variant.h"

namespace respire {

OptionalError BuildTargets(
    Environment* env,
    stdext::file_system::PathStrRef initial_registry_path) {
  // Convert the input initial registry path to JSON formatting because that's
  // the format the rest of the Respire system expects it in.
  std::string path_as_json = ebb::lib::ToJSON(
      initial_registry_path.c_str());
  ebb::lib::JSONPathStringView initial_registry_path_string_view(
      ebb::lib::JSONStringView(path_as_json.data(), path_as_json.size()));

  FileExistsNode initial_file_exists_node(
      env, initial_registry_path_string_view);
  LockedNodeStorage locked_node_storage;
  RegistryNode initial_registry_node(
      env, FileInfoNodeOutput(&initial_file_exists_node, 0),
      initial_registry_path_string_view, &locked_node_storage);

  return *initial_registry_node.PopulateLockedNodeStorage(nullptr)->GetValue();
}

}  // namespace respire
