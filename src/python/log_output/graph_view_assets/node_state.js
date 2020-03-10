const NodeState = Object.freeze({
  INACTIVE: 'inactive',
  ACTION_PENDING: 'action-pending',
  SCANNING_DEPENDENCIES: 'scanning-dependencies',
  EXECUTING: 'executing',
  COMPLETED_UPTODATE: 'completed-uptodate',
  COMPLETED_SUCCESS: 'completed-success',
  COMPLETED_ERROR: 'completed-error',
});
