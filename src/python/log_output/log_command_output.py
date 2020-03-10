from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


import os
import sys


def ShouldPrintCommandOutput(start_event, event, respire_details_dir):
  return ('error' in event or
          _ShouldPrintRedirect('stderr', start_event, respire_details_dir) or
          _ShouldPrintRedirect('stdout', start_event, respire_details_dir))


def LogCommandOutput(start_event, event, respire_details_dir):
  if start_event:
    sys.stdout.write('\n')
    if start_event['type'] == 'CreateSystemCommandNode':
      sys.stdout.write('(command)\n')
      sys.stdout.write(start_event['command'])
      sys.stdout.write('\n\n')
      _OutputErrorIfExists(event)
      sys.stdout.write('(outputs)\n')
      for o in start_event['outputs'] + start_event['soft_outs']:
        # Don't log output files if they are part of Respire's internal details.
        if not _PathInDir(o,respire_details_dir):
          sys.stdout.write(o)
          sys.stdout.write('\n')
      sys.stdout.write('\n')
      _PrintRedirectIfExists('stdout', start_event, respire_details_dir)
      _PrintRedirectIfExists('stderr', start_event, respire_details_dir)
      sys.stdout.write('\n')
    elif start_event['type'] == 'CreateRegistryNode':
      _OutputErrorIfExists(event)
      sys.stdout.write('(include path)\n')
      sys.stdout.write(start_event['path'])
      sys.stdout.write('\n')
  else:
    if event['type'] == 'SignalRespireError':
      sys.stdout.write(event['error'])
      sys.stdout.write('\n')


def _OutputErrorIfExists(event):
  # TODO, also consider writing out event.get('error') here, but the problem
  #       is that errors get forwarded and you see a *lot* of text get dumped
  #       out because.
  error = event.get('error')
  if error:
    sys.stdout.write('(error)\n')
    sys.stdout.write(error + '\n')


def _PathInDir(path, dir):
    real_path = os.path.realpath(path)
    real_dir = os.path.realpath(dir)
    return real_path.startswith(real_dir)


def _ShouldPrintRedirect(redirect_name, start_event, respire_details_dir):
  try:
    redirect_file = start_event[redirect_name]
  except KeyError as k:
    return False

  return (os.path.exists(redirect_file) and
          os.path.getsize(redirect_file) > 0 and
          _PathInDir(redirect_file, respire_details_dir))


def _PrintRedirectIfExists(redirect_name, start_event, respire_details_dir):
  if not _ShouldPrintRedirect(redirect_name, start_event, respire_details_dir):
    return

  try:
    sys.stdout.write('(')
    sys.stdout.write(redirect_name)
    sys.stdout.write(')\n')
    with open(start_event[redirect_name], 'r') as f:
      sys.stdout.write(f.read())
  except Exception as e:
    sys.stdout.write(str(e))

  sys.stdout.write('\n')
