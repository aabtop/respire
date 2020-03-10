'''Wraps calls to python functions invoked through the respire registry.

This function wraps all Python functions executed via Registry.PythonFunction.
Its tasks mostly involve resolving filenames and deserializing parameters.
'''


from os import path
import respire_python_wrapper_helpers
import sys

import json_utils


class _ParsedPythonFunctionRunnerCommandLineParameters(object):
  def __init__(self):
    assert(len(sys.argv) == 5)
    self.module_filepath = sys.argv[1]
    self.function_name = sys.argv[2]
    self.params_filepath = sys.argv[3]
    self.deps_filepath = sys.argv[4]


def main():
  args = _ParsedPythonFunctionRunnerCommandLineParameters()

  # Make sure that the path has the target script's directory in it first thing.
  sys.path.insert(0, path.dirname(args.module_filepath))

  # Make sure that all modules have access to the respire standard build
  # library.
  sys.path.insert(0, path.join(path.dirname(__file__), 'buildlib'))

  # Load in the module containing the function to execute.
  function_module = respire_python_wrapper_helpers.LoadSourceModule(
      'python_function_module', args.module_filepath)

  # Get a reference to the target function within the module.
  if not hasattr(function_module, args.function_name):
    raise Exception(
        'Requested respire function, "%s", not found in build file: %s.'
        % (args.function_name, args.module_filepath))
  function = getattr(function_module, args.function_name)

  # Read the parameters for the function.
  with open(args.params_filepath, 'r') as f:
    params_content = f.read()
  params = json_utils.DecodeFromJSON(params_content)

  # Execute the function.
  exit_code = function(**params)

  # Discover and write out the deps files for this Python script.
  deps_file_content = '\n'.join(
      respire_python_wrapper_helpers
          .GetCurrentNonBuiltInImportDependencySources())
  respire_python_wrapper_helpers.WriteToFileIfContentsDiffer(
      args.deps_filepath, deps_file_content)

  # Return the functions exit code.
  return exit_code


if __name__ == "__main__":
  sys.exit(main())
