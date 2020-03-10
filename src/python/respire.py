from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import multiprocessing
import os
import subprocess
import sys
import time

import json_utils
import log_output.log_processor as log_processor
import registry_helpers


def GetStandardArgumentsParser(default_out_dir):
  parser = argparse.ArgumentParser(description='Entry point to respire build.')
  parser.add_argument('-o', '--out_dir', type=str, default=default_out_dir,
                      help='The out directory where all generated files will '
                           'be placed.')
  parser.add_argument('-j', '--jobs', type=int, default=multiprocessing.cpu_count(),
                      help='Maximum number of jobs to run in parallel.')
  parser.add_argument('-v', '--verbose', action='store_true')
  parser.add_argument('-g', '--graph_view', action='store_true')
  parser.add_argument('-r', '--raw_logs', action='store_true',
                      help='Dump the raw unparsed JSON log output.')
  return parser


def EntryPoint(build_filepath, build_function_name, args, params=None):
  out_dir = os.path.abspath(args.out_dir)
  out_dir_respire = os.path.join(out_dir, '__respire_build_files')

  params_with_out_dir = {'out_dir': out_dir}
  if params:
    params_with_out_dir.update(params)

  return Run(
      out_dir_respire, build_filepath, build_function_name, params_with_out_dir,
      max_jobs=args.jobs, verbose=args.verbose, raw_logs=args.raw_logs,
      graph_view=args.graph_view)


def Run(out_dir, build_filepath, build_function_name, params,
        max_jobs=multiprocessing.cpu_count(), verbose=False, raw_logs=False,
        graph_view=False):
  # respire was freshly called from the system (and not recursively).
  if not os.path.exists(out_dir):
    # Create the initial registry file.
    os.makedirs(out_dir)

  log_dir = registry_helpers.GetCommandLogDirectory(out_dir)
  if not os.path.exists(log_dir):
    os.makedirs(log_dir)

  sub_respire_filepaths = registry_helpers.EnsureGenRegistryInputFilesExist(
      out_dir, build_filepath, build_function_name, params)

  return LaunchRespireCore(
      sub_respire_filepaths.gen_registry_filepath, max_jobs, out_dir,
      verbose, raw_logs, graph_view) == 0


def LaunchRespireCore(root_respire_file, max_jobs, out_dir, verbose, raw_logs,
                      graph_view):
  start_time = time.time()

  if 'linux' in sys.platform:
    RESPIRE_EXECUTABLE = 'respire'
  else:
    RESPIRE_EXECUTABLE = 'respire.exe'
  respire_command_path = os.path.join(
      os.path.dirname(__file__), RESPIRE_EXECUTABLE)

  output_level_flag = '-o'
  if graph_view:
    output_level_flag = '-oo'

  respire_command_line = [
      respire_command_path, '-j', str(max_jobs),
      output_level_flag,
      root_respire_file]

  if verbose:
    print('Respire core command line:\n%s\n' % (
          _FormatCommandLine(respire_command_line)))

  respire_process = subprocess.Popen(
      respire_command_line, stdout=subprocess.PIPE)

  log_processor.LoopOnConsoleRefresh(
      respire_process.stdout,
      out_dir,
      verbose=verbose, raw_logs=raw_logs, graph_view=graph_view)

  print('Time elapsed: %.3f' % (time.time() - start_time))

  exit_code = respire_process.wait()
  return exit_code


def _FormatCommandLine(cmd):
  return ' '.join([x if ' ' not in x else '"%s"' % (x) for x in cmd])


def main():
  args = registry_helpers.ParseRespireCommandLineArgs()

  # If running directly from the command line, we will parse and decode an
  # input parameters file before calling Run().
  with open(args.params_file, 'r') as f:
    params = json_utils.DecodeFromJSON(f.read())

  if Run(args.out_dir, args.build_file, args.function, params):
    return 0
  else:
    return 1


if __name__ == "__main__":
  sys.exit(main())
