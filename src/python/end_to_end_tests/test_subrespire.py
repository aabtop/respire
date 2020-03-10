import os
import unittest
import test_utils

class TestSubRespire(unittest.TestCase):
  def RunRespire(self, script_filepath, target_filepath):
    return test_utils.RunRespire(
      script_filepath=script_filepath,
      function='TestBuild',
      params={'out_dir' : self.temp_dirs.out_dir},
      out_dir=self.temp_dirs.respire_out_dir,
      targets=[target_filepath])

  def setUp(self):
    self.temp_dirs = test_utils.TemporaryTestDirectories('test_subrespire')
    self.temp_dirs.__enter__()

  def tearDown(self):
    self.temp_dirs.teardown()

  def test_DoubleInput(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'double_input.respire.py')
    foo_path = os.path.join(self.temp_dirs.source_dir, 'foo.txt')
    foofoo_filepath = os.path.join(self.temp_dirs.out_dir, 'foofoo.txt')

    result = self.RunRespire(script_filepath, foofoo_filepath)
    self.assertTrue(result)

    self.assertEqual(test_utils.ContentsForFilePath(foofoo_filepath),
                     'foofoo')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'double_input.TestBuild.count')))

  def test_Chain(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'chain.respire.py')
    chain_results_1_path = os.path.join(
        self.temp_dirs.out_dir, 'chain_results_1.txt')
    chain_results_2_path = os.path.join(
        self.temp_dirs.out_dir, 'chain_results_2.txt')
    chain_results_1_1_path = os.path.join(
        self.temp_dirs.out_dir, 'chain_output_11.txt')

    result = self.RunRespire(script_filepath, chain_results_1_path)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(chain_results_1_path), '1')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'chain.TestBuild.count')))
    self.assertEqual(7, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'Chain.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'WriteResults.count')))

    result = self.RunRespire(script_filepath, chain_results_2_path)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'chain.TestBuild.count')))
    self.assertEqual(7, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'Chain.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'WriteResults.count')))
    self.assertEqual(test_utils.ContentsForFilePath(chain_results_2_path), '2')

    result = self.RunRespire(script_filepath, chain_results_2_path)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'chain.TestBuild.count')))
    self.assertEqual(7, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'Chain.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'WriteResults.count')))

  def test_AddNumbers(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'add_numbers.respire.py')
    two_file = os.path.join(self.temp_dirs.out_dir, 'two_file.txt')
    three_file = os.path.join(self.temp_dirs.out_dir, 'three_file.txt')
    three_again_file = os.path.join(
        self.temp_dirs.out_dir, 'three_again_file.txt')
    four_file = os.path.join(self.temp_dirs.out_dir, 'four_file.txt')

    result = self.RunRespire(script_filepath, four_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(four_file), '4')

    result = self.RunRespire(script_filepath, two_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(two_file), '2')

    result = self.RunRespire(script_filepath, three_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(three_file), '3')

    result = self.RunRespire(script_filepath, three_again_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(three_again_file), '3')

    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'add_numbers.TestBuild.count')))
    self.assertEqual(4, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'WriteAsString.count')))
    self.assertEqual(4, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'AddNumbers.count')))

  def test_Diamond(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'diamond.respire.py')
    top_file = os.path.join(self.temp_dirs.out_dir, 'top.txt')

    result = self.RunRespire(script_filepath, top_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(top_file),
                     'TheBottomfooTheBottombarTheBottomTheBottom')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'diamond.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBottom.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatBottomWith.count')))
    self.assertEqual(4, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

  def test_FuturesResolve(self):
    # This test makes sure that when using futures as part of the parameters
    # for a subrespire build, the futures resolve when determining which
    # subrespire to run, so that multiples calls with different futures that
    # resolve into the same parameters will all call into the same subrespire.
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'futures_resolve.respire.py')
    out_file = os.path.join(self.temp_dirs.out_dir, 'out.txt')

    result = self.RunRespire(script_filepath, out_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(out_file), 'TheBottomTheBottom')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'futures_resolve.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBottom.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

  def test_ObjectAsParams(self):
    # Make sure that we can pass objects as parameters properly.
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'object_as_params.respire.py')
    sent_file = os.path.join(self.temp_dirs.out_dir, 'sent_file.txt')
    returned_file = os.path.join(self.temp_dirs.out_dir, 'returned_file.txt')

    result = self.RunRespire(script_filepath, sent_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(sent_file), '1_2')

    result = self.RunRespire(script_filepath, returned_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(returned_file),
                     '1_there_ghoul')

  def test_FunctionAsParams(self):
        # Make sure that we can pass objects as parameters properly.
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'function_as_params.respire.py')
    sent_file = os.path.join(self.temp_dirs.out_dir, 'sent_file.txt')

    result = self.RunRespire(script_filepath, sent_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(sent_file),
                     'foo-foo_bar-bar-bar-')
