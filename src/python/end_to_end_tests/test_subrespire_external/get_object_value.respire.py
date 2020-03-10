import os
import registry
import end_to_end_tests.test_utils as utils


import foo_class


def GetObjectValue(registry, out_dir):
  utils.AddCount(os.path.join(out_dir, 'get_object_value.count'))

  return foo_class.Foo('woof', 'bark')
