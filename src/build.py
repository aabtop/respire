import os
import respire
import sys


def main():
  abs_script_path = os.path.abspath(os.path.dirname(__file__))
  default_out_dir = os.path.abspath(
      os.path.join(abs_script_path, os.path.pardir, 'out'))
  build_file = os.path.join(abs_script_path, 'build.respire.py')

  arg_parser = respire.GetStandardArgumentsParser(default_out_dir)
  arg_parser.add_argument(
      '-t', '--target', type=str,
      choices=['all', 'build', 'run_cpp_tests', 'package', 'run_package_tests'],
      default='build')
  arg_parser.add_argument(
      '-c', '--config', type=str,
      choices=['debug', 'release'], default='debug')
  arg_parser.add_argument(
      '-p', '--platform', type=str,
      choices=['raspi', 'host'], default='host')
  args = arg_parser.parse_args()

  if not respire.EntryPoint(
             build_file, 'EntryPoint', args,
             params={'target': args.target,
                     'config': args.config,
                     'platform': args.platform}):
    return 1
  else:
    return 0


if __name__ == "__main__":
  sys.exit(main())
