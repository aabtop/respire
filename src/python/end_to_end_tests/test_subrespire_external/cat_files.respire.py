import os
import registry
import end_to_end_tests.test_utils as utils


def CatFiles(registry, out_dir, inputs, output_file):
  utils.AddCount(os.path.join(out_dir, 'CatFiles.count'))
  registry.SystemCommand(
      inputs=inputs,
      outputs=[output_file],
      command=utils.CatCommand(inputs, output_file))
  return output_file

