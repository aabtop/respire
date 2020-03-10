from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import hashlib
import os
import string
import sys
import tempfile

import json_utils
from respire_builder import RespireBuilder

def _GetSubRespirePyPath():
  return os.path.abspath(
      os.path.join(os.path.dirname(__file__), 'sub_respire.py'))


RESPIRE_MAIN_FUNCTION_NAME = 'RespireBuild'


def FillStdErrAndStdOutForLoggingIfEmpty(out_dir, command, stderr, stdout):
  log_dir = GetCommandLogDirectory(out_dir)
  
  ret_stderr = stderr
  ret_stdout = stdout

  if not stderr:
    ret_stderr = os.path.join(log_dir,
        hashlib.sha256(' '.join(command).encode('utf-8')).hexdigest() +
                       '_stderr.txt')
  if not stdout:
    ret_stdout = os.path.join(log_dir,
        hashlib.sha256(' '.join(command).encode('utf-8')).hexdigest() +
                       '_stdout.txt')
  return (ret_stderr, ret_stdout)


def _MakeGenRegistryContents(
    build_filepath, build_function_name, out_dir, sub_respire_filepaths,
    future_deps, additional_deps):
  respire_builder = RespireBuilder()
  for future in future_deps:
    respire_builder.AddInclude(future.IncludeFilepath())
  command = [sys.executable, '-B', _GetSubRespirePyPath(), '-b',
             build_filepath, '-p', sub_respire_filepaths.params_filepath, '-o',
             out_dir, '-f', build_function_name, '-t',
             sub_respire_filepaths.timestamp_filepath]
  (stderr, stdout) = FillStdErrAndStdOutForLoggingIfEmpty(
      out_dir, command, None, None)

  respire_builder.AddSystemCommand(
      inputs=([build_filepath, sub_respire_filepaths.params_filepath] +
              [x.ValueFilepath() for x in future_deps] + additional_deps),
      outputs=[sub_respire_filepaths.timestamp_filepath],
      soft_outputs=[sub_respire_filepaths.registry_filepath,
                    sub_respire_filepaths.output_filepath],
      command=command,
      deps=sub_respire_filepaths.deps_filepath,
      stderr=stderr,
      stdout=stdout)
  respire_builder.AddInclude(sub_respire_filepaths.registry_filepath)
  return respire_builder.CompileToString()


def ParseRespireCommandLineArgs():
  parser = argparse.ArgumentParser(description='Respire build entry point.')

  def _IsInputFileValid(arg):
    if not os.path.isfile(arg):
      parser.error('The file %s does not exist!' % arg)
    else:
      return arg

  def _IsOutDirectoryValid(arg):
    if os.path.isfile(arg):
      parser.error('%s is a file, but it was expected to be a directory' % arg)
    else:
      return arg

  parser.add_argument(
      '-b', '--build_file', required=True, type=_IsInputFileValid)
  parser.add_argument(
      '-f', '--function', required=True)
  parser.add_argument(
      '-p', '--params_file', required=True, type=_IsInputFileValid)
  parser.add_argument(
      '-o', '--out_dir', required=True, type=_IsOutDirectoryValid)
  parser.add_argument('-t', '--timestamp_file')
  return parser.parse_args()


class SubRespireFilepaths(object):
  def __init__(self, params, gen_registry, registry, output, flattened_output,
               deps_output, timestamp_filepath):
    self.params_filepath = params
    self.gen_registry_filepath = gen_registry
    self.registry_filepath = registry
    self.output_filepath = output
    self.flattened_output_filepath = flattened_output
    self.deps_filepath = deps_output
    self.timestamp_filepath = timestamp_filepath


_SUB_RESPIRE_PARAMS_EXTENSION = '.respire.params.json'
_SUB_RESPIRE_GEN_REGISTRY_EXTENSION = '.respire.gen.reg'
_SUB_RESPIRE_REGISTRY_EXTENSION = '.respire.reg'
_SUB_RESPIRE_OUTPUT_EXTENSION = '.respire.output.json'
_SUB_RESPIRE_FLATTENED_OUTPUT_EXTENSION = '.respire.flattened.output.json'
_SUB_RESPIRE_DEPS_EXTENSION = '.respire.deps'
_SUB_RESPIRE_TIMESTAMP_EXTENSION = '.respire.timestamp'

_SUB_RESPIRE_EXTENSION_LENGTH_MAX = max([len(x) for x in [
    _SUB_RESPIRE_PARAMS_EXTENSION,
    _SUB_RESPIRE_GEN_REGISTRY_EXTENSION,
    _SUB_RESPIRE_REGISTRY_EXTENSION,
    _SUB_RESPIRE_OUTPUT_EXTENSION,
    _SUB_RESPIRE_FLATTENED_OUTPUT_EXTENSION,
    _SUB_RESPIRE_DEPS_EXTENSION]])


def GetSubRespireFilepaths(out_dir, basename):
  base_filepath = os.path.join(out_dir, basename)

  params_filepath = base_filepath + _SUB_RESPIRE_PARAMS_EXTENSION
  gen_registry_filepath = (
      base_filepath + _SUB_RESPIRE_GEN_REGISTRY_EXTENSION)
  registry_filepath = (
      base_filepath + _SUB_RESPIRE_REGISTRY_EXTENSION)
  output_filepath = base_filepath + _SUB_RESPIRE_OUTPUT_EXTENSION
  flattened_output_filepath = (
      base_filepath + _SUB_RESPIRE_FLATTENED_OUTPUT_EXTENSION)
  deps_filepath = (
      base_filepath + _SUB_RESPIRE_DEPS_EXTENSION)
  timestamp_filepath = base_filepath + _SUB_RESPIRE_TIMESTAMP_EXTENSION

  return SubRespireFilepaths(
      params_filepath, gen_registry_filepath, registry_filepath,
      output_filepath, flattened_output_filepath, deps_filepath,
      timestamp_filepath)


def GetHashedBaseFilename(build_filepath, build_function_name,
                          params_content_string):
  key_string = RespireIdentifier(build_filepath, build_function_name)
  key_string_with_params = key_string + ':' + params_content_string

  params_file_hash = (
      hashlib.sha256(key_string_with_params.encode('utf-8')).hexdigest())

  MAX_FILE_LENGTH = 200
  MAX_FILE_BASE_NAME_LENGTH = (
      MAX_FILE_LENGTH - _SUB_RESPIRE_EXTENSION_LENGTH_MAX - 1)

  assert(len(params_file_hash) <= MAX_FILE_BASE_NAME_LENGTH)
  FILENAME_DESCRIPTOR_LENGTH = (
      MAX_FILE_BASE_NAME_LENGTH - len(params_file_hash))

  # Thank you seanh ( https://gist.github.com/seanh/93666 )
  VALID_FILENAMES = '-_.() %s%s' % (string.ascii_letters, string.digits)
  REPLACEMENT_CHAR = '_'
  sanitized_filename = (
      [(c if c in VALID_FILENAMES else REPLACEMENT_CHAR) for c in key_string])

  # Add the actual source filename to the beginning of the hash, in order to
  # make the filename something that better describes the personality of the
  # file, instead of a stone cold hash.
  return (
      ''.join(sanitized_filename[:FILENAME_DESCRIPTOR_LENGTH]) + '_' +
      params_file_hash)


def GetBasenameFromBuildFilepath(build_filepath):
  return  


def GetCommandLogDirectory(out_dir):
  return os.path.join(out_dir, 'logs')


def _EnsureUniqueFileWithContentsExists(filepath, contents):
  '''Assuming |filepath| can only contain |contents|, atomically creates it.
  '''

  # First check if the file already exists, and if it does, assume that it
  # contains the correct contents.
  if not os.path.exists(filepath):
    # If it does not exist, create it, carefully.  We do our writing work to a
    # temporary file and then rename that (which is an atomic operation) to the
    # target filename.
    work_file = tempfile.NamedTemporaryFile(
        mode='w', delete=False, dir=os.path.dirname(filepath))
    work_file.write(contents)
    work_filepath = work_file.name
    work_file.close()

    try:
      os.rename(work_filepath, filepath)
    except OSError as e:
      # Check if this was an error caused by the file already existing, and if
      # so that's no problem, just return.
      pass

    # Try to remove the file now, which may not exist at this point (i.e. if it
    # was just renamed).
    try:
      os.remove(work_filepath)
    except OSError as e:
      pass

  if not os.path.exists(filepath):
    raise Exception('Could not open/write to file: %s' % filepath)


def RespireIdentifier(build_filepath, build_function_name):
  return build_filepath + ':' + build_function_name


def EnsureGenRegistryInputFilesExist(
    out_dir, build_filepath, build_function_name, params, additional_deps=[]):
  (params_content, futures) = json_utils.EncodeToJSON(params)
  sub_respire_filepaths = GetSubRespireFilepaths(
      out_dir,
      GetHashedBaseFilename(
          build_filepath, build_function_name, params_content))

  _EnsureUniqueFileWithContentsExists(
      sub_respire_filepaths.params_filepath, params_content)

  if not os.path.exists(sub_respire_filepaths.gen_registry_filepath):
    gen_registry_contents = _MakeGenRegistryContents(
        build_filepath, build_function_name, out_dir, sub_respire_filepaths,
        futures, additional_deps)

    _EnsureUniqueFileWithContentsExists(
        sub_respire_filepaths.gen_registry_filepath, gen_registry_contents)

  return sub_respire_filepaths
