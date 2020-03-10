'''Script to ensure a file is deleted before running the given command on it.

This script is catered to the 'ar' command, specifically looking for the
archive file parameter.
'''

import os
import sys
import subprocess

def main():
  assert(len(sys.argv) >= 4)
  archive_file = sys.argv[3]
  if os.path.exists(archive_file):
    os.remove(archive_file)

  return subprocess.call(sys.argv[1:])


if __name__ == "__main__":
  sys.exit(main())
