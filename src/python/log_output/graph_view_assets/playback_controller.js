const PlaybackState = Object.freeze({
  PAUSED: 'paused',
  PLAYING: 'playing',
  PLAYING_AT_END: 'playing-at-end'
});

const MILLISECONDS_PER_SECOND = 1000.0;
const MICROSECONDS_PER_SECOND = 1000000.0;

function PlaybackController(on_state_update) {
  var event_list = [];

  var state = null;

  var mutation_observers = []

  // Time in seconds for current playback position, relative to the event times.
  var playback_build_time_start = 0;

  // Time in seconds for real clock time when playback was started.
  var playback_real_time_start;

  // The time of the latest event that we've seen so far.
  var latest_event_time = 0;

  var latest_event_real_time_receipt = null;

  // Are we still expecting more events to arrive?
  var event_stream_finished = false;

  var next_request_animation_frame = null;

  // The event that is last reflected in the current state.
  var next_event_index = 0;

  // The current state, resulting from the accumulation of prior events.
  var current_state = new DataState();

  function ResetCurrentState() {
    current_state = new DataState();
    next_event_index = 0;
  }

  this.NumFetchedEvents = function() {
    return event_list.length;
  }

  this.NumEventsPlayed = function() {
    return next_event_index;
  }

  this.SetCurrentBuildTime = function(time) {
    var most_recent_event_time = MostRecentPlayedEventTime();
    if (most_recent_event_time != null) {
      if (time < most_recent_event_time) {
        // If we move time back before the most recent event, we'll have to
        // re-accumulate all preceeding events to build the new state again.
        ResetCurrentState();
      }
    }

    playback_build_time_start = time;
    playback_real_time_start = performance.now() / MILLISECONDS_PER_SECOND;

    AdvanceNodeState(this)
    SignalMutation();
  }

  this.LatestEventTime = function() {
    return latest_event_time;
  }

  function SignalMutation() {
    mutation_observers.forEach((o) => { o(); });
  }

  function AdvanceNodeState(controller) {
    // Update the current event index.
    var previous_event_index = next_event_index;
    
    var current_build_time = controller.CurrentTimeInSeconds();
    if (next_event_index < event_list.length && 'time_us' in event_list[next_event_index]) {
    }
    while (next_event_index < event_list.length &&
           (!('time_us' in event_list[next_event_index]) ||
            event_list[next_event_index].time_us / MICROSECONDS_PER_SECOND
                <= current_build_time)) {
      ++next_event_index;
    }
    // Advance the current state.
    if (previous_event_index != next_event_index) {
      current_state.ProcessEvents(
          event_list.slice(previous_event_index, next_event_index));

      on_state_update(current_state.nodes_data, current_state.edges_data);
    }
  }

  function RefreshTimeline(controller) {
    next_request_animation_frame = null;

    AdvanceNodeState(controller);

    if (event_stream_finished &&
        (state == PlaybackState.PLAYING_AT_END || 
         (state == PlaybackState.PLAYING &&
          controller.CurrentTimeInSeconds() >=
              controller.EndTimeInSeconds()))) {
      controller.SetState(PlaybackState.PAUSED);
    }

    SignalMutation();

    // Decide whether it is necessary or not to register a future refresh.
    if (state != PlaybackState.PAUSED) {
      next_request_animation_frame =
          requestAnimationFrame(
              (timestamp) => { RefreshTimeline(controller); });
    }
  }

  this.RegisterMutationObserver = function(observer) {
    mutation_observers.push(observer);
  }

  this.SetState = function(new_state) {
    if (state == new_state) {
      return;
    }

    if (new_state == PlaybackState.PLAYING_AT_END) {
      if (event_stream_finished) {
        // Prohibit entering the "playing at end" state if the stream is complete.
        // Instead treat this as a "seek-to-end".
        this.SetCurrentBuildTime(latest_event_time);
      }
    } else if (new_state == PlaybackState.PAUSED ||
               new_state == PlaybackState.PLAYING) {
      this.SetCurrentBuildTime(this.CurrentTimeInSeconds());
    }

    state = new_state;

    RefreshTimeline(this);

    SignalMutation();
  }

  this.State = function() {
    return state;
  }

  this.ProcessNewEvents = function(events) {
    if (events == null) {
      event_stream_finished = true;
    } else {
      event_list = event_list.concat(events);
      if (event_list.length > 0) {
        latest_event_time =
            event_list[event_list.length - 1].time_us / MICROSECONDS_PER_SECOND;
        latest_event_real_time_receipt =
            performance.now() / MILLISECONDS_PER_SECOND;
      }
    }

    SignalMutation();
  }

  this.CurrentTimeInSeconds = function() {
    if (state == PlaybackState.PAUSED) {
      return playback_build_time_start;
    } else if (state == PlaybackState.PLAYING) {
      var playback_time = playback_build_time_start +
          (performance.now() / MILLISECONDS_PER_SECOND -
              playback_real_time_start);
      if (!event_stream_finished || playback_time < latest_event_time) {
        return playback_time;
      } else {
        return latest_event_time;
      }
    } else if (state == PlaybackState.PLAYING_AT_END) {
      if (event_stream_finished) {
        return latest_event_time;
      } else {
        return latest_event_time +
                   (performance.now() / MILLISECONDS_PER_SECOND -
                    latest_event_real_time_receipt);
      }
    }
  }

  this.EndTimeInSeconds = function() {
    return latest_event_time;
  }

  this.EventStreamFinished = function() {
    return event_stream_finished;
  }

  // The time that the most recently played event was played at.
  function MostRecentPlayedEventTime() {
    last_played_event_with_time_index = next_event_index;
    do {
      --last_played_event_with_time_index;
    } while (last_played_event_with_time_index >= 0 &&
             !('time_us' in event_list[last_played_event_with_time_index]));

    if (last_played_event_with_time_index > 0) {
      return event_list[last_played_event_with_time_index].time_us /
                 MICROSECONDS_PER_SECOND;
    } else {
      return null;
    }
  }

  this.StepBackEvent = function(index) {
    var target_previous_event_index = next_event_index;
    while (target_previous_event_index > 0) {
      --target_previous_event_index;
      var event = event_list[target_previous_event_index];
      if ('time_us' in event &&
          !current_state.WillEventBeIgnored(event)) {
        event_time = event.time_us /
                         MICROSECONDS_PER_SECOND;
        if (event_time < this.CurrentTimeInSeconds()) {
          this.SetCurrentBuildTime(event_time);
          return;
        }
      }
    }

    this.SetCurrentBuildTime(0);
  }

  this.StepForwardEvent = function() {
    var latest_event_time = -1;
    target_next_event_index = next_event_index;
    while (target_next_event_index < event_list.length) {
      ++target_next_event_index;
      var event = event_list[target_next_event_index - 1];
      if ('time_us' in event) {
        var event_time_in_seconds = event.time_us / MICROSECONDS_PER_SECOND;
        latest_event_time = Math.max(latest_event_time, event_time_in_seconds);
        if (current_state.WillEventBeIgnored(event)) {
          continue;
        }
        this.SetCurrentBuildTime(latest_event_time);
        return;
      }
    }
  }

  this.SetCurrentBuildTime(0);
  ResetCurrentState();
  this.SetState(PlaybackState.PLAYING_AT_END);
}
