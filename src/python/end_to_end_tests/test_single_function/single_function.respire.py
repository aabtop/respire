import os
import registry
import end_to_end_tests.test_utils as utils

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

def TestBuild(registry, out_dir):
  foo_path = os.path.join(SCRIPT_DIR, 'foo.txt')
  bar_path = os.path.join(SCRIPT_DIR, 'bar.txt')
  foo_foo_path = os.path.join(out_dir, 'foofoo.txt')
  bar_bar_path = os.path.join(out_dir, 'barbar.txt')
  foo_bar_path = os.path.join(out_dir, 'foofoobarbar.txt')

  registry.SystemCommand(
      inputs=[foo_path],
      outputs=[foo_foo_path],
      command=utils.CatCommand([foo_path, foo_path], foo_foo_path))

  registry.SystemCommand(
      inputs=[bar_path],
      outputs=[bar_bar_path],
      command=utils.CatCommand([bar_path, bar_path], bar_bar_path,
                               os.path.join(out_dir, 'bar_bar.count')))

  registry.SystemCommand(
      inputs=[foo_foo_path, bar_bar_path],
      outputs=[foo_bar_path],
      command=utils.CatCommand([foo_foo_path, bar_bar_path], foo_bar_path))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'single_function.count'))
