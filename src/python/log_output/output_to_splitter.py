from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import log_output.output_to

class OutputToSplitter(log_output.output_to.OutputTo):
  def __init__(self, outputs):
    self.outputs = outputs

  def __enter__(self):
    for output in self.outputs:
      output.__enter__()
    return self

  def __exit__(self, *args):
    for output in self.outputs:
      output.__exit__(args)

  def OnEvent(self, event):
    for output in self.outputs:
      output.OnEvent(event)

  def OnTotalProgressTasksUpdated(self, total):
    for output in self.outputs:
      output.OnTotalProgressTasksUpdated(total)

  def OnTaskStarted(self, event, counts_towards_progress):
    for output in self.outputs:
      output.OnTaskStarted(event, counts_towards_progress)

  def OnTaskEnded(self, start_event, event, counts_towards_progress):
    for output in self.outputs:
      output.OnTaskEnded(start_event, event, counts_towards_progress)

  def OnRespireError(self, event):
    for output in self.outputs:
      output.OnRespireError(event)
