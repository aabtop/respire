import os
import registry
import end_to_end_tests.test_utils as utils
import get_foo_value


def GenerateFoo(registry, out_dir):
  foo_filepath = os.path.join(out_dir, 'foo.txt')

  registry.SystemCommand(
      inputs=[],
      outputs=[foo_filepath],
      command=utils.EchoCommand(get_foo_value.foo_value, foo_filepath))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'GenerateFoo.count'))
  return foo_filepath
