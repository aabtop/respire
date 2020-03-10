from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


class OutputTo(object):
  def __init__(self):
    pass

  def __enter__(self):
    pass

  def __exit__(self, *args):
    pass

  def OnEvent(self, event):
    pass

  def OnTotalProgressTasksUpdated(self, total):
    pass

  def OnTaskStarted(self, event, counts_towards_progress):
    pass

  def OnTaskEnded(self, start_event, event, counts_towards_progress):
    pass

  def OnRespireError(self, event):
    pass
