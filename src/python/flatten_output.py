import sys

import json_utils


def main():
  if len(sys.argv) != 3:
    print('Incorrect usage, expected 2 parameters.')
    return 1

  output_filepath = sys.argv[1]
  flattened_output_filepath = sys.argv[2]

  DoFlatten(output_filepath, flattened_output_filepath)

  return 0


def DoFlatten(output_filepath, flattened_output_filepath):
  with open(output_filepath, 'r') as f:
    output_content = f.read()

  # Decode the data, and let the decode function handle the Future
  # flattening.
  output = json_utils.DecodeFromJSONWithFlattening(
      output_content, expand_objects=False)

  # Re-encode the flattened data for writing.
  (flattened_output_content, futures) = json_utils.EncodeToJSON(output)
  if futures:
    raise Exception('Expected there to be no futures after flattening.')

  with open(flattened_output_filepath, 'w') as f:
    f.write(flattened_output_content)


if __name__ == "__main__":
  sys.exit(main())
