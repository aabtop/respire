'''Python script that implements the Unix `cat` command cross-platform

This script helps out with respire testing by providing a reliable `cat` method
that all platforms can rely on.

An extra 'count' parameter is expected, indicating a "count" file that will
be incremented when the function is called.
'''

import sys
import test_utils


def main():
  if len(sys.argv) < 3:
    print('Must supply at least 2 parameters.')

  output_string = ''
  for input_filepath in sys.argv[1:-2]:
    if input_filepath == 'stdin':
      output_string += sys.stdin.read()
    else:
      with open(input_filepath, 'r') as f:
        output_string += f.read()

  if sys.argv[-2] == 'stdout':
    sys.stdout.write(output_string)
  elif sys.argv[-2] == 'stderr':
    sys.stderr.write(output_string)
  else:
    with open(sys.argv[-2], 'w') as f:
      f.write(output_string)

  count_file = sys.argv[-1]
  if count_file != 'None':
    test_utils.AddCount(count_file)

  return 0


if __name__ == "__main__":
  sys.exit(main())
