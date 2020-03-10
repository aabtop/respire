import os
import unittest
import test_utils

class TestSubRespireExternal(unittest.TestCase):
  def RunRespire(self, script_filepath, target_filepath):
    return test_utils.RunRespire(
      script_filepath=script_filepath,
      function='TestBuild',
      params={'out_dir' : self.temp_dirs.out_dir},
      out_dir=self.temp_dirs.respire_out_dir,
      targets=[target_filepath])

  def setUp(self):
    self.temp_dirs = test_utils.TemporaryTestDirectories('test_subrespire_external')
    self.temp_dirs.__enter__()

  def tearDown(self):
    self.temp_dirs.teardown()

  def test_SimpleTest(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'simple_test_entry_point.respire.py')
    generate_foo_script_filepath = (
        os.path.join(self.temp_dirs.source_dir, 'generate_foo.respire.py'))
    foobar_filepath = os.path.join(self.temp_dirs.out_dir, 'foobar.txt')

    result = self.RunRespire(script_filepath, foobar_filepath)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(foobar_filepath),
                     'foobar')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'simple_test_entry_point.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBar.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateFoo.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

    test_utils.Touch(generate_foo_script_filepath)
    result = self.RunRespire(script_filepath, foobar_filepath)
    self.assertTrue(result)
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'simple_test_entry_point.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBar.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateFoo.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

  def test_ExtraPythonDepsTest(self):
    # Test to ensure that respire can correctly compute the python dependencies
    # of a module and properly re-build if those dependencies change.
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'simple_test_entry_point.respire.py')
    get_foo_value_filepath = (
        os.path.join(self.temp_dirs.source_dir, 'get_foo_value.py'))
    foobar_filepath = os.path.join(self.temp_dirs.out_dir, 'foobar.txt')

    result = self.RunRespire(script_filepath, foobar_filepath)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(foobar_filepath),
                     'foobar')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'simple_test_entry_point.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBar.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateFoo.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

    # Modify an import of the generate_foo.respire.py module, and then make
    # sure that it results in a rebuild.
    with open(get_foo_value_filepath, 'w') as f:
      f.write('foo_value = "foov2"')

    result = self.RunRespire(script_filepath, foobar_filepath)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(foobar_filepath),
                     'foov2bar')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'simple_test_entry_point.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateBar.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'GenerateFoo.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'CatFiles.count')))

  def test_CanPassObjectAsParameter(self):
    script_filepath = os.path.join(self.temp_dirs.source_dir,
                                   'can_pass_object_as_parameter.respire.py')
    foo_class_script_filepath = (
        os.path.join(self.temp_dirs.source_dir, 'foo_class.py'))
    
    value1_filepath = os.path.join(
        self.temp_dirs.out_dir, 'value_with_import.txt')
    value2_filepath = os.path.join(
        self.temp_dirs.out_dir, 'value_without_import.txt')

    def GenerateFoo(get_value):
      template = (
          'class Foo(object):\n'
          '  def __init__(self, p, q):\n'
          '    self.a = p\n'
          '    self.b = q\n'
          '\n'
          '  def GetValue(self):\n'
          '    return self.%s\n'
          '\n')

      with open(foo_class_script_filepath, 'w') as f:
        f.write(template % (get_value))

    GenerateFoo('a')

    result = self.RunRespire(script_filepath, value1_filepath)
    self.assertTrue(result)
    result = self.RunRespire(script_filepath, value2_filepath)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(value1_filepath), 'woof')
    self.assertEqual(test_utils.ContentsForFilePath(value2_filepath), 'woof')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir,
        'can_pass_object_as_parameter.TestBuild.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'get_object_value.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'write_object_value_with_import.count')))
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'write_object_value_without_import.count')))

    GenerateFoo('b')

    result = self.RunRespire(script_filepath, value1_filepath)
    self.assertTrue(result)
    result = self.RunRespire(script_filepath, value2_filepath)
    self.assertTrue(result)
    self.assertEqual(test_utils.ContentsForFilePath(value1_filepath), 'bark')
    self.assertEqual(test_utils.ContentsForFilePath(value2_filepath), 'bark')
    self.assertEqual(1, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir,
        'can_pass_object_as_parameter.TestBuild.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'get_object_value.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'write_object_value_with_import.count')))
    self.assertEqual(2, test_utils.GetCount(os.path.join(
        self.temp_dirs.out_dir, 'write_object_value_without_import.count')))
