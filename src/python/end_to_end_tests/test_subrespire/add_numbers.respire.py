import os
import registry
import end_to_end_tests.test_utils as utils


def TestBuild(registry, out_dir):
  two_file = os.path.join(out_dir, 'two_file.txt')
  three_file = os.path.join(out_dir, 'three_file.txt')
  three_again_file = os.path.join(out_dir, 'three_again_file.txt')
  four_file = os.path.join(out_dir, 'four_file.txt')

  two = registry.SubRespire(AddNumbers, out_dir=out_dir, numbers=[1, 1])
  three = registry.SubRespire(AddNumbers, out_dir=out_dir, numbers=[1, two])
  three_again = registry.SubRespire(AddNumbers, out_dir=out_dir, numbers=[1, 2])
  four = registry.SubRespire(AddNumbers, out_dir=out_dir, numbers=[1, 3])

  registry.SubRespire(
      WriteAsString,
      out_dir=out_dir, out_file=two_file, contents=two)
  registry.SubRespire(
      WriteAsString,
      out_dir=out_dir, out_file=three_file, contents=three)
  registry.SubRespire(
      WriteAsString,
      out_dir=out_dir, out_file=three_again_file, contents=three_again)
  registry.SubRespire(
      WriteAsString,
      out_dir=out_dir, out_file=four_file, contents=four)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'add_numbers.TestBuild.count'))


def AddNumbers(registry, out_dir, numbers):
  utils.AddCount(os.path.join(out_dir, 'AddNumbers.count'))
  if numbers[1] == 0:
    return numbers[0]
  else:
    return registry.SubRespire(
        AddOne, number=registry.SubRespire(
            AddNumbers, out_dir=out_dir, numbers=[numbers[0], numbers[1] - 1]))


def AddOne(registry, number):
  return number + 1


def WriteAsString(registry, out_dir, contents, out_file):
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(str(contents), out_file))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'WriteAsString.count'))
