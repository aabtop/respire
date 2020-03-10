import os
import registry
import end_to_end_tests.test_utils as utils

def TestBuild(registry, out_dir):
  stdin_path = os.path.join(out_dir, 'stdin.txt')
  stdout_path = os.path.join(out_dir, 'stdout.txt')
  stderr_path = os.path.join(out_dir, 'stderr.txt')
  final_path = os.path.join(out_dir, 'final.txt')

  registry.SystemCommand(
      inputs=[],
      outputs=[stdin_path],
      command=utils.EchoCommand('in', stdin_path))

  registry.SystemCommand(
      inputs=[],
      outputs=[],
      command=utils.EchoCommand('out', 'stdout'),
      stdout=stdout_path)

  registry.SystemCommand(
      inputs=[],
      outputs=[],
      command=utils.EchoCommand('err', 'stderr'),
      stderr=stderr_path)

  registry.SystemCommand(
      inputs=[stdout_path, stderr_path],
      outputs=[final_path],
      command=utils.CatCommand(
                  [stdout_path, stderr_path, 'stdin'], final_path,
                  os.path.join(out_dir, 'final.count')),
      stdin=stdin_path)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'stdouterrin_function.count'))
