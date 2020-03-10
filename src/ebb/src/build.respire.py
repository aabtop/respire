from collections import namedtuple
import os
import sys

import respire.buildlib.cc as cc
import respire.buildlib.cc_toolchains.discovery as cc_discovery
import respire.buildlib.modules as modules
import respire.buildlib.run_with_timestamp as run_with_timestamp

def EntryPoint(registry, out_dir):
  toolchain = cc_discovery.DiscoverHostToolchain()
  configuration = cc.Configuration(optimize='Maximum')

  configured_toolchain = cc.ToolchainWithConfiguration(toolchain, configuration)

  googletest_modules = registry.SubRespireExternal(
      'stdext/src/third_party/googletest/build.respire.py', 'Build',
      out_dir=os.path.join(out_dir, 'googletest'),
      configured_toolchain=configured_toolchain)

  stdext_modules = registry.SubRespireExternal(
      'stdext/src/build.respire.py', 'Build',
      out_dir=os.path.join(out_dir, 'stdext'),
      configured_toolchain=configured_toolchain,
      googletest_modules=googletest_modules)

  build_outputs = registry.SubRespire(
      Build, out_dir=out_dir, configured_toolchain=configured_toolchain,
      stdext_modules=stdext_modules,
      googletest_modules=googletest_modules)

  registry.SubRespire(StartBuilds, build_targets=build_outputs,
                      stdext_modules=stdext_modules)


def Build(registry, out_dir, configured_toolchain,
          stdext_modules, googletest_modules=None):
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  assert(googletest_modules)

  ebb_lib = modules.StaticLibraryModule(
      'ebb_lib', registry, out_dir, configured_toolchain,
      sources=[
        'consumer.cc',
        'ebb.h',
        'environment.cc',
        'fiber_condition_variable.cc',
        'fiber_condition_variable.h',
        'lib/file_reader.cc',
        'lib/file_reader.h',
        'lib/json_string_view.cc',
        'lib/json_string_view.h',
        'lib/json_tokenizer.cc',
        'lib/json_tokenizer.h',
        'linked_list.h',
        'queue.cc',
        'thread_pool.cc',
        'thread_pool.h',
      ],
      public_include_paths=['.'],
      module_dependencies=[stdext_modules['stdext_lib']])

  ebb_tests = modules.ExecutableModule(
      'ebb_tests', registry, out_dir, configured_toolchain,
      sources = [
        'consumer_test.cc',
        'environment_test.cc',
        'queue_test.cc',
        'push_pull_consumer_test.cc',
        'lib/json_tokenizer_test.cc',
        'lib/file_reader_test.cc',
      ],
      module_dependencies=[
        ebb_lib,
        googletest_modules['gtest_main'],
      ])

  run_ebb_tests_timestamp_file = os.path.join(out_dir, 'ebb_tests.timestamp')
  registry.PythonFunction(
      inputs=[ebb_tests.GetOutputFiles()[0]],
      outputs=[run_ebb_tests_timestamp_file],
      function=run_with_timestamp.RunAndTimestampOnSuccess,
      timestamp_file=run_ebb_tests_timestamp_file,
      command=[ebb_tests.GetOutputFiles()[0]])

  return {
    'ebb_lib': ebb_lib,
    'ebb_tests': ebb_tests,
    'run_ebb_tests': run_ebb_tests_timestamp_file,
  }


def StartBuilds(registry, build_targets, stdext_modules):
  for output_file in build_targets['ebb_lib'].GetOutputFiles():
    registry.Build(output_file)

  registry.Build(build_targets['run_ebb_tests'])

  registry.Build(stdext_modules['run_stdext_tests'])

  registry.Build(stdext_modules['run_platform_tests'])
