'''Simple log outputter that logs actions on to separate lines.

This is a more standard output format that doesn't require terminal escape
sequences.  It simply logs all activity to a file.
'''

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


import sys


import log_output.event_filtering as event_filtering
import log_output.log_command_output as log_command_output
import log_output.output_to


class OutputToLog(log_output.output_to.OutputTo):
  def __init__(self, respire_details_dir):
    self._progress_tasks_total = 0
    self._progress_tasks_completed = 0
    self._progress_tasks_active = 0
    self._active_tasks = {}
    self._output_logged = False
    self._respire_details_dir = respire_details_dir

  def __enter__(self):
    pass

  def __exit__(self, *args):
    if self._output_logged:
      sys.stdout.write('\n')
    pass

  def OnTotalProgressTasksUpdated(self, total):
    self._progress_tasks_total += 1

  def OnTaskStarted(self, event, counts_towards_progress):
    self._active_tasks[event['id']] = event
    if counts_towards_progress:
      self._progress_tasks_active += 1

    self._LogStatusUpdate(event)

  def OnTaskEnded(self, start_event, event, counts_towards_progress):
    if event['id'] not in self._active_tasks:
      return

    if (log_command_output.ShouldPrintCommandOutput(
            start_event, event, self._respire_details_dir)):
      log_command_output.LogCommandOutput(
          start_event, event, self._respire_details_dir)

    del self._active_tasks[event['id']]
    if counts_towards_progress:
      self._progress_tasks_active -= 1
      self._progress_tasks_completed += 1
    
    self._LogStatusUpdate(start_event, completed=True)

  def OnRespireError(self, event):
    if log_command_output.ShouldPrintCommandOutput(
           None, event, self._respire_details_dir):
      log_command_output.LogCommandOutput(
          None, event, self._respire_details_dir)


  def _LogStatusUpdate(self, start_event, completed=False):
    if completed:
      return

    progress_string = (
        '[%d / %d] ' % (self._progress_tasks_completed,
                        self._progress_tasks_total))

    event_summary_string = (
        event_filtering.SummaryStringForEvent(start_event))

    sys.stdout.write(
        '%s %s\n' % (progress_string, event_summary_string))
    sys.stdout.flush()

    self._output_logged = True
