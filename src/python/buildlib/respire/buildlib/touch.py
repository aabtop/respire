'''Script to run a command and touch a timestamp file upon success.
'''


def Touch(outputs):
  for output in outputs:
    open(output, 'w').close()
  return 0
