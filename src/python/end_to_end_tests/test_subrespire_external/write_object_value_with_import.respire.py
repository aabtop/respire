import os
import registry
import end_to_end_tests.test_utils as utils


import foo_class


def WriteObjectValueWithImport(registry, out_dir, a):
  out_file = os.path.join(out_dir, 'value_with_import.txt')

  utils.AddCount(
      os.path.join(out_dir, 'write_object_value_with_import.count'))

  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(a.GetValue(), out_file))
