cmake_minimum_required (VERSION 3.2)
project (ebb)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

if(WIN32)
  # Disable "mutliple constructors" warning, because we need to explicitly
  # declare the non-const reference constructor for the C++11 version of
  # stdext::variant.
  ADD_COMPILE_OPTIONS(/wd4521)
endif(WIN32)

set(CORE_LIB_HEADERS
  ebb.h
  fiber_condition_variable.h
  lib/file_reader.h
  lib/json_string_view.h
  lib/json_tokenizer.h
  linked_list.h
  stdext/src/stdext/align.h
  stdext/src/stdext/murmurhash/MurmurHash3.h
  stdext/src/stdext/numeric.h
  stdext/src/stdext/optional.h
  stdext/src/stdext/span.h
  stdext/src/stdext/string_view.h
  stdext/src/stdext/type_id.h
  stdext/src/stdext/types.h
  stdext/src/stdext/variant.h
  thread_pool.h
)
set(CORE_LIB_SRCS
  consumer.cc
  environment.cc
  fiber_condition_variable.cc
  queue.cc
  lib/file_reader.cc
  lib/json_string_view.cc
  lib/json_tokenizer.cc
  stdext/src/stdext/murmurhash/MurmurHash3.cpp
  thread_pool.cc
)

set(PLATFORM_LIB_HEADERS
  stdext/src/platform/context.h
  stdext/src/platform/file_system.h
  stdext/src/platform/subprocess.h
)

if(WIN32)
  set(PLATFORM_LIB_SRCS
    stdext/src/platform/win32/context.cc
    stdext/src/platform/win32/file_system.cc
    stdext/src/platform/win32/subprocess.cc
  )
else(WIN32)
  set(PLATFORM_LIB_SRCS
    stdext/src/platform/posix/context.cc
    stdext/src/platform/posix/subprocess.cc
    stdext/src/platform/unix/file_system.cc
  )
endif(WIN32)


set(ebb_INCLUDE_DIRS
  "${PROJECT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/stdext/src"
)

include_directories("${ebb_INCLUDE_DIRS}")
add_library(ebb
    ${CORE_LIB_SRCS}
    ${CORE_LIB_HEADERS}
    ${PLATFORM_LIB_SRCS}
    ${PLATFORM_LIB_HEADERS})

target_link_libraries(ebb Threads::Threads)

target_include_directories(ebb PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)

export(TARGETS ebb FILE ebbConfig.cmake)


################################################################################
# Unit tests
################################################################################

set(UNIT_TEST_SRCS
  consumer_test.cc
  environment_test.cc
  queue_test.cc
  push_pull_consumer_test.cc
  lib/json_tokenizer_test.cc
  lib/file_reader_test.cc
  stdext/src/stdext/file_system_test.cc
  stdext/src/stdext/span_test.cc
  stdext/src/stdext/variant_test.cc
)

set(PLATFORM_UNIT_TEST_SRCS
  stdext/src/platform/context_test.cc)

# Override gtest's  aults.
SET(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject")
SET(BUILD_GMOCK ON CACHE BOOL "Builds the googlemock subproject")
SET(INSTALL_GTEST OFF CACHE BOOL "Enable installation of googletest.")
SET(INSTALL_GMOCK OFF CACHE BOOL "Enable installation of googlemock.")
add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
SET(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) run-time lib even when Google Test is built as static lib.")

enable_testing()
add_subdirectory(stdext/src/third_party/googletest)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
include_directories(${ebb_INCLUDE_DIR})
link_directories(${ebb_BINARY_DIR})
link_directories(${gtest_BINARY_DIR})

add_executable(ebb_tests ${UNIT_TEST_SRCS} ${PLATFORM_UNIT_TEST_SRCS})
target_link_libraries(ebb_tests ebb gtest gtest_main)

add_test(EbbUnitTests ebb_tests)
