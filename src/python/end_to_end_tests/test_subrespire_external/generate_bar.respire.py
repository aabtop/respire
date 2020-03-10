import os
import registry
import end_to_end_tests.test_utils as utils


def GenerateBar(registry, out_dir):
  bar_filepath = os.path.join(out_dir, 'bar.txt')

  registry.SystemCommand(
      inputs=[],
      outputs=[bar_filepath],
      command=utils.EchoCommand('bar', bar_filepath))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'GenerateBar.count'))
  return bar_filepath
