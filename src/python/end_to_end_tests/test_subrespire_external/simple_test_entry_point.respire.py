import os
import registry
import end_to_end_tests.test_utils as utils


def TestBuild(registry, out_dir):
  foobar_filepath = os.path.join(out_dir, 'foobar.txt')

  foo = registry.SubRespireExternal('generate_foo.respire.py', 'GenerateFoo',
                                    out_dir=out_dir)
  bar = registry.SubRespireExternal('generate_bar.respire.py', 'GenerateBar',
                                    out_dir=out_dir)

  registry.SubRespireExternal(
      'cat_files.respire.py', 'CatFiles',
      inputs=[foo, bar], output_file=foobar_filepath, out_dir=out_dir)

  # Track how many times this python is executed.
  utils.AddCount(
      os.path.join(out_dir, 'simple_test_entry_point.TestBuild.count'))
