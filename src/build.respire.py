from collections import namedtuple
import os
import sys

import respire.buildlib.cc as cc
import respire.buildlib.cc_toolchains.raspi as raspi
import respire.buildlib.cc_toolchains.discovery as cc_discovery
import respire.buildlib.copy_files as copy_files
import respire.buildlib.modules as modules
import respire.buildlib.run_with_timestamp as run_with_timestamp
import respire.buildlib.touch as touch


_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))


def EntryPoint(registry, out_dir, target, config, platform):
  if platform == 'raspi':
    if 'RASPI_TOOLS' not in os.environ:
      print('Set the RASPI_TOOLS environment variable to the location that ' +
            'you cloned https://github.com/raspberrypi/tools in to.')
      assert False
    toolchain = raspi.RaspberryPiToolchain(os.environ['RASPI_TOOLS'])
  else:
    assert platform == 'host'
    toolchain = cc_discovery.DiscoverHostToolchain()

  if config == 'debug':
    out_dir = os.path.join(out_dir, 'debug')
    configuration = cc.Configuration(optimize='None', include_symbols=True)
  elif config == 'release':
    out_dir = os.path.join(out_dir, 'release')
    configuration = cc.Configuration(optimize='Maximum', include_symbols=False)

  configured_toolchain = cc.ToolchainWithConfiguration(toolchain, configuration)

  googletest_modules = registry.SubRespireExternal(
      'ebb/src/stdext/src/third_party/googletest/build.respire.py', 'Build',
      out_dir=os.path.join(out_dir, 'googletest'),
      configured_toolchain=configured_toolchain)

  stdext_modules = registry.SubRespireExternal(
      'ebb/src/stdext/src/build.respire.py', 'Build',
      out_dir=os.path.join(out_dir, 'stdext'),
      configured_toolchain=configured_toolchain,
      googletest_modules=googletest_modules)

  ebb_outputs = registry.SubRespireExternal(
      'ebb/src/build.respire.py', 'Build', out_dir=os.path.join(out_dir, 'ebb'),
      configured_toolchain=configured_toolchain,
      stdext_modules=stdext_modules,
      googletest_modules=googletest_modules)

  build_outputs = registry.SubRespire(
      Build, out_dir=out_dir, configured_toolchain=configured_toolchain,
      googletest_modules=googletest_modules,
      ebb_outputs=ebb_outputs,
      stdext_modules=stdext_modules)

  registry.SubRespire(StartBuilds, respire_build_targets=build_outputs,
                      ebb_build_targets=ebb_outputs,
                      stdext_build_targets=stdext_modules, target=target)


def Build(registry, out_dir, configured_toolchain, googletest_modules,
          ebb_outputs, stdext_modules):
  gtest_main_module = googletest_modules['gtest_main']
  ebb_module = ebb_outputs['ebb_lib']
  stdext_module = stdext_modules['stdext_lib']

  if not os.path.exists(out_dir):
    os.makedirs(out_dir)

  core_lib = registry.SubRespire(
      BuildCoreLib, out_dir=out_dir, configured_toolchain=configured_toolchain,
      ebb_module=ebb_module, stdext_module=stdext_module)

  respire_executable = registry.SubRespire(
      BuildRespireExecutable, out_dir=out_dir,
      configured_toolchain=configured_toolchain, core_lib=core_lib)

  respire_cpp_tests = registry.SubRespire(
      BuildRespireCPPTests, out_dir=out_dir,
      configured_toolchain=configured_toolchain,
      gtest_main_module=gtest_main_module, core_lib=core_lib)

  run_cpp_tests_stamp = registry.SubRespire(
      RunCPPTests, out_dir=out_dir, respire_cpp_tests=respire_cpp_tests)

  package = registry.SubRespire(
      PackageRespire, out_dir=os.path.join(out_dir, 'package'),
      respire_executable=respire_executable, include_tests=False)

  run_end_to_end_tests_stamp = registry.SubRespire(
      RunEndToEndTests, out_dir=out_dir, respire_executable=respire_executable)

  return {
    'executable': respire_executable,
    'cpp_tests': respire_cpp_tests,
    'run_cpp_tests': run_cpp_tests_stamp,
    'package': package,
    'run_e2e_tests': run_end_to_end_tests_stamp,
  }


def BuildCoreLib(registry, out_dir, configured_toolchain, ebb_module,
                 stdext_module):
  return modules.StaticLibraryModule(
      'respire_lib', registry, out_dir, configured_toolchain,
      sources=[
        'activity_log.cc',
        'activity_log.h',
        'build_targets.cc',
        'build_targets.h',
        'environment.cc',
        'environment.h',
        'error.h',
        'file_exists_node.cc',
        'file_exists_node.h',
        'file_info_node.h',
        'file_process_node.cc',
        'file_process_node.h',
        'future.h',
        'locked_node_storage.cc',
        'locked_node_storage.h',
        'parse_deps.cc',
        'parse_deps.h',
        'property_tree/dictionary_node.h',
        'property_tree/node.h',
        'property_tree/string_node.h',
        'registry_node.cc',
        'registry_node.h',
        'registry_parser.cc',
        'registry_parser.h',
        'registry_processor.cc',
        'registry_processor.h',
        'system_command_node.cc',
        'system_command_node.h',
      ],
      module_dependencies=[ebb_module, stdext_module])


def BuildRespireExecutable(registry, out_dir, configured_toolchain, core_lib):
  return modules.ExecutableModule(
      'respire', registry, out_dir, configured_toolchain,
      sources=[
        'main.cc',
      ],
      module_dependencies=[
        core_lib,
      ])


def BuildRespireCPPTests(
    registry, out_dir, configured_toolchain, gtest_main_module, core_lib):
  return modules.ExecutableModule(
      'respire_tests', registry, out_dir, configured_toolchain,
      sources = [
        'build_targets_test.cc',
        'file_exists_node_test.cc',
        'file_process_node_test.cc',
        'registry_parser_test.cc',
        'test_system_command.cc',
      ],
      module_dependencies=[
        gtest_main_module,
        core_lib,
      ])


def RunCPPTests(registry, out_dir, respire_cpp_tests):
  run_respire_tests_timestamp_file = os.path.join(
      out_dir, 'respire_tests.timestamp')
  registry.PythonFunction(
      inputs=[respire_cpp_tests.GetOutputFiles()[0]],
      outputs=[run_respire_tests_timestamp_file],
      function=run_with_timestamp.RunAndTimestampOnSuccess,
      timestamp_file=run_respire_tests_timestamp_file,
      command=[respire_cpp_tests.GetOutputFiles()[0]])
  return run_respire_tests_timestamp_file


PYTHON_SRC_DIR = os.path.join(_SCRIPT_DIR, 'python/')
SOURCE_FILE_EXTENSIONS = ['.py', '.cmd', '.html', '.css', '.js', '.txt']


def _FilterSourceFiles(x):
  return (os.path.splitext(x)[1] in SOURCE_FILE_EXTENSIONS)


def _FilterSourceFilesNoTests(x):
  def IsTestFile(filepath):
    if 'test' in os.path.relpath(filepath, PYTHON_SRC_DIR):
      return True

  return _FilterSourceFiles(x) and not IsTestFile(x)


def PackageRespire(registry, out_dir, respire_executable, include_tests):
  respire_package_token = out_dir + 'all.package.stamp'
  respire_package_python_token = out_dir + 'python.package.stamp'
  respire_package_exe_token = out_dir + 'exe.package.stamp'
  copy_files.CopyFiles(
      registry,
      inputs=[PYTHON_SRC_DIR],
      output_dir=out_dir,
      stamp_file=respire_package_python_token,
      filter=_FilterSourceFiles if include_tests else _FilterSourceFilesNoTests)
  copy_files.CopyFiles(
      registry,
      inputs=respire_executable.GetOutputFiles(),
      output_dir=out_dir,
      stamp_file=respire_package_exe_token)
  registry.PythonFunction(
      inputs=[respire_package_python_token, respire_package_exe_token],
      outputs=[respire_package_token],
      function=touch.Touch)
  return respire_package_token


def RunEndToEndTests(registry, out_dir, respire_executable):
  package_with_tests_dir = os.path.join(out_dir, 'package_with_tests')

  # In order to run the Python tests, because they include end-to-end tests,
  # we need a full packaged setup of the   
  package_with_tests = registry.SubRespire(
      PackageRespire, out_dir=package_with_tests_dir,
      respire_executable=respire_executable, include_tests=True)

  return registry.SubRespire(
      RunEndToEndTestsWithPackage, out_dir=out_dir,
      package_with_tests=package_with_tests,
      package_with_tests_dir=package_with_tests_dir)


def RunEndToEndTestsWithPackage(
    registry, out_dir, package_with_tests, package_with_tests_dir):
  run_end_to_end_tests_timestamp_file = os.path.join(
      out_dir, 'end_to_end_tests.timestamp')

  registry.PythonFunction(
      inputs=[package_with_tests],
      outputs=[run_end_to_end_tests_timestamp_file],
      function=run_with_timestamp.RunAndTimestampOnSuccess,
      timestamp_file=run_end_to_end_tests_timestamp_file,
      command=[sys.executable, '-m', 'unittest', 'discover', '-s',
               os.path.join(package_with_tests_dir, 'end_to_end_tests')])

  return run_end_to_end_tests_timestamp_file


def StartBuilds(registry, respire_build_targets, ebb_build_targets,
                stdext_build_targets, target):
  if target in ['all', 'build']:
    for output_file in \
        respire_build_targets['executable'].GetOutputFiles():
      registry.Build(output_file)
  if target in ['all', 'run_cpp_tests']:
    registry.Build(respire_build_targets['run_cpp_tests'])
    registry.Build(ebb_build_targets['run_ebb_tests'])
    registry.Build(stdext_build_targets['run_platform_tests'])
    registry.Build(stdext_build_targets['run_stdext_tests'])
  if target in ['all', 'package']:
    registry.Build(respire_build_targets['package'])
  if target in ['all', 'run_e2e_tests']:
    registry.Build(respire_build_targets['run_e2e_tests'])
