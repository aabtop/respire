import os
import registry
import end_to_end_tests.test_utils as utils

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

def TestBuild(registry, out_dir):
  test_directory_path = os.path.join(SCRIPT_DIR, 'test_dir')
  final_path = os.path.join(out_dir, 'final.txt')

  registry.PythonFunction(
      inputs=[test_directory_path],
      outputs=[final_path],
      function=CatDirectory,
      out_dir=out_dir)


def CatDirectory(inputs, outputs, out_dir):
  with open(outputs[0], 'w') as o:
    for entry in os.listdir(inputs[0]):
      with open(os.path.join(inputs[0], entry), 'r') as i:
        o.write(i.read())

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'cat_directory.count'))
