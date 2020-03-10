import os
import registry
import end_to_end_tests.test_utils as utils

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

def TestBuild(registry, out_dir):
  foo_path = os.path.join(SCRIPT_DIR, 'foo.txt')
  foo_foo_path = os.path.join(out_dir, 'foofoo.txt')

  registry.SubRespire(
      DoubleInput, input=foo_path, output=foo_foo_path, out_dir=out_dir)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'double_input.TestBuild.count'))


def DoubleInput(registry, out_dir, input, output):
  registry.SystemCommand(
      inputs=[input],
      outputs=[output],
      command=utils.CatCommand([input, input], output))
  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'DoubleInput.count'))
