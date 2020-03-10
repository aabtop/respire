from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import imp
import inspect
import json
import os
import sys
import utils

from buildlib.respire.buildlib.utils import AssertIsValidRespireFunction
from respire_future import RespireFuture
import registry_helpers

def GenerateCyclicDependencyErrorMessage(error_frame, future_call_stack):
  message = 'Cyclic dependency detected: \n'

  found_error_path = False
  for frame in future_call_stack:
    if frame == error_frame:
      found_error_frame = True
    if found_error_frame:
      message += frame[0] + '->\n'
  message += error_frame + '\n'
  return message


def DecodeFromJSONWithFlattening(encoded, future_call_stack=[],
                                 expand_objects=True):
  def AsFuture(dct):
    if '__FUTURE__' in dct:
      # First check to make sure there are no cyclic dependencies.
      # As an aside though, respire is actually *preeeeetty close* to just being
      # able to actually support cyclic dependencies.  All the other pieces
      # are in place except for how to express the cyclic dependency in the
      # Python decoded JSON.  One idea would be to make Futures() resolve to
      # pointers to values instead of the value itself.
      call_stack_frame = (
          registry_helpers.RespireIdentifier(dct['source_build_filepath'],
                                             dct['source_build_function']),
          dct['value_filepath'])

      if call_stack_frame in future_call_stack:
        raise Exception(GenerateCyclicDependencyErrorMessage(
            call_stack_frame, future_call_stack))

      # Open the future (which should now be resolved) and replace it with its
      # known value.
      with open(dct['value_filepath'], 'r') as f:
        future_contents = f.read()
      return DecodeFromJSONWithFlattening(
          future_contents,
          future_call_stack=future_call_stack + [call_stack_frame],
          expand_objects=expand_objects)
    elif expand_objects and '__is_object' in dct:
      return _DictToObject(dct)
    elif expand_objects and '__is_function' in dct:
      return _DictToFunction(dct)
    return dct
  return json.loads(encoded, object_hook=AsFuture)


def DecodeFromJSON(encoded):
  def CheckNoFutures(dct):
    if '__FUTURE__' in dct:
      raise Exception('Expected to find flattened (no futures) JSON content.')
    elif '__is_object' in dct:
      return _DictToObject(dct)
    elif '__is_function' in dct:
      return _DictToFunction(dct)
    return dct
  return json.loads(encoded, object_hook=CheckNoFutures)


# Returns a tuple where the first item is the encoded JSON string, and the
# second item is a list of all the futures found within the input it.
def EncodeToJSON(decoded):
  # Python's overridable JSONEncoder is not flexible enough to let us hook
  # in for anything other than objects, so we convert our input manually
  # to a form that is trivially JSON encodable.
  (encodable, futures) = _ConvertInputToJSONEncodableForm(decoded)

  return (json.dumps(encodable, indent=2, separators=(',', ': ')),
          futures)


def _ConvertInputToJSONEncodableForm(decoded):
  futures = []

  def Encode(o):
    if isinstance(o, dict) or isinstance(o, collections.OrderedDict):
      return {Encode(key): Encode(value) for key, value in o.items()}
    elif _IsObject(o):
      if isinstance(o, RespireFuture):
        futures.append(o)
        return {'__FUTURE__' : '',
                'value_filepath' : o.ValueFilepath(),
                'include_filepath' : o.IncludeFilepath(),
                'source_build_filepath': o.SourceBuildFilepath(),
                'source_build_function': o.SourceBuildFunction()}
      else:
        return Encode(_ObjectToDict(o))
    elif utils.IsString(o):
      return o
    elif hasattr(o, '__iter__'):
      return [Encode(x) for x in o]
    else:
      return o

  result = Encode(decoded)
  return (result, futures)


def _IsObject(o):
  return hasattr(o, '__dict__') or hasattr(o, '_asdict')


def _ObjectToDict(o):
  if callable(o):
    AssertIsValidRespireFunction(o)
    return {
      '__is_function': True,
      'function_name': o.__name__,
      'function_module': inspect.getsourcefile(sys.modules[o.__module__])
    }

  class_module = inspect.getmodule(o.__class__)
  if not hasattr(class_module, '__file__'):
    raise Exception('Only objects of classes from non-builtin modules can be '
                    'sent across subrespire boundaries.')
  if not hasattr(class_module, o.__class__.__name__):
    raise Exception('Only objects of classes defined in the top-level can be '
                    'sent across subrespire boundaries.')

  if hasattr(o, '__dict__'):
    members = vars(o)
  elif hasattr(o, '_asdict'):
    members = o._asdict()

  a = {
    '__is_object': True,
    'module': os.path.normcase(os.path.abspath((
                  inspect.getsourcefile(class_module)))),
    'class': o.__class__.__name__,
    'vars': members,
  }

  return a


def _DictToObject(d):
  assert('module' in d)
  assert('class' in d)
  assert('vars' in d)

  module_path = d['module']
  class_name = d['class']

  # Search through the list of currently opened module and see if we can find
  # in it the one that this class came from.
  module = None
  for cur_module in sys.modules.values():
    if not cur_module:
      # Apparently this case is possible.
      continue
    try:
      source_file = inspect.getsourcefile(cur_module)
    except TypeError:
      # TypeError may be raised if the current module is built-in, in which
      # case we should just ignore it.
      continue

    if (hasattr(cur_module, '__file__') and source_file and
        module_path == os.path.normcase(os.path.abspath(source_file))):
      module = cur_module
      break

  # If we couldn't find the class's source module open already, try to open
  # it here and now so that we can instantiate it still.
  if not module:
    basename = os.path.basename(module_path)
    module = imp.load_source(
                 '__RESPIRE__' + basename[:basename.find('.')], module_path)

  if not module:
    raise Exception('Could not load module with path ' + module_path + ' while '
                    'attempting to load class ' + class_name)

  # Now that we have the module and the class name, get a reference to the
  # actual class and instantiate it.
  obj_class = getattr(module, class_name)
  if hasattr(obj_class, '_asdict'):
    # namedtuples need to be initialized right when they're constructed.
    obj = obj_class.__new__(obj_class, **d['vars'])
  else:
    obj = obj_class.__new__(obj_class)
    for k, v in d['vars'].items():
      setattr(obj, k, v)

  return obj

def _DictToFunction(d):
  assert('function_name' in d)
  assert('function_module' in d)

  function_name = d['function_name']
  function_module_filepath = d['function_module']
  function_module_basename = os.path.basename(function_module_filepath)
  function_module = imp.load_source(
      '__RESPIRE__' +
          function_module_basename[:function_module_basename.find('.')],
      function_module_filepath)

  if not hasattr(function_module, function_name):
    raise Exception(
        'Serialized function, "%s", not found in module file: %s.'
        % (function_name, function_module_filepath))
  return getattr(function_module, function_name)
