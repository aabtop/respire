function GraphVisualization(container_div) {
  var simulation = d3.forceSimulation()
      .force("charge_force",
             d3.forceManyBody()
                   .strength(-1200)
                   .distanceMax(1000))
      .force("center_force", d3.forceCenter(0, 0))
      .force('collision', d3.forceCollide().radius(NODE_RADIUS))
      .alphaDecay(0.035);

  var svg = d3.select(container_div).append('svg')
      .attr('width', '100%')
      .attr('height', '100%');

  var defs = svg.append("defs")

  defs.append('marker')
      .attr('id', 'arrow')
      .attr('markerUnits', 'strokeWidth')
      .attr('markerWidth', 8)
      .attr('markerHeight', 8)
      .attr('viewBox', '0 0 8 8')
      .attr('refX', 7)
      .attr('refY', 4)
      .attr('orient', 'auto-start-reverse')
      .append('path')
          .attr('d', 'M1,1 L7,4 L1,7 L4,4 L1,1')
          .attr('class', 'arrow-head');

  var g = svg.append('g');

  var zoom_transform = null;
  var user_has_zoomed = false;
  var zoom = d3.zoom()
      .on('zoom', () => {
        if (d3.event.sourceEvent != null) {
          user_has_zoomed = true;
          // Cancel any existing transitions.
          svg.transition();
        }
        zoom_transform = d3.event.transform;
        g.attr('transform', d3.event.transform)
      });
  svg.call(zoom);

  // Start off with the view centered on the origin.
  svg.call(zoom.transform,
            d3.zoomIdentity.translate(container_div.clientWidth / 2,
                                      container_div.clientHeight / 2));

  var edges_container = g.append('g');
  var nodes_container = g.append('g');
  var labels_container = g.append('g');

  var current_selection = null;

  function ZoomToFit(transition_time_in_ms) {
    const PADDING_PERCENT = 0.75;

    var data_bounds = g.node().getBBox();
    var container = g.node().parentElement;
    var container_width = container.clientWidth,
        container_height = container.clientHeight;
    var width = data_bounds.width,
        height = data_bounds.height;
    var mid_x = data_bounds.x + width / 2,
        mid_y = data_bounds.y + height / 2;
    if (width == 0 || height == 0) return; // nothing to fit
    var scale = PADDING_PERCENT / Math.max(width / container_width,
                                           height / container_height);
    scale = Math.min(1, scale);
    var translate = [container_width / 2 - scale * mid_x,
                     container_height / 2 - scale * mid_y];

    svg.transition()
        .ease(d3.easeSin)
        .duration(transition_time_in_ms)
        .call(zoom.transform,
              d3.zoomIdentity.translate(translate[0], translate[1])
                             .scale(scale));
  }

  var timer = -1;
  function ResetZoomToFitTimer(time_in_ms, transition_time_in_ms) {
    if (timer != -1) {
      clearTimeout(timer);
      timer = -1;
    }
    timer = setTimeout(() => {
      if (!user_has_zoomed) {
        ZoomToFit(transition_time_in_ms);
      }
    }, time_in_ms);
  }

  this.UpdateData = function(node_data, edge_data) {
    var transition = d3.transition()
        .duration(500);

    var nodes = nodes_container.selectAll('.node').data(node_data);
    nodes.exit().transition(transition).style('opacity', 0).remove();
    nodes_enter = nodes.enter().append('circle');
    nodes_enter.attr('class', 'node')
               .attr("r", 0)
               .transition(transition)
                   .attr('r', NODE_RADIUS);
    nodes = nodes_enter.merge(nodes);
    nodes.attr('class', (d) => { return 'node ' + d.state; });

    var edges = edges_container.selectAll('.edge').data(edge_data);
    edges.exit().transition(transition).style('opacity', 0).remove();
    edges_enter = edges.enter().append('path');
    edges_enter.attr('class', 'edge')
               .attr('marker-start', 'url(#arrow)')
               .style('opacity', 0)
               .transition(transition)
                   .style('opacity', 1);
    edges = edges_enter.merge(edges);

    function LabelXPosition(d) { return d.x + NODE_RADIUS + 10; }
    function LabelYPosition(d) { return d.y - NODE_RADIUS * 2; }

    function UpdateLabels() {

      var label_fade_transition = d3.transition().duration(100);
      var labels =
          labels_container.selectAll('.node-label-fo').data(
              node_data.filter(
                  (d) => { return (d.state == NodeState.EXECUTING ||
                                   'mouseover' in d) &&
                                  'x' in d && 'y' in d; }),
              (d) => { return d.title; });

      labels_enter = labels.enter().append('foreignObject');
      node_label_fos = labels_enter.attr('class', 'node-label-fo')
                  .attr('x', LabelXPosition)
                  .attr('y', LabelYPosition)
                  .attr('width', 600)
                  .attr('height', 60);
      node_label_fos.style('opacity', 0)
                    .transition(label_fade_transition)
                        .style('opacity', 1);
      label_boxes = node_label_fos.append('xhtml:div');
      label_boxes.attr('class', 'node-label')
                 .html((d) => { return d.title; });

      labels.exit()
          .transition(label_fade_transition).style('opacity', 0).remove();
      return labels_enter.merge(labels);
    }

    labels = UpdateLabels();

    simulation
        .nodes(node_data)
        .force('links',
               d3.forceLink(edge_data)
                   .id((node_d) => { return node_d.index; })
                   .distance((d) => { return 200; })
                   .strength(1));

    quad_tree = BuildQuadTree(nodes);

    simulation.on('tick', () => {
      nodes.attr('cx', (d) => { return d.x; })
           .attr('cy', (d) => { return d.y; });
      labels.attr('x', LabelXPosition)
            .attr('y', LabelYPosition);

      edges.attr('d', (d) => {
        // Render a path from the source node's radius edge to the
        // destination node's radius edge.
        var dx = d.target.x - d.source.x,
            dy = d.target.y - d.source.y;
        if (Math.abs(dx) < 0.1 && Math.abs(dy) < 0.1) {
          return ''
        }
        var dr_inv = 1.0 / VectorLength([dx, dy]),
            dxr = dx * dr_inv * NODE_RADIUS,
            dyr = dy * dr_inv * NODE_RADIUS;
        return 'M' + (d.source.x + dxr) + ',' + (d.source.y + dyr) +
               'L' + (d.target.x - dxr) + ',' + (d.target.y - dyr);
      });

      quad_tree = BuildQuadTree(nodes);
    });

    // Hookup the mousemove event to search for nodes through the quadtree for
    // hover events.
    svg.on('mousemove', () => {
      if (!zoom_transform) {
        return;
      }

      // Functions for transforming/inverse transforming a 2D point by the
      // D3 Zoom transform. Useful for converting mouse coordinates to/from
      // the node graph space.
      function TransformPoint(point, transform) {
        return [(point[0] - transform.x) / transform.k,
                (point[1] - transform.y) / transform.k];
      }
      function InverseTransformPoint(point, transform) {
        return [point[0] * transform.k + transform.x,
                point[1] * transform.k + transform.y];
      }

      graph_space_mouse_point =
          TransformPoint([d3.event.clientX, d3.event.clientY], zoom_transform); 
      var node_element = quad_tree.find(
          graph_space_mouse_point[0], graph_space_mouse_point[1]);

      if (node_element) {
        var node_selection = d3.select(node_element);

        // Determine if the mouse is close enough to the closest node to engage
        // the hover logic or not.
        const NODE_HOVER_SCREEN_RADIUS = 60;
        var screen_space_node_pos = InverseTransformPoint(
            [node_selection.datum().x, node_selection.datum().y],
            zoom_transform);
        var mouse_to_node_screen = [
                screen_space_node_pos[0] - d3.event.clientX,
                screen_space_node_pos[1] - d3.event.clientY],
            mouse_to_node_screen_distance = VectorLength(mouse_to_node_screen);
        var mouse_to_node_graph = [
                node_selection.datum().x - graph_space_mouse_point[0],
                node_selection.datum().y - graph_space_mouse_point[1]],
            mouse_to_node_graph_distance = VectorLength(mouse_to_node_graph);
        if (mouse_to_node_screen_distance > NODE_HOVER_SCREEN_RADIUS &&
            mouse_to_node_graph_distance > NODE_RADIUS) {
          // The closest node is too far away (in screen space distance) to
          // engage the hover logic, so mark that there is no newly selected
          // node.
          node_element = null;
        }

        if (current_selection != node_element) {
          if (current_selection) {
            delete d3.select(current_selection).datum()['mouseover'];
            d3.select(current_selection).classed('mouse-over', false);
          }

          if (node_element) {
            node_selection.datum()['mouseover'] = true;
            node_selection.classed('mouse-over', true);
          }
          
          current_selection = node_element;
          labels = UpdateLabels();
        }
      }
    });

    if (nodes_enter.size() > 0) {
      if (!user_has_zoomed) {
        const TRANSITION_TIME_IN_MS = 1000;
        const RESET_ZOOM_TIMER_IN_MS = 1000;

        ZoomToFit(TRANSITION_TIME_IN_MS);
        ResetZoomToFitTimer(
            RESET_ZOOM_TIMER_IN_MS, TRANSITION_TIME_IN_MS)
      }
      simulation.alpha(1).restart();
    }
  }
}

function BuildQuadTree(nodes) {
  return d3.quadtree()
      .x((e) => { return d3.select(e).datum().x; })
      .y((e) => { return d3.select(e).datum().y; })
      .addAll(nodes.nodes());
}

function VectorLength(vector) {
  return Math.sqrt(vector[0] * vector[0] + vector[1] * vector[1]);
}
