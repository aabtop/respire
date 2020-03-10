import os
import registry
import end_to_end_tests.test_utils as utils

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))


class Foo(object):
  def __init__(self, p):
    self.a = p
    self.b = 2
    self.c = [1, 2, 3]
    self.d = {'hello': 'there'}

  def GetA(self):
    return self.a


def TestBuild(registry, out_dir):
  a = Foo(1)
  b = Foo(a)

  c = registry.SubRespire(SendObject, out_dir=out_dir, a=a, b=b)

  registry.SubRespire(ResolveReturnedObject, out_dir=out_dir, c=c)


def SendObject(registry, out_dir, a, b):
  out_file = os.path.join(out_dir, 'sent_file.txt')
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(str(a.GetA()) + '_' +
                                str(b.GetA().b), out_file))

  b.d['hello'] += '_ghoul'

  return b


def ResolveReturnedObject(registry, out_dir, c):
  out_file = os.path.join(out_dir, 'returned_file.txt')
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(str(c.GetA().GetA()) + '_' +
                                str(c.d['hello']), out_file))
