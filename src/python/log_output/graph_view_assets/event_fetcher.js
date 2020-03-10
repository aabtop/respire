function EventFetcher(on_events_function) {
  this.FetchLoop = function() {
    fetch('/log_stream')
        .then((response) => { return response.json(); })
        .then((event_data) => {
          quit = false;
          if ('quit_key' in event_data[event_data.length - 1]) {
            quit = true;
            event_data.pop();
            on_events_function(null);
            console.log('got quit message');
          }

          on_events_function(event_data);

          if (!quit) {
            setTimeout(() => { this.FetchLoop(on_events_function); }, 0);
          }
        })
        .catch((error) => {
          console.error(error);
        });
  }
}
