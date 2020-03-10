import inspect
import imp
import json
import os
import shutil
import subprocess
import sys
import tempfile
import uuid


RESPIRE_SCRIPT_PATH = os.path.join(os.path.dirname(__file__),
                                   os.pardir, 'respire.py')


class TemporaryTestDirectories(object):
  def __init__(self, original_source_tree):
    '''Creates a temporary directory for running end-to-end tests.

    Creates a temporary directory, copies a read-only source directory tree
    (specified relative to this script's directory) into a subdirectory of the
    temporary directory, and also creates an output subdirectory within the
    temporary directory.
    '''
    self.temp_dir = tempfile.mkdtemp()
    #self.temp_dir = 'C:\\Users\\andrew\\temp'
    #if os.path.exists(self.temp_dir):
    #  shutil.rmtree(self.temp_dir)

    self.source_dir = os.path.join(self.temp_dir, 'source')
    shutil.copytree(
        os.path.join(os.path.dirname(__file__), original_source_tree),
        self.source_dir)

    self.out_dir = os.path.join(self.temp_dir, 'output')
    os.mkdir(self.out_dir)

    self.respire_out_dir = os.path.join(self.temp_dir, 'respire_output')
    os.mkdir(self.respire_out_dir)

  def teardown(self):
    shutil.rmtree(self.temp_dir)

  def __enter__(self):
    return self

  def __exit__(self, exception_type, exception_value, traceback):
    self.teardown()


def ContentsForFilePath(filepath):
  with open(filepath, 'r') as f:
    contents = f.read()
  return contents


def DriverStep(registry, forward_params, build_file, function, targets):
  registry.SubRespireExternal(build_file, function, **forward_params)
  for target in targets:
    registry.Build(target)


def RunRespire(script_filepath, function, params, out_dir, targets):
  driver_params = {
    'forward_params': params,
    'build_file': script_filepath,
    'function': function,
    'targets': targets,
  }

  # Ensure that the local Python tree of Respire is used, and not whichever
  # one happened to already be in the path.
  sys.path.insert(
      0, os.path.join(os.path.join(os.path.dirname(__file__), os.pardir)))

  respire_module = imp.load_source('respire_module', RESPIRE_SCRIPT_PATH)
  return respire_module.Run(
      out_dir, inspect.getsourcefile(sys.modules[__name__]), 'DriverStep',
      driver_params)


def GetCount(count_filepath):
  if os.path.isdir(count_filepath):
    count = len([x for x in os.listdir(count_filepath) if os.path.isfile(os.path.join(count_filepath, x))])
    return count
  else:
    return 0


def AddCount(count_filepath):
  try:
    os.makedirs(count_filepath)
  except OSError:
    # No problem if the directory already exists.
    pass

  open(os.path.join(count_filepath, str(uuid.uuid4().hex)), 'a').close()


def Touch(filepath):
  if os.path.exists(filepath):
    os.utime(filepath, None)
  else:
    open(filepath, 'a').close()


def CatCommand(inputs, output, count_file=None):
  '''Returns a cross-platform command string list to `cat` inputs to output.'''
  shell_cat_path = os.path.join(os.path.dirname(__file__), 'shell_cat.py')

  count_file_string = count_file if count_file else 'None'

  return [sys.executable, shell_cat_path] + inputs + [output, count_file_string]


def EchoCommand(input, output, count_file=None):
  '''Returns a cross-platform command string list to `echo` input to a file.'''
  shell_echo_path = os.path.join(os.path.dirname(__file__),
                                 'shell_echo.py')

  count_file_string = count_file if count_file else 'None'

  return [sys.executable, shell_echo_path, input, output, count_file_string]
