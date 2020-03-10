from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import hashlib
import inspect  
import os
import registry_helpers
import string
import sys
import utils

import json_utils
from buildlib.respire.buildlib.utils import AssertIsValidRespireFunction
from buildlib.respire.buildlib.utils import ResolveFilepathRelativeToCallerScript
from respire_builder import RespireBuilder
from respire_builder import CheckListOfStrings
from respire_future import RespireFuture
import python_function_main


class Registry(object):
  def __init__(
      self, out_dir, build_filepath, build_module, respire_filepaths):
    self.out_dir = out_dir
    self.build_filepath = build_filepath
    self.build_module = build_module
    self.respire_filepaths = respire_filepaths
    self.deps = []

    self.builder = RespireBuilder()

  def RegisterSelfDependency(self, dep):
    '''Registers the given |dep| file as a dependency of this build script.'''
    self.deps.append(dep)

  def SystemCommand(self, inputs, outputs, command, soft_outputs=None,
                    deps=None, stdout=None, stderr=None, stdin=None):
    CheckListOfStrings(inputs)
    inputs = [ResolveFilepathRelativeToCallerScript(x) for x in inputs]
    (stderr, stdout) = registry_helpers.FillStdErrAndStdOutForLoggingIfEmpty(
        self.out_dir, command, stderr, stdout)

    self.builder.AddSystemCommand(
        inputs + [self.respire_filepaths.registry_filepath],
        outputs, command, soft_outputs, deps, stdout, stderr, stdin)

  def PythonFunction(self, inputs, outputs, function, soft_outputs=None,
                     stdout=None, stderr=None, stdin=None, **kwargs):
    CheckListOfStrings(inputs)

    # If the function's keyword arguments contain any of the input parameters
    # to this function, forward them along.
    (function_positional_args, _, function_keyword_args, _) = (
        inspect.getargspec(function))
    function_args = (
        (function_positional_args or []) + (function_keyword_args or []))
    params = kwargs.copy()
    if function_args:
      if 'inputs' in function_args:
        params['inputs'] = inputs
      if 'outputs' in function_args:
        params['outputs'] = outputs
      if 'soft_outputs' in function_args:
        params['soft_outputs'] = outputs

    (command, deps_file) = (
        self._MakeSystemCommandForPythonFunctionCall(function, params))

    (stderr, stdout) = registry_helpers.FillStdErrAndStdOutForLoggingIfEmpty(
        self.out_dir, command, stderr, stdout)

    self.builder.AddSystemCommand(
        inputs + [self.respire_filepaths.registry_filepath],
        outputs, command, soft_outputs=soft_outputs, deps=deps_file,
        stdout=stdout, stderr=stderr, stdin=stdin)

  def SubRespire(self, function, additional_deps=[], **kwargs):
    AssertIsValidRespireFunction(function)
    function_module = sys.modules[function.__module__]
    module_filepath = inspect.getsourcefile(function_module)
    if 'registry' not in inspect.getargspec(function)[0]:
      raise Exception(
          'You may only pass functions with a "registry" parameter to '
          'SubRespire().')

    return self.SubRespireExternal(
        module_filepath, function.__name__, additional_deps=additional_deps,
        **kwargs)

  def SubRespireExternal(self, build_filepath, build_function_name,
                         additional_deps=[], **kwargs):
    if not os.path.isabs(build_filepath):
      # Make any relative paths relative to the source respire module that is
      # declaring them.
      build_filepath = os.path.join(os.path.dirname(self.build_filepath),
                                    build_filepath)

    (params_content, futures) = json_utils.EncodeToJSON(kwargs)

    # Check to see if the parameters contain any futures.  If they do, then
    # we insert a layer of indirection to resolve the futures first, so that
    # we can take a hash of |kwargs| using the resolved futures, allowing us
    # to coalesce registry computations for the same parameters regardless of
    # whether those parameters are specified explicitly or via futures.
    if futures:
      return self._SubRespireExternalPrivate(
          inspect.getsourcefile(sys.modules[__name__]), '_ResolveFutures',
          {'forward_filepath': build_filepath,
           'forward_function': build_function_name,
           'forward_params': kwargs,
           'forward_additional_deps': additional_deps})
    else:
      return self._SubRespireExternalPrivate(
          build_filepath, build_function_name, kwargs,
          additional_deps=additional_deps)

  def Build(self, target_filepath):
    if not utils.IsString(target_filepath):
      raise Exception('Expected a string parameter for target_filepath.')

    self.builder.AddBuild(target_filepath)

  def _SubRespireExternalPrivate(
      self, build_filepath, build_function_name, params, additional_deps=[]):
    sub_respire_filepaths = registry_helpers.EnsureGenRegistryInputFilesExist(
        self.out_dir, build_filepath, build_function_name, params,
        additional_deps)
    
    self.builder.AddInclude(sub_respire_filepaths.gen_registry_filepath)

    return RespireFuture(sub_respire_filepaths.flattened_output_filepath,
                         sub_respire_filepaths.gen_registry_filepath,
                         build_filepath, build_function_name)

  def _MakeSystemCommandForPythonFunctionCall(self, function, params):
    AssertIsValidRespireFunction(function)
    module_filepath = inspect.getsourcefile(sys.modules[function.__module__])

    (params_content, futures) = json_utils.EncodeToJSON(params)
    if futures:
      raise Exception('Passing futures as parameters to PythonFunctions is '
                      'not yet supported.')
  
    filepaths = _GetPythonFunctionCallFilePaths(
        self.out_dir, module_filepath, function.__name__,
        params_content)

    registry_helpers._EnsureUniqueFileWithContentsExists(
          filepaths.params, params_content)

    # Setup a function call that calls our "python function runner" Python
    # script, which would in turn deserialize the parameters and call the
    # desired function with them.
    # The '-B' option is used to avoid making ".pyc" files, which were found
    # to result in timing issues resulting in occasionally failing tests.
    command = [sys.executable, '-B',
               inspect.getsourcefile(python_function_main), module_filepath,
               function.__name__, filepaths.params, filepaths.deps]

    return (command, filepaths.deps)

  def CompileToString(self):
    return self.builder.CompileToString()

  def CompileToFile(self, out_respire_file):
    self.builder.CompileToFile(out_respire_file)


def _ResolveFutures(
    registry, forward_filepath, forward_function, forward_params,
    forward_additional_deps):
  # _SubRespireExternalPrivate() above will have been responsible already for
  # resolving all of the futures within params, however they wouldn't have
  # made it into the hash for generating this subrespire.  Now that they are
  # resolved, if we forward them to their actual destination, the resolved
  # future values will get used in the hash, allowing them to coalesce with
  # other invokationsto the same subrespire regardless of whether futures were
  # used or not.
  return registry.SubRespireExternal(
      forward_filepath, forward_function,
      additional_deps=forward_additional_deps, **forward_params)


class PythonFunctionCallFilePaths(object):
  def __init__(self, params, deps):
    self.params = params
    self.deps = deps


_PYTHON_FUNCTION_PARAMS_EXTENSION='.pythonfunction.params.json'
_PYTHON_FUNCTION_DEPS_EXTENSION='.pythonfunction.deps'


def _GetPythonFunctionCallFilePaths(
    out_dir, module_filepath, function_name, params_content):
  base_filepath = os.path.join(out_dir, registry_helpers.GetHashedBaseFilename(
      module_filepath, function_name, params_content))

  params_filepath = base_filepath + _PYTHON_FUNCTION_PARAMS_EXTENSION
  deps_filepath = base_filepath + _PYTHON_FUNCTION_DEPS_EXTENSION

  return PythonFunctionCallFilePaths(params_filepath, deps_filepath)
