import os
import registry
import end_to_end_tests.test_utils as utils


def TestBuild(registry, out_dir):
  input_file = os.path.join(out_dir, 'input_file.txt')
  output_file = os.path.join(out_dir, 'output_file.txt')

  registry.RegisterSelfDependency(input_file)

  with open(input_file, 'r') as f:
    input_contents = f.read()

  registry.PythonFunction(
      inputs=[], outputs=[output_file], function=WriteContent,
      content=input_contents)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'build_dynamic_dependency.count'))


def WriteContent(outputs, content):
  with open(outputs[0], 'w') as f:
    f.write(content)
