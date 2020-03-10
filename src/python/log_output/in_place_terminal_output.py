'''Functionality to enable in-place terminal output.

This is useful for implementing multi-line progress bars that update
in-place, i.e. without creating a new line for each text update.
'''

import ctypes
import os
import sys

import terminalsize

class TerminalLines(object):
  def __init__(self):
    self._lines = ['']
    self._cur_row = 0
    self._terminal_width = terminalsize.get_terminal_size()[0]

    if os.name == 'nt':
      # On Windows cmd, control codes must be explicitly enabled.
      ctypes.windll.kernel32.SetConsoleMode(
          ctypes.windll.kernel32.GetStdHandle(-12), 7)

    self.output_written = False

  def __enter__(self):
    return self

  def __exit__(self, *args):
    # Move our cursor to a brand new line to wrap things up.
    if self.output_written:
      self._MoveToRow(0)
      sys.stdout.flush()

  def NumLines(self):
    return len(self._lines)

  def GetLine(self, row):
    assert(row >= 0)
    assert(row < self.NumLines())
    return self._lines[row]

  def SetLine(self, row, contents):
    assert(row >= 0)

    if not self.output_written:
      self._ResetCurrentLine()
      self.output_written = True

    if row < self.NumLines():
      self._MoveToRow(row)
      self._ResetCurrentLine()
    else:
      # First move to the last line...
      self._MoveToRow(self.NumLines() - 1)
      # Then insert blank lines until we get to the row location.
      for i in range(self.NumLines(), row + 1):
        sys.stdout.write('\n')
        self._lines.append('')
        self._cur_row += 1

    truncated_contents = self._Truncate(contents)
    self._lines[row] = truncated_contents
    sys.stdout.write(truncated_contents)
    # Only flush if the contents are not a blank line, in an attempt to cut
    # down on flickering output.  Usually blank lines are immediately replaced
    # by fresh output.
    if contents:
      sys.stdout.flush()

  def ClearedOutput(self):
    '''Temporarily keeps the in-place text clear while this object is in scope.

    Useful for when one wishes to pause the in-place console text in order to
    print out actual log results (like warning/error messages), and then
    continue to print the in-place console text afterwards.'''

    class ClearedOutputScope(object):
      def __init__(self, terminal):
        self._terminal = terminal

      def __enter__(self):
        self._terminal._EnterClearOutput()

      def __exit__(self, *args):
        self._terminal._ExitClearOutput()

    return ClearedOutputScope(self)

  def _EnterClearOutput(self):
    for i in range(0, self.NumLines()):
      self._MoveToRow(i)
      self._ResetCurrentLine()
    self._MoveToRow(0)

  def _ExitClearOutput(self):
    sys.stdout.write('\n'.join(self._lines))
    self._cur_row = self.NumLines() - 1

  def _Truncate(self, contents):
    if len(contents) < self._terminal_width:
      return contents

    # If the terminal width is really small, do a trivial truncation leaving
    # only the end of the string.
    MIN_TERMINAL_WIDTH_MID_TRUNCATION = 10
    if self._terminal_width < MIN_TERMINAL_WIDTH_MID_TRUNCATION:
      return contents[-self._terminal_width + 1:]

    # If we have more columns to work with, truncate the string by adding
    # "..." to the middle of it.
    terminal_midpoint = int(self._terminal_width / 2)
    return (
        contents[:terminal_midpoint-3] + '...' + contents[-terminal_midpoint:])

  def _ResetCurrentLine(self):
    # Ensure that we start at the beginning of the current line.
    sys.stdout.write('\r')
    # Erase to end of line
    sys.stdout.write('\033[K')

  def _MoveToRow(self, row):
    assert(row >= 0)
    assert(row < self.NumLines())

    row_diff = row - self._cur_row
    if row_diff > 0:
      sys.stdout.write('\033[%dB' % (row_diff))
    elif row_diff < 0:
      sys.stdout.write('\033[%dA' % (-row_diff))
    self._cur_row = row
