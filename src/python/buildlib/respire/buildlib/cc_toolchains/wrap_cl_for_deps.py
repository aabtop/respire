'''Script to wrap MSVC cl.exe and parse its /showIncludes output.

cl.exe /showIncludes will dump a list of includes to standard output.  This
script will parse those includes, and write respire-compatible dependency file
output to the filepath specified as the first argument to this script.
The remaining output will continue to be sent to stdout.
'''

import argparse
import os
import sys
import subprocess

PARSE_STRING = 'Note: including file:'
PARSE_STRING_LENGTH = len(PARSE_STRING)


def ProcessOutput(stdout, system_include_paths):
  system_include_paths_tuple = tuple(system_include_paths)

  include_paths = []

  for lineno, line in enumerate(stdout.readlines()):
    if lineno == 0:
      # Ignore the first line since it is just the name of the source file.
      # We eat this and do not forward on the output.
      continue

    if not line:
      continue

    if isinstance(line, bytes):
      line_str = line.decode('utf-8')
    else:
      line_str = line

    if line_str.startswith('Note: including file:'):
      include_path = line_str[PARSE_STRING_LENGTH:].strip()
      if not include_path.startswith(system_include_paths_tuple):
        include_paths.append(include_path)
      continue
    
    # Let execution flow out of here so we can forward this line to stdout.
    if sys.stdout:
      print(line_str.rstrip())

  return include_paths


def main():
  assert(len(sys.argv) >= 3)

  system_include_paths = []
  cur_param = 2
  while sys.argv[cur_param] == '/I':
    assert(len(sys.argv) > cur_param + 1)
    system_include_paths.append(sys.argv[cur_param + 1])
    cur_param += 2

  # Run the wrapped cl.exe process.
  cl_process = subprocess.Popen(
      sys.argv[cur_param:] + sys.argv[2:cur_param] + ['/showIncludes'],
      stdout=subprocess.PIPE)

  include_paths = ProcessOutput(cl_process.stdout, system_include_paths)

  cl_process.wait()

  if cl_process.returncode == 0:
    # Write out all discovered include paths to a respire-formatted deps file.
    with open(sys.argv[1], 'w') as f:
      f.write('\n'.join(include_paths))

  return cl_process.returncode


if __name__ == "__main__":
  sys.exit(main())
