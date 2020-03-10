import inspect
import os
import sys

def AssertIsValidRespireFunction(function):
  function_module = sys.modules[function.__module__]
  if not hasattr(function_module, function.__name__):
    raise Exception(
        'Only named top-level functions are valid in this context.')


def GetCallerScript():
  this_source = inspect.getsourcefile(sys.modules[__name__])
  stack = inspect.stack()
  stack_index = 0

  for i in range(0, 2):
    while stack_index < len(stack) and stack[stack_index][1] == this_source:
      stack_index += 1
    this_source = stack[stack_index][1]

  if stack_index < len(stack):
    return stack[stack_index][1]
  else:
    return None


def ResolveFilepathRelativeToCallerScript(filepath):
  '''Returns an absolute filepath relative to the calling script's directory.'''
  if os.path.isabs(filepath):
    return filepath
  caller_script = GetCallerScript()
  if caller_script:
    return os.path.join(os.path.abspath(os.path.dirname(caller_script)),
                        filepath)
  else:
    return os.path.abspath(filepath)
