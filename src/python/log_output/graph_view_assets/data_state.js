function DataState() {
  var startup_params = {}

  var node_id_to_nodes = {}
  var output_file_to_nodes = {}

  this.nodes_data = [];
  this.edges_data = [];
  var nodes_data = this.nodes_data;
  var edges_data = this.edges_data;

  function InitializeNodeData(node_data) {
    node_data.state = NodeState.INACTIVE;
    node_data.title = node_data.outputs[0];
  }

  this.WillEventBeIgnored = function(event) {
    if (!('respire_details_dir' in startup_params)) {
      return false;
    }

    if (event.type == 'CreateSystemCommandNode') {
      return event.outputs[0].startsWith(startup_params.respire_details_dir);
    } else if (event.id in node_id_to_nodes) {
      return false;
    }
    return true;
  }

  this.ProcessEvents = function(events) {
    var WillEventBeIgnored = this.WillEventBeIgnored;

    function ProcessEvent(event) {
      if (!('type' in event)) {
        console.error('Expected all events to have a "type" field.');
        console.error(event);
        return;
      }

      if (WillEventBeIgnored(event)) {
        return;
      }

      if (event.type == 'StartupParams') {
        console.log('Received startup params:');
        console.log(event.params);
        startup_params = event.params;
      } else if (event.type == 'CreateSystemCommandNode') {
        InitializeNodeData(event);

        node_id_to_nodes[event.id] = event;
        for (var i = 0; i < event.outputs.length; ++i) {
          output_file_to_nodes[event.outputs[i]] = event;
        }


        nodes_data.push(event);

        input_nodes = []
        for (var i = 0; i < event.inputs.length; ++i) {
          if (event.inputs[i] in output_file_to_nodes) {
            input_nodes.push(output_file_to_nodes[event.inputs[i]]);
          }
        }
        input_nodes.forEach((node) => {
          edges_data.push({source: event, target: node});
        });

        
      } else if (event.id in node_id_to_nodes) {
        node_data = node_id_to_nodes[event.id];

        if (event.type == 'ScanningDependencies' && !('dry_run' in event)) {
          node_data.state = NodeState.SCANNING_DEPENDENCIES;
        } else if (event.type == 'ExecutingCommand') {
          if ('dry_run' in event) {
            node_data.state = NodeState.ACTION_PENDING;
          } else {
            node_data.state = NodeState.EXECUTING;
          }
        } else if (event.type == 'ProcessingComplete') {
          if ('error' in event) {
            node_data.state = NodeState.COMPLETED_ERROR;
          } else {
            if (node_data.state != NodeState.ACTION_PENDING &&
                node_data.state != NodeState.EXECUTING) {
              node_data.state = NodeState.COMPLETED_UPTODATE;
            } else {
              if (!('dry_run' in event)) {
                node_data.state = NodeState.COMPLETED_SUCCESS;
              }
            }
          }
        }
      }
    }

    events.forEach(ProcessEvent);
  }
}
