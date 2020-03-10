import os
import registry
import end_to_end_tests.test_utils as utils


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))


def TestBuild(registry, out_dir):
  bottom_file = os.path.join(out_dir, 'bottom.txt')
  out_file = os.path.join(out_dir, 'out.txt')

  bottom = registry.SubRespire(
      GenerateBottom, out_dir=out_dir, out_file=bottom_file)

  # Ultimately these should all result in the same output command.
  out_file_1 = registry.SubRespire(CatFiles,
      out_dir=out_dir, inputs=[bottom, bottom], output_file=out_file)
  out_file_2 = registry.SubRespire(CatFiles,
      out_dir=out_dir, inputs=[bottom_file, bottom], output_file=out_file)
  out_file_3 = registry.SubRespire(CatFiles,
      out_dir=out_dir, inputs=[bottom, bottom_file], output_file=out_file)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'futures_resolve.TestBuild.count'))


def GenerateBottom(registry, out_dir, out_file):
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand('TheBottom', out_file))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'GenerateBottom.count'))
  return out_file


def CatFiles(registry, out_dir, inputs, output_file):
  utils.AddCount(os.path.join(out_dir, 'CatFiles.count'))
  registry.SystemCommand(
      inputs=inputs,
      outputs=[output_file],
      command=utils.CatCommand(inputs, output_file))
  return output_file

