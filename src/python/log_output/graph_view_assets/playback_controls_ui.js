// -*- coding: utf-8 -*-

function PlaybackControlsUI(playback_controller) {
  var update_cursor_timer = null;

  var container_div = document.createElement('div');
  container_div.classList.add('playback-container');

  inner_div = document.createElement('div');
  inner_div.classList.add('playback-container-inner');

  function OnClickGoToStart() {
    playback_controller.SetState(PlaybackState.PAUSED);
    playback_controller.SetCurrentBuildTime(0);
  }

  function OnClickStepBack() {
    playback_controller.SetState(PlaybackState.PAUSED);
    playback_controller.StepBackEvent();
  }

  function OnClickPause() {
    playback_controller.SetState(PlaybackState.PAUSED);
  }

  function OnClickPlay() {
    playback_controller.SetState(PlaybackState.PLAYING);
  }

  function OnClickStep() {
    playback_controller.SetState(PlaybackState.PAUSED);
    playback_controller.StepForwardEvent();
  }

  function OnClickPlayFromHead() {
    playback_controller.SetState(PlaybackState.PLAYING_AT_END);
  }

  function OnClickProgressBar(progress) {
    if (progress == 1) {
      OnClickPlayFromHead();
    } else {
      playback_controller.SetState(PlaybackState.PAUSED);
      playback_controller.SetCurrentBuildTime(
          playback_controller.LatestEventTime() * progress);
    }
  }

  function UpdatePlaybackStateChange() {
    new_state = playback_controller.State();
    if (new_state == PlaybackState.PAUSED) {
      playback_buttons.paused_indicator.classList.add('selected');
      playback_buttons.playing_indicator.classList.remove('selected');
      playback_buttons.playing_from_head_indicator.classList.remove('selected');
    } else if (new_state == PlaybackState.PLAYING) {
      playback_buttons.paused_indicator.classList.remove('selected');
      playback_buttons.playing_indicator.classList.add('selected');
      playback_buttons.playing_from_head_indicator.classList.remove('selected');
    } else if (new_state == PlaybackState.PLAYING_AT_END) {
      playback_buttons.paused_indicator.classList.remove('selected');
      playback_buttons.playing_indicator.classList.remove('selected');
      playback_buttons.playing_from_head_indicator.classList.add('selected');
    }
  }

  function UpdateTimelineState() {
    progress_bar.SetEndTimeInSeconds(
        playback_controller.EndTimeInSeconds());
    progress_bar.SetCurrentTimeInSeconds(
        playback_controller.CurrentTimeInSeconds());

    if (playback_controller.EventStreamFinished()) {
      progress_bar.end_time_div.classList.remove('active');
    } else {
      progress_bar.end_time_div.classList.add('active');
    }
  }

  function UpdateEventTracker() {
    event_tracker.UpdateEventInfo(playback_controller.NumEventsPlayed(),
                                  playback_controller.NumFetchedEvents());
  }

  function OnPlaybackControllerUpdated() {
    UpdatePlaybackStateChange();
    UpdateTimelineState();
    UpdateEventTracker();
  }

  progress_bar = new ProgressBar(OnClickProgressBar);
  inner_div.appendChild(progress_bar.div);
  playback_buttons = CreatePlaybackButtons(
      OnClickGoToStart,
      OnClickStepBack,
      OnClickPause,
      OnClickPlay,
      OnClickStep,
      OnClickPlayFromHead);
  inner_div.appendChild(playback_buttons.playback_buttons_div);

  container_div.appendChild(inner_div);
  document.body.appendChild(container_div);

  event_tracker = new EventProgressWidget();
  inner_div.appendChild(event_tracker.div);

  playback_controller.RegisterMutationObserver(OnPlaybackControllerUpdated);
  OnPlaybackControllerUpdated();
}

function EventProgressWidget() {
  var event_progress_div = document.createElement('div');
  event_progress_div.classList.add('playback-event-progress');
  event_progress_div.classList.add('playback-text');

  this.UpdateEventInfo = function(events_played, events_fetched) {
    if (event_progress_div.firstChild) {
      event_progress_div.removeChild(event_progress_div.firstChild);
    }
    event_progress_div.appendChild(document.createTextNode(
        events_played + ' / ' + events_fetched));
  }

  this.div = event_progress_div;
}

function ProgressBar(on_progress_click) {
  var progress_bar_div = document.createElement('div');
  progress_bar_div.classList.add('playback-progress-bar-container');

  var timeline_div = document.createElement('div');
  timeline_div.classList.add('playback-timeline');

  var end_time_div = document.createElement('div');
  end_time_div.classList.add('playback-end-time');
  end_time_div.classList.add('playback-text');
  timeline_div.appendChild(end_time_div);

  var cursor_div = document.createElement('div');
  cursor_div.classList.add('playback-cursor');
  timeline_div.appendChild(cursor_div);

  var cursor_time_div = document.createElement('div');
  cursor_time_div.classList.add('playback-cursor-time');
  cursor_time_div.classList.add('playback-text');
  cursor_div.appendChild(cursor_time_div);

  progress_bar_div.appendChild(timeline_div);

  var current_time_in_seconds = 0;
  var end_time_in_seconds = 1;

  progress_bar_div.addEventListener('click', (event) => {
    var container_rect = progress_bar_div.getBoundingClientRect();
    var bar_rect = timeline_div.getBoundingClientRect();

    var click_relative_to_bar =
        (event.clientX - bar_rect.left) / bar_rect.width;
    click_relative_to_bar = Math.max(Math.min(click_relative_to_bar, 1), 0);
    on_progress_click(click_relative_to_bar);
  });

  function UpdateCursorPosition() {
    if (current_time_in_seconds >= end_time_in_seconds) {
      cursor_div.style.left = '100%';
    } else {
      cursor_div.style.left =
          ((current_time_in_seconds / end_time_in_seconds) * 100) + '%';
    }
  }

  this.div = progress_bar_div;
  this.end_time_div = end_time_div;

  this.SetCurrentTimeInSeconds = function(time_in_seconds) {
    if (cursor_time_div.firstChild) {
      cursor_time_div.removeChild(cursor_time_div.firstChild);
    }
    cursor_time_div.appendChild(document.createTextNode(
        time_in_seconds.toFixed(2) + 's'));
    
    current_time_in_seconds = time_in_seconds;
    UpdateCursorPosition();
  }
  
  this.SetEndTimeInSeconds = function(time_in_seconds) {
    if (end_time_div.firstChild) {
      end_time_div.removeChild(end_time_div.firstChild);
    }
    end_time_div.appendChild(document.createTextNode(
        time_in_seconds.toFixed(2) + 's'));

    end_time_in_seconds = time_in_seconds;
    UpdateCursorPosition();
  }
}

function CreatePlaybackButtons(
    on_click_go_to_start,
    on_click_step_back,
    on_click_pause,
    on_click_play,
    on_click_step,
    on_click_play_from_head) {
  var playback_buttons_div = document.createElement('div');
  playback_buttons_div.classList.add('playback-button-container');

  function CreateButton(on_click_function, icon_text, scale_x, translate_x) {
    var button_div = document.createElement('div');
    button_div.classList.add('playback-button');

    var inner_div = document.createElement('div');
    inner_div.appendChild(document.createTextNode(icon_text));

    if (scale_x != null) {
      inner_div.style.transform = 'scaleX(' + scale_x + ')';
      if (translate_x != null) {
        inner_div.style.transform += 'translateX(' + translate_x + ')';
      }
    }

    button_div.appendChild(inner_div);
    button_div.addEventListener('click', on_click_function);
    return button_div;
  }

  var pause_icon = '\u275a\u275a';
  var play_icon = '\u25b6';
  var step_icon = play_icon + '\u275a';
  var fast_forward_icon = play_icon + play_icon;
  var advance_to_end_icon = fast_forward_icon + '\u275a';

  playback_buttons_div.appendChild(
      CreateButton(on_click_go_to_start, advance_to_end_icon, -0.5, '-0.15em'));
  playback_buttons_div.appendChild(
      CreateButton(on_click_step_back, step_icon, -0.6));
  var pause_button =
      CreateButton(on_click_pause, pause_icon);
  playback_buttons_div.appendChild(pause_button);
  var play_button =
      CreateButton(on_click_play, play_icon, 1.0, '0.05em');
  playback_buttons_div.appendChild(play_button);
  playback_buttons_div.appendChild(
      CreateButton(on_click_step, step_icon, 0.6));
  var play_from_head_button =
      CreateButton(on_click_play_from_head,
                   advance_to_end_icon, 0.5, '-0.15em');
  playback_buttons_div.appendChild(play_from_head_button);

  return {'playback_buttons_div': playback_buttons_div,
          'paused_indicator': pause_button,
          'playing_indicator': play_button,
          'playing_from_head_indicator': play_from_head_button};
}