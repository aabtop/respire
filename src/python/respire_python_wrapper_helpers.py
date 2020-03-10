'''Helper functions for Python scripts that wrap Python function calls.'''

import sys
if sys.version_info[0] == 3:
  import importlib.util
else:
  import imp

import inspect
import os


PYTHON_EXECUTABLE_DIR = os.path.realpath(os.path.dirname(sys.executable))


def GetCurrentNonBuiltInImportDependencySources():
  def ModuleIsBuiltIn(m):
    source_file = None
    if m and hasattr(m, '__file__'):
      source_file = inspect.getsourcefile(m)
    if not source_file:
      return True
    if os.path.realpath(source_file).startswith(PYTHON_EXECUTABLE_DIR):
      return True
    return False

  return [inspect.getsourcefile(m) for m in sys.modules.values() \
          if not ModuleIsBuiltIn(m)]


def WriteToFileIfContentsDiffer(filepath, content):
  def FileExistsAndHasContents(filepath, content):
    file_contents = None
    try:
      with open(filepath, 'r') as f:
        file_contents = f.read()
    except:
      return False

    return file_contents == content

  if not FileExistsAndHasContents(filepath, content):
    with open(filepath, 'w') as f:
      f.write(content)


def LoadSourceModule(name, filepath):
  '''Python 2/3 agnostic arbitrary Python source code loader/executor.'''
  if sys.version_info[0] == 3:
    spec = importlib.util.spec_from_file_location(
        name, filepath)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = mod
    spec.loader.exec_module(mod)
    return mod
  else:
    return imp.load_source(name, filepath)
