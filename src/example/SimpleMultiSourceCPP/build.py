import os
import respire
import sys


def main():
  abs_script_path = os.path.abspath(os.path.dirname(__file__))
  default_out_dir = os.path.abspath(
      os.path.join(abs_script_path, os.path.pardir, 'out'))
  build_file = os.path.join(abs_script_path, 'build.respire.py')

  arg_parser = respire.GetStandardArgumentsParser(default_out_dir)
  args = arg_parser.parse_args()

  if not respire.EntryPoint(
             build_file, 'EntryPoint', args):
    return 1
  else:
    return 0


if __name__ == "__main__":
  sys.exit(main())
