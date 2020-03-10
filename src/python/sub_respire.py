from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import inspect
import os
import sys

if sys.version_info[0] == 3:
  import importlib.util
else:
  import imp

import json_utils
from registry import Registry
import registry_helpers
import respire_python_wrapper_helpers


def _CopyCommand(output_filepath, flattened_output_filepath):
  # TODO: We just call in to _FlattenCommand() for now because it's easy, but
  #       if running all this python is slow, we can do better by choosing a
  #       platform-specific copy command.
  return _FlattenCommand(output_filepath, flattened_output_filepath)


def _FlattenCommand(output_filepath, flattened_output_filepath):
  return [sys.executable, _GetFlattenScriptPath(), output_filepath,
          flattened_output_filepath]


def _GetFlattenScriptPath():
  return os.path.abspath(os.path.join(os.path.dirname(__file__),
                                      'flatten_output.py'))


def _GenerateSystemCommandForFlattenedOutput(
    registry, out_futures, output_filepath, flattened_output_filepath):
  if not out_futures:
    # If we know that there are no futures, then there is nothing to flatten
    # and we can just do a copy instead.
    command = _CopyCommand(output_filepath, flattened_output_filepath)
  else:
    command = _FlattenCommand(output_filepath, flattened_output_filepath)

  registry.SystemCommand(
      inputs=([x.ValueFilepath() for x in out_futures] + [output_filepath]),
      outputs=[flattened_output_filepath],
      command=command)


def _IsFunctionParameter(param_name, function):
  if sys.version_info[0] == 3:
    return param_name in inspect.signature(function).parameters
  else:
    return param_name in inspect.getargspec(function)[0]


def SubRespire(build_filepath, params_file, out_dir, function_name,
               timestamp_file=None):
  with open(params_file, 'r') as f:
    params_content = f.read()

  registry_files_basename = registry_helpers.GetHashedBaseFilename(
            build_filepath, function_name, params_content)

  respire_filepaths = registry_helpers.GetSubRespireFilepaths(
      out_dir, registry_files_basename)

  build_dir = os.path.dirname(build_filepath)

  # Make sure that the path has the current respire module's directory in it
  # first thing.
  sys.path.insert(0, build_dir)

  # Change the system current directory to the build file's directory to make
  # it so that build files can be more hermetic in assuming the cwd is the
  # directory they reside in.
  original_cwd = os.getcwd()
  os.chdir(build_dir)

  # Make sure that all modules have access to the respire standard build
  # library.
  sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'buildlib'))

  build_module = None
  if sys.version_info[0] == 3:
    build_spec = importlib.util.spec_from_file_location(
        'respire_build', build_filepath)
    build_module = importlib.util.module_from_spec(build_spec)
    sys.modules[build_spec.name] = build_module
    build_spec.loader.exec_module(build_module)
  else:
    build_module = imp.load_source('respire_build', build_filepath)

  registry = Registry(out_dir, build_filepath, build_module, respire_filepaths)

  params = json_utils.DecodeFromJSONWithFlattening(params_content)
  if not hasattr(build_module, function_name):
    raise Exception(
        'Requested respire function, "%s", not found in build file: %s.'
        % (function_name, build_filepath))
  function = getattr(build_module, function_name)
  if not _IsFunctionParameter('registry', function):
    raise Exception(
        'You may only pass functions with a "registry" parameter to '
        'SubRespire().')

  out_params = function(registry=registry, **params)
  (out_params_content, out_futures) = json_utils.EncodeToJSON(out_params)

  _GenerateSystemCommandForFlattenedOutput(
      registry, out_futures, respire_filepaths.output_filepath,
      respire_filepaths.flattened_output_filepath)

  if timestamp_file:
    # Timestamp the timestamp file!
    open(timestamp_file, 'w').close()

  registry_contents = registry.CompileToString()

  respire_python_wrapper_helpers.WriteToFileIfContentsDiffer(
      respire_filepaths.registry_filepath, registry_contents)

  respire_python_wrapper_helpers.WriteToFileIfContentsDiffer(
      respire_filepaths.output_filepath, out_params_content)

  deps = (respire_python_wrapper_helpers
              .GetCurrentNonBuiltInImportDependencySources() +
          registry.deps)

  deps_file_content = '\n'.join(deps)

  respire_python_wrapper_helpers.WriteToFileIfContentsDiffer(
      respire_filepaths.deps_filepath, deps_file_content)

  os.chdir(original_cwd)

  return True


def main():
  args = registry_helpers.ParseRespireCommandLineArgs()
  if SubRespire(args.build_file, args.params_file, args.out_dir, args.function,
                args.timestamp_file):
    return 0
  else:
    return 1


if __name__ == "__main__":
  sys.exit(main())
