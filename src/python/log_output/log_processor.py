from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import json
import sys


from log_output.output_to_graph_view import OutputToGraphView
from log_output.output_to_log import OutputToLog
from log_output.output_to_raw import OutputToRaw
from log_output.output_to_splitter import OutputToSplitter
from log_output.output_to_terminal_with_escape_codes import \
    OutputToTerminalWithEscapeCodes
import log_output.event_filtering as event_filtering


def LoopOnConsoleRefresh(log_byte_stream, respire_details_dir, verbose=False,
                         raw_logs=False, graph_view=False):
  '''Parses input bytes into events to update the console output with.

This function is meant to be a blocking function that takes over console
output, updating it with events that arrive from |log_byte_stream|.  The
parameter |respire_details_dir| specifies a directory that, when stdout/stderr
redirection is detected to go go in to those directories, it is printed to
the console's stdout as well.'''

  if not log_byte_stream:
    return

  with _EventProcessor(respire_details_dir, verbose=verbose, raw_logs=raw_logs,
                       graph_view=graph_view) as ep:
    for event in _ParseLog(log_byte_stream):
      ep.ProcessEvent(event)


def _ParseLog(log):
  while True:
    line = log.readline()
    if not line:
      break

    line_as_str = line
    if not isinstance(line, str):
      line_as_str = line.decode('utf-8')

    line_as_str = line_as_str.strip()
    if not line_as_str:
      continue

    yield json.loads(line_as_str.rstrip(','))


class _EventProcessor(object):
  def __init__(self, respire_details_dir, verbose=False, raw_logs=False,
               graph_view=False):
    # "Start events" are special because they contain all of the construction
    # parameters for the event, essentially defining the object represented
    # by the event ID.
    self._create_events = {}
    self._total_progress_events = set()
    self._events_processed = set()
    self._events_being_processed = set()
    self._verbose = verbose

    if raw_logs:
      self._output_to = OutputToRaw()
    else:
      if sys.stdout.isatty():
        self._output_to = OutputToTerminalWithEscapeCodes(respire_details_dir)
      else:
        self._output_to = OutputToLog(respire_details_dir)

    if graph_view:
      self._output_to = OutputToSplitter(
          [self._output_to, OutputToGraphView(respire_details_dir)])

  def __enter__(self):
    self._output_to.__enter__()
    return self

  def __exit__(self, *args):
    self._output_to.__exit__()

  def ProcessEvent(self, event):
    self._output_to.OnEvent(event)

    if (event['type'] == 'CreateSystemCommandNode' or
        event['type'] == 'CreateRegistryNode'):
      self._create_events[event['id']] = event
    elif event['type'] == 'ExecutingCommand':
      dry_run = False
      if 'dry_run' in event:
        dry_run = True
        del event['dry_run']
      self.ProcessExecuteCommandEvent(
          self._create_events[event['id']], dry_run)
    elif event['type'] == 'ProcessingComplete':
      self.ProcessProcessingCompleteEvent(
          self._create_events[event['id']], event)
    elif event['type'] == 'SignalRespireError':
      self.ProcessRespireErrorEvent(event)

  def ProcessExecuteCommandEvent(self, event, dry_run):
    if dry_run:
      self._total_progress_events.add(event['id'])
      self._output_to.OnTotalProgressTasksUpdated(
          len(self._total_progress_events))
      return

    self._events_being_processed.add(event['id'])

    if self._verbose or event_filtering.ShouldPubliclyLogEvent(event):
      self._output_to.OnTaskStarted(
          event,
          event['id'] in self._total_progress_events)

  def ProcessProcessingCompleteEvent(self, start_event, event):
    if ('error' not in event and
        event['id'] not in self._events_being_processed):
      return

    if event['id'] in self._events_being_processed:
      self._events_being_processed.remove(event['id'])
      self._events_processed.add(event['id'])

    if self._verbose or event_filtering.ShouldPubliclyLogEvent(start_event):
      self._output_to.OnTaskEnded(
          start_event, event,
          event['id'] in self._total_progress_events)

  def ProcessRespireErrorEvent(self, event):
    self._output_to.OnRespireError(event)
