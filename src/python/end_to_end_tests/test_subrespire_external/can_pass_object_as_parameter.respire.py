import os
import registry
import end_to_end_tests.test_utils as utils

def TestBuild(registry, out_dir):
  a = registry.SubRespireExternal(
      'get_object_value.respire.py', 'GetObjectValue',
      out_dir=out_dir)

  registry.SubRespireExternal(
      'write_object_value_with_import.respire.py', 'WriteObjectValueWithImport',
      out_dir=out_dir, a=a)

  registry.SubRespireExternal(
      'write_object_value_without_import.respire.py',
      'WriteObjectValueWithoutImport',
      out_dir=out_dir, a=a)

  utils.AddCount(
      os.path.join(out_dir, 'can_pass_object_as_parameter.TestBuild.count'))
