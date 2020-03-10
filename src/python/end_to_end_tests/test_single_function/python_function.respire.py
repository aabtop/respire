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

  registry.PythonFunction(
      inputs=[bar_path],
      outputs=[bar_bar_path],
      function=_DoubleFile,
      my_input=bar_path,
      my_output=bar_bar_path)

  registry.PythonFunction(
      inputs=[foo_foo_path, bar_bar_path],
      outputs=[foo_bar_path],
      function=_ConcatenateFiles)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'python_function.count'))


def _DoubleFile(my_input, my_output):
  with open(my_input, 'r') as i:
    my_content = i.read()

  with open(my_output, 'w') as o:
    o.write(my_content)
    o.write(my_content)


def _ConcatenateFiles(inputs, outputs):
  with open(outputs[0], 'w') as o:
    for input_file in inputs:
      with open(input_file, 'r') as i:
        o.write(i.read())
