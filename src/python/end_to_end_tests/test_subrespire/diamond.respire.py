import os
import registry
import end_to_end_tests.test_utils as utils


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))


def TestBuild(registry, out_dir):
  foo_path = os.path.join(SCRIPT_DIR, 'foo.txt')
  bar_path = os.path.join(SCRIPT_DIR, 'bar.txt')
  bottom_file = os.path.join(out_dir, 'bottom.txt')
  middle1_file = os.path.join(out_dir, 'middle1.txt')
  middle2_file = os.path.join(out_dir, 'middle2.txt')
  middle3_file = os.path.join(out_dir, 'middle3.txt')
  top_file = os.path.join(out_dir, 'top.txt')

  bottom = registry.SubRespire(
      GenerateBottom, out_dir=out_dir, out_file=bottom_file)
  middle1 = registry.SubRespire(CatBottomWith,
      out_dir=out_dir, bottom_file=bottom_file, cat_file=foo_path,
      out_file=middle1_file)
  middle2 = registry.SubRespire(CatBottomWith,
      out_dir=out_dir, bottom_file=bottom_file, cat_file=bar_path,
      out_file=middle2_file)
  middle3 = registry.SubRespire(CatFiles,
      out_dir=out_dir, inputs=[bottom, bottom], out_file=middle3_file)
  top = registry.SubRespire(CatFiles,
      out_dir=out_dir, inputs=[middle1, middle2, middle3],
      out_file=top_file)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'diamond.TestBuild.count'))


def GenerateBottom(registry, out_dir, out_file):
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand('TheBottom', out_file))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'GenerateBottom.count'))
  return out_file


def CatBottomWith(registry, out_dir, bottom_file, cat_file, out_file):
  utils.AddCount(os.path.join(out_dir, 'CatBottomWith.count'))

  bottom = registry.SubRespire(
      GenerateBottom,
      out_dir=out_dir, out_file=bottom_file)

  return registry.SubRespire(
      CatFiles,
      out_dir=out_dir, inputs=[bottom, cat_file], out_file=out_file)


def CatFiles(registry, out_dir, inputs, out_file):
  utils.AddCount(os.path.join(out_dir, 'CatFiles.count'))
  registry.SystemCommand(
      inputs=inputs,
      outputs=[out_file],
      command=utils.CatCommand(inputs, out_file))
  return out_file

