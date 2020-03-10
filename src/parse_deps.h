#ifndef __RESPIRE_PARSE_DEPS_H__
#define __RESPIRE_PARSE_DEPS_H__

#include <vector>

#include "environment.h"
#include "file_info_node.h"
#include "locked_node_storage.h"
#include "stdext/optional.h"

namespace respire {

stdext::optional<std::vector<FileInfoNodeOutput>> ParseDeps(
    Environment* env, LockedNodeStorage* locked_node_storage,
    FileInfoNodeOutput deps_node, ebb::lib::JSONPathStringView filename);

}  // namespace respire

#endif  // __RESPIRE_PARSE_DEPS_H__
