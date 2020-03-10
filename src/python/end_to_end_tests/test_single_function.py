import os
import unittest
import test_utils

class TestSingleFunction(unittest.TestCase):
  def RunRespire(self, script_filepath, target_filepath):
    return test_utils.RunRespire(
      script_filepath=script_filepath,
      function='TestBuild',
      params={'out_dir' : self.temp_dirs.out_dir},
      out_dir=self.temp_dirs.respire_out_dir,
      targets=[target_filepath])

  def setUp(self):
    self.temp_dirs = test_utils.TemporaryTestDirectories('test_single_function')
    self.temp_dirs.__enter__()

  def tearDown(self):
    self.temp_dirs.teardown()

  def test_StandardSingleFunction(self):
    output_filepath = os.path.join(self.temp_dirs.out_dir, 'foofoobarbar.txt')
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'single_function.respire.py')
    foo_path = os.path.join(self.temp_dirs.source_dir, 'foo.txt')

    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)

    self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                     'foofoobarbar')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'single_function.count')))

    # Check that a modification to one of the source files is properly
    # propagated to the output files, and that the python build file does not
    # get re-evaluated.
    with open(foo_path, 'w') as f:
      f.write('fooey')
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'single_function.count')))
    self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                     'fooeyfooeybarbar')
    # Make sure that the 'barbar' file was not re-built, since it was not
    # modified.
    self.assertEqual(
        1, test_utils.GetCount(os.path.join(self.temp_dirs.out_dir, 'bar_bar.count')))

    # Make sure that directly running it again does *not* result in the
    # python respire script being executed again.
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)

    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'single_function.count')))

    # But if the respire script file is touched, then it should be executed
    # again.
    test_utils.Touch(script_filepath)
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'single_function.count')))

  def test_StdOutErrInFunction(self):
      output_filepath = os.path.join(self.temp_dirs.out_dir, 'final.txt')
      script_filepath = os.path.join(self.temp_dirs.source_dir,
                                     'stdouterrin_function.respire.py')

      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)

      self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                       'outerrin')
      self.assertEqual(1, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'stdouterrin_function.count')))

      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)
      self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                       'outerrin')
      self.assertEqual(1, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'stdouterrin_function.count')))

  def test_PythonFunction(self):
    output_filepath = os.path.join(self.temp_dirs.out_dir, 'foofoobarbar.txt')
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'python_function.respire.py')
    foo_path = os.path.join(self.temp_dirs.source_dir, 'foo.txt')

    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)

    self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                     'foofoobarbar')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'python_function.count')))

    # Check that a modification to one of the source files is properly
    # propagated to the output files, and that the python build file does not
    # get re-evaluated.
    with open(foo_path, 'w') as f:
      f.write('fooey')
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'python_function.count')))
    self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                     'fooeyfooeybarbar')

    # Make sure that directly running it again does *not* result in the
    # python respire script being executed again.
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)

    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'python_function.count')))

    # But if the respire script file is touched, then it should be executed
    # again.
    test_utils.Touch(script_filepath)
    result = self.RunRespire(script_filepath, output_filepath)
    self.assertTrue(result)
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'python_function.count')))

  def test_DirectoryAsInputFunction(self):
      output_filepath = os.path.join(self.temp_dirs.out_dir, 'final.txt')
      script_filepath = os.path.join(self.temp_dirs.source_dir,
                                     'directory_as_input_function.respire.py')

      test_directory = os.path.join(self.temp_dirs.source_dir, 'test_dir')
      os.mkdir(test_directory)
      test_file1 = os.path.join(test_directory, 'test_file1')
      test_file2 = os.path.join(test_directory, 'test_file2')

      with open(test_file1, 'w') as f:
        f.write('foo')
      with open(test_file2, 'w') as f:
        f.write('bar')

      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)
      self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                       'foobar')
      self.assertEqual(1, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'cat_directory.count')))

      # Doing nothing should not result in a rebuild.
      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)
      self.assertEqual(1, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'cat_directory.count')))

      # Removing a file from the directory should result in a rebuild.
      os.remove(test_file2)
      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)
      self.assertEqual(test_utils.ContentsForFilePath(output_filepath), 'foo')
      self.assertEqual(2, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'cat_directory.count')))

      # Adding a file should result in a rebuild.
      with open(test_file2, 'w') as f:
        f.write('blue')
      result = self.RunRespire(script_filepath, output_filepath)
      self.assertTrue(result)
      self.assertEqual(test_utils.ContentsForFilePath(output_filepath),
                       'fooblue')
      self.assertEqual(3, test_utils.GetCount(os.path.join(
          self.temp_dirs.out_dir, 'cat_directory.count')))

  def test_BuildDynamicDependency(self):
    # Make sure that build scripts that register additional dependencies rebuild
    # when those dependencies change.
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'build_dynamic_dependency.respire.py')
    input_file = os.path.join(self.temp_dirs.out_dir, 'input_file.txt')
    output_file = os.path.join(self.temp_dirs.out_dir, 'output_file.txt')

    with open(input_file, 'w') as f:
        f.write('hello')
    result = self.RunRespire(script_filepath, output_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(output_file), 'hello')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'build_dynamic_dependency.count')))

    result = self.RunRespire(script_filepath, output_file)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'build_dynamic_dependency.count')))

    with open(input_file, 'w') as f:
        f.write('there')
    result = self.RunRespire(script_filepath, output_file)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(output_file), 'there')
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'build_dynamic_dependency.count')))
