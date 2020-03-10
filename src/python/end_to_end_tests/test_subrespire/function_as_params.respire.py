import os
import registry
import end_to_end_tests.test_utils as utils


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))


def Foo(num_times):
  return '-'.join(['foo'] * num_times)


def Bar(f):
  return f(3)


def TestBuild(registry, out_dir):
  registry.SubRespire(WriteOutFunctionOutput, out_dir=out_dir, f=Foo, b=Bar)


def WriteOutFunctionOutput(registry, out_dir, f, b):
  out_file = os.path.join(out_dir, 'sent_file.txt')
  f_2_output = f(2)

  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(
                  f_2_output + '_' + b(lambda x: 'bar-' * x), out_file))

  return out_file
