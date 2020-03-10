#ifndef __RESPIRE_BUILD_TARGETS_H__
#define __RESPIRE_BUILD_TARGETS_H__

#include <vector>

#include "environment.h"
#include "error.h"
#include "stdext/file_system.h"

namespace respire {

OptionalError BuildTargets(
    respire::Environment* env,
    stdext::file_system::PathStrRef initial_registry_path);

}  // namespace respire

#endif  // __RESPIRE_BUILD_TARGETS_H__
