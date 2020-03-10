cmake_minimum_required (VERSION 3.2)
project (respire)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

add_subdirectory(ebb/src)
include_directories("ebb/src/stdext/src")

if(WIN32)
  # Disable "mutliple constructors" warning, because we need to explicitly
  # declare the non-const reference constructor for the C++11 version of
  # stdext::variant.
  ADD_COMPILE_OPTIONS(/wd4521)
endif(WIN32)

set(CORE_LIB_HEADERS
  activity_log.h
  build_targets.h
  environment.h
  error.h
  file_exists_node.h
  file_info_node.h
  file_process_node.h
  future.h
  locked_node_storage.h
  parse_deps.h
  registry_node.h
  registry_parser.h
  registry_processor.h
  system_command_node.h
)
set(CORE_LIB_SRCS
  activity_log.cc
  build_targets.cc
  environment.cc
  file_exists_node.cc
  file_process_node.cc
  locked_node_storage.cc
  parse_deps.cc
  registry_node.cc
  registry_parser.cc
  registry_processor.cc
  system_command_node.cc
)

set(respire_INCLUDE_DIR "${PROJECT_SOURCE_DIR}")
include_directories("$(respire_INCLUDE_DIR)")
add_library(respire_lib ${CORE_LIB_SRCS} ${CORE_LIB_HEADERS})

target_link_libraries(respire_lib ebb)

set(CORE_EXE_SRCS
  main.cc
)
set(CORE_EXE_HEADERS  
)

add_executable(respire ${CORE_EXE_SRCS} ${CORE_EXE_HEADERS})
target_link_libraries(respire respire_lib Threads::Threads)

# Package Python source files with generated respire executable.
set(respire_python_package_dir package/python)
install(TARGETS respire DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/package/python)
install(DIRECTORY python DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/package
        PATTERN *.pyc EXCLUDE)

################################################################################
# Unit tests
################################################################################

set(UNIT_TEST_SRCS
  build_targets_test.cc
  file_exists_node_test.cc
  file_process_node_test.cc
  registry_parser_test.cc
  test_system_command.cc
)

# Override gtest's  aults.
SET(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject")
SET(BUILD_GMOCK ON CACHE BOOL "Builds the googlemock subproject")
SET(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest.")
SET(INSTALL_GMOCK OFF CACHE BOOL "Enable installation of googlemock.")
add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
SET(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) run-time lib even when Google Test is built as static lib.")

enable_testing()

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
include_directories(${respire_INCLUDE_DIR})
link_directories(${respire_BINARY_DIR})
link_directories(${gtest_BINARY_DIR})

add_executable(respire_tests ${UNIT_TEST_SRCS})
target_link_libraries(respire_tests respire_lib gtest gtest_main)

add_test(respireUnitTests respire_tests)
