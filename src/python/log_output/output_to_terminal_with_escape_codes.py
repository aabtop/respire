'''Fancy log outputter that makes use of terminal escape codes.

Will create separate row of terminal output to track each process being
executed, so that the user can see all items being processed in parallel.
This only works when terminal escape sequences are available to move the
cursor around on the terminal.
'''

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


import log_output.event_filtering as event_filtering
import log_output.output_to
import log_output.in_place_terminal_output as in_place_terminal_output
import log_output.log_command_output as log_command_output


class OutputToTerminalWithEscapeCodes(log_output.output_to.OutputTo):
  def __init__(self, respire_details_dir):
    self._terminal = in_place_terminal_output.TerminalLines()
    self._event_output_line_map = {}
    self._max_task_line = 0
    self._progress_tasks_total = 0
    self._progress_tasks_completed = 0
    self._progress_tasks_active = 0
    self._respire_details_dir = respire_details_dir

  def __enter__(self):
    self._terminal.__enter__()
    return self

  def __exit__(self, *args):
    if self._max_task_line > 0:
      self._terminal.SetLine(self._terminal.NumLines() - 1, '')

    self._terminal.__exit__(*args)

  def OnTotalProgressTasksUpdated(self, total):
    self._progress_tasks_total = total

  def OnTaskStarted(self, event, counts_towards_progress):
    line = self._FindUnusedOutputLine()
    self._event_output_line_map[event['id']] = line
    output_string = str(line + 1) + ': '
    output_string += event_filtering.SummaryStringForEvent(event)
    self._terminal.SetLine(line, output_string)

    if counts_towards_progress:
      self._progress_tasks_active += 1
      self._UpdateProgress()

  def OnTaskEnded(self, start_event, event, counts_towards_progress):
    if event['id'] not in self._event_output_line_map:
      return

    self._MaybeLogCommandOutput(start_event, event)

    line = self._event_output_line_map[event['id']]
    del self._event_output_line_map[event['id']]
    self._terminal.SetLine(line, '')

    if counts_towards_progress:
      self._progress_tasks_active -= 1
      self._progress_tasks_completed += 1
      self._UpdateProgress()

  def OnRespireError(self, event):
    self._MaybeLogCommandOutput(None, event)

  def _MaybeLogCommandOutput(self, start_event, event):
    if log_command_output.ShouldPrintCommandOutput(
           start_event, event, self._respire_details_dir):
      with self._terminal.ClearedOutput():
        log_command_output.LogCommandOutput(
            start_event, event, self._respire_details_dir)

  def _FindUnusedOutputLine(self):
    # Since the last line is reserved for the progress info, we search for
    # unused lines only in the lines before it.
    for i in range(0, self._max_task_line):
      if not self._terminal.GetLine(i):
        return i
    self._max_task_line += 1
    return self._max_task_line - 1


  def _UpdateProgress(self):
    progress_string = (
        '[%d / %d / %d]' % (self._progress_tasks_completed,
                            self._progress_tasks_active,
                            self._progress_tasks_total))

    self._terminal.SetLine(self._max_task_line, progress_string)
