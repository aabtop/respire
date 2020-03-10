function PopulateLegend(legend_container) {
  const NODE_SCALE = 0.8;
  const ICON_PADDING = 8;
  const PADDED_ICON_RADIUS = NODE_RADIUS * NODE_SCALE + ICON_PADDING;
  const ROW_HEIGHT = PADDED_ICON_RADIUS * 2;

  var legend_header = document.createElement('div');
  legend_header.classList.add('legend-header');
  legend_header.appendChild(document.createTextNode('Legend'));
  legend_header.style.paddingLeft = ICON_PADDING + 'px';
  legend_container.appendChild(legend_header);

  for (var state in NodeState) {
    const state_class = NodeState[state];
    const label_text =
        state_class
            .split('-')
            .map((s) => s.charAt(0).toUpperCase() + s.substring(1))
            .join(' ');

    var row_div = document.createElement('div');

    const SVG_NAMESPACE = 'http://www.w3.org/2000/svg';

    var icon_svg = document.createElementNS(SVG_NAMESPACE,'svg');
    icon_svg.setAttribute('width', ROW_HEIGHT + 'px');
    icon_svg.setAttribute('height', ROW_HEIGHT + 'px');
    icon_svg.style.verticalAlign = 'middle';

    var circle = document.createElementNS(SVG_NAMESPACE, 'circle');
    circle.style.transformOrigin = 'center';
    circle.style.transform = 'scale(' + NODE_SCALE + ')';
    circle.setAttribute('class', 'node ' + state_class);
    circle.setAttribute('r', NODE_RADIUS);
    circle.setAttribute('cx', PADDED_ICON_RADIUS);
    circle.setAttribute('cy', PADDED_ICON_RADIUS);
    icon_svg.appendChild(circle);
    row_div.appendChild(icon_svg);

    var span = document.createElement('span');
    span.classList.add('legend-label');
    span.style =
        'display: inline-block; vertical-align: middle; padding-right: ' +
        ICON_PADDING + 'px;';
    span.appendChild(document.createTextNode(label_text));
    row_div.appendChild(span);

    legend_container.appendChild(row_div);
  }
}
