'''Helper functions for Python scripts that wrap Python function calls.'''


import inspect
import os
import sys


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
