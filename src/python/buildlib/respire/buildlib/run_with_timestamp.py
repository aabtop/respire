'''Script to run a command and touch a timestamp file upon success.
'''


import subprocess


def RunAndTimestampOnSuccess(timestamp_file, command):
  subprocess_return_code = subprocess.call(command)
  if subprocess_return_code == 0:
    open(timestamp_file, 'w').close()

  return subprocess_return_code
