from __future__ import absolute_import
from __future__ import division
from __future__ import print_function


try:
  # Python 2
  from SimpleHTTPServer import SimpleHTTPRequestHandler
  from BaseHTTPServer import HTTPServer as BaseHTTPServer
  from Queue import Queue
except ImportError:
  # Python 3
  from http.server import HTTPServer as \
      BaseHTTPServer, SimpleHTTPRequestHandler
  from queue import Queue

import copy
import json
import os
import sys
import threading
import webbrowser

import log_output.output_to


HTTP_PORT = 12877


class HTTPHandler(SimpleHTTPRequestHandler):
  def translate_path(self, path):
    path = SimpleHTTPRequestHandler.translate_path(self, path)
    relpath = os.path.relpath(path, os.getcwd())
    fullpath = os.path.join(self.server.base_path, relpath)
    return fullpath

  def log_message(self, format, *args):
    pass

  def do_GET(self):
    if self.path == '/log_stream':
      self._HandleLogStreamRequest()
    else:
      SimpleHTTPRequestHandler.do_GET(self)

  def _DrainQueue(self, event_queue):
    events = []
    # Wait for an event in the queue.
    next_event = event_queue.get()
    event_queue.task_done()
    if next_event == 'quit':
      return (events, False)
    else:
      events.append(next_event)

    # Drain all remaining events from the queue.
    while not event_queue.empty():
      next_event = event_queue.get()
      event_queue.task_done()
      if next_event == 'quit':
        return (events, False)
      else:
        events.append(next_event)

    return (events, True)

  def _HandleLogStreamRequest(self):
    assert(not self.server.stream_done)
    (events, stream_has_more_data) = self._DrainQueue(self.server.event_queue)
    if not stream_has_more_data:
      self.server.stream_done = True
      events.append({'quit_key': ' '})
      self.server.client_sent_end_event.set()

    self.send_response(200)
    self.end_headers()
    message = '[' + ', \n'.join([json.dumps(x) for x in events]) + ']\n'
    self.wfile.write(message.encode('utf-8'))


class HTTPServer(BaseHTTPServer):
  timeout = 1
  allow_reuse_address = True

  def __init__(self, base_path, server_address, input_params):
    self.base_path = base_path
    self.event_queue = Queue()
    self.stream_done = False
    self.client_sent_end_event = threading.Event()
    self.input_params = input_params

    self.event_queue.put({'type': 'StartupParams', 'params': input_params})

    BaseHTTPServer.__init__(self, server_address, HTTPHandler)


class ThreadedHTTPServer:
  def __init__(self, base_path, server_address, input_params):
    self._server = HTTPServer(base_path, server_address, input_params)
    self._thread = threading.Thread(target=self.run)
    self._thread.daemon = True

  def run(self):
    while not self._server.stream_done:
      self._server.handle_request()

  def PushEvent(self, event):
    self._server.event_queue.put(event)

  def Start(self):
    self._thread.start()

  def Shutdown(self):
    self._server.event_queue.put('quit')
    # Wait for the client to get in at least one event haul before we quit.
    WAIT_FOR_CLIENT_TIMEOUT = 5
    if not self._server.client_sent_end_event.wait(WAIT_FOR_CLIENT_TIMEOUT):
      sys.stderr.write(
          'Timeout waiting for client browser to read all graph view events.\n')
      self._server.stream_done = True
      self._server.socket.close()
    else:
      self._thread.join()


class OutputToGraphView(log_output.output_to.OutputTo):
  def __init__(self, respire_details_dir):
    self.respire_details_dir = respire_details_dir

  def __enter__(self):
    self._webserver = ThreadedHTTPServer(
        base_path=os.path.join(os.path.dirname(__file__), 'graph_view_assets'),
        server_address=('localhost', HTTP_PORT),
        input_params={'respire_details_dir': self.respire_details_dir})
    self._webserver.Start()

    webbrowser.open('http://localhost:' + str(HTTP_PORT) + '/index.html')
    return self

  def __exit__(self, *args):
    self._webserver.Shutdown()

  def OnEvent(self, event):
    self._webserver.PushEvent(copy.copy(event))

  def OnTotalProgressTasksUpdated(self, total):
    pass

  def OnTaskStarted(self, event, counts_towards_progress):
    pass

  def OnTaskEnded(self, start_event, event, counts_towards_progress):
    pass

  def OnRespireError(self, event):
    pass
