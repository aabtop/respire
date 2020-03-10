'''Python script that implements the Unix `echo` command cross-platform

This script helps out with respire testing by providing a reliable `echo` method
that all platforms can rely on.

An extra 'count' parameter is expected, indicating a "count" file that will
be incremented when the function is called.
'''

import sys
import test_utils


def main():
  if len(sys.argv) < 4:
    print('Must supply at least 3 parameters.')

  if sys.argv[-2] == 'stdout':
    sys.stdout.write(sys.argv[1])
  elif sys.argv[-2] == 'stderr':
    sys.stderr.write(sys.argv[1])
  else:
    with open(sys.argv[2], 'w') as f:
      f.write(sys.argv[1])

  count_file = sys.argv[3]
  if count_file != 'None':
    test_utils.AddCount(count_file)

  return 0


if __name__ == "__main__":
  sys.exit(main())
