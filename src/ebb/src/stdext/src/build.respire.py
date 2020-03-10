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
      'third_party/googletest/build.respire.py', 'Build',
      out_dir=os.path.join(out_dir, 'googletest'),
      configured_toolchain=configured_toolchain)

  build_outputs = registry.SubRespire(
      Build, out_dir=out_dir, configured_toolchain=configured_toolchain,
      googletest_modules=googletest_modules)

  registry.SubRespire(StartBuilds, build_targets=build_outputs)


def Build(registry, out_dir, configured_toolchain, platform='host',
          googletest_modules=None):
  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  if platform == 'host':
    platform = sys.platform

  if platform == 'win32':
    platform_sources = [
      'platform/win32/context.cc',
      'platform/win32/file_system.cc',
      'platform/win32/subprocess.cc',
    ]
  elif platform == 'raspi' or 'linux' in platform or platform == 'jetson':
    platform_sources = [
      'platform/posix/context.cc',
      'platform/posix/subprocess.cc',
      'platform/unix/file_system.cc',
    ]

  platform_lib = modules.StaticLibraryModule(
      'platform_lib', registry, out_dir, configured_toolchain,
      sources=[
        'platform/context.h',
        'platform/file_system.h',
        'platform/subprocess.h',
      ] + platform_sources,
      public_include_paths=['.'])

  stdext_lib = modules.StaticLibraryModule(
      'stdext_lib', registry, out_dir, configured_toolchain,
      sources=[
        'stdext/align.h',
        'stdext/murmurhash/MurmurHash3.cpp',
        'stdext/murmurhash/MurmurHash3.h',
        'stdext/numeric.h',
        'stdext/optional.h',
        'stdext/span.h',
        'stdext/string_view.h',
        'stdext/type_id.h',
        'stdext/types.h',
        'stdext/variant.h',
      ],
      public_include_paths=['.'],
      module_dependencies=[platform_lib])

  if googletest_modules:
    stdext_tests = modules.ExecutableModule(
        'stdext_tests', registry, out_dir, configured_toolchain,
        sources = [
          'stdext/file_system_test.cc',
          'stdext/span_test.cc',
          'stdext/variant_test.cc',
        ],
        module_dependencies=[
          stdext_lib,
          googletest_modules['gtest_main'],
        ])

    run_stdext_tests_timestamp_file = os.path.join(
        out_dir, 'stdext_tests.timestamp')
    registry.PythonFunction(
        inputs=[stdext_tests.GetOutputFiles()[0]],
        outputs=[run_stdext_tests_timestamp_file],
        function=run_with_timestamp.RunAndTimestampOnSuccess,
        timestamp_file=run_stdext_tests_timestamp_file,
        command=[stdext_tests.GetOutputFiles()[0]])

    platform_tests = modules.ExecutableModule(
        'platform_tests', registry, out_dir, configured_toolchain,
        sources = [
          'platform/context_test.cc',
        ],
        module_dependencies=[
          platform_lib,
          googletest_modules['gtest_main'],
        ])

    run_platform_tests_timestamp_file = os.path.join(
        out_dir, 'platform_tests.timestamp')
    registry.PythonFunction(
        inputs=[platform_tests.GetOutputFiles()[0]],
        outputs=[run_platform_tests_timestamp_file],
        function=run_with_timestamp.RunAndTimestampOnSuccess,
        timestamp_file=run_platform_tests_timestamp_file,
        command=[platform_tests.GetOutputFiles()[0]])

  output_modules = {'stdext_lib': stdext_lib}
  if googletest_modules:
    output_modules.update({
      'stdext_tests': stdext_tests,
      'run_stdext_tests': run_stdext_tests_timestamp_file,
      'platform_tests': platform_tests,
      'run_platform_tests': run_platform_tests_timestamp_file,
    })

  return output_modules

def StartBuilds(registry, build_targets):
  for output_file in build_targets['stdext_lib'].GetOutputFiles():
    registry.Build(output_file)

  registry.Build(build_targets['run_stdext_tests'])

  registry.Build(build_targets['run_platform_tests'])
