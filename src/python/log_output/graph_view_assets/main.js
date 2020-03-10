NODE_RADIUS = 14;
ZOOM_TO_FIT_UPDATE_INTERVAL_IN_MS = 500;

// Setup a pulsing radius animation on executing nodes based on the NODE_RADIUS
// constant.
var animate_execute_node_style = document.createElement('style');
animate_execute_node_style.type = 'text/css';
// Note that this animation doesn't quite work on Firefox and Edge, since it's
// animating a SVG element attribute, not a CSS property, and only Chrome
// supports that.
animate_execute_node_style.innerHTML = `
@keyframes pulse {
    from {
      r: ${NODE_RADIUS}px;
    }

    to {
      r: ${NODE_RADIUS * 1.2}px;
    }
  }

  .node.executing {
    animation: pulse 400ms infinite alternate-reverse;
  }`;
document.getElementsByTagName('head')[0].appendChild(
    animate_execute_node_style);

function OnLoad() {
  var d3_container = document.getElementById('d3-container');
  var graph_view = new GraphVisualization(d3_container);
  var legend_container = document.getElementById('legend');
  PopulateLegend(legend_container);

  // Create an event parser to convert events in to node and edge data to
  // be consumed by the graph view.
  var playback_controller = new PlaybackController((node_data, edge_data) => {
    graph_view.UpdateData(node_data, edge_data);
  });

  // Hook up a UI view on top of the playback controller.
  var playback_controls_ui = new PlaybackControlsUI(playback_controller);

  // Make a HTTP request at the special URL that our log stream will be
  // delivered at.
  event_fetcher = new EventFetcher((events) => {
    playback_controller.ProcessNewEvents(events);
  });

  event_fetcher.FetchLoop();
}

window.addEventListener('load', OnLoad);
