import os
import registry
import end_to_end_tests.test_utils as utils

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

def TestBuild(registry, out_dir):
  out_dir = out_dir
  bar_path = os.path.join(SCRIPT_DIR, 'bar.txt')
  base_chain_out_file_1 = os.path.join(out_dir, 'chain_output_1')
  base_chain_out_file_2 = os.path.join(out_dir, 'chain_output_2')
  chain_results_1_path = os.path.join(out_dir, 'chain_results_1.txt')
  chain_results_2_path = os.path.join(out_dir, 'chain_results_2.txt')

  chain_results1 = registry.SubRespire(
      Chain, input=bar_path, base_out_file=base_chain_out_file_1,
                     number=3, out_dir=out_dir)

  chain_results2 = registry.SubRespire(
      Chain, input=bar_path, base_out_file=base_chain_out_file_2,
                     number=4, out_dir=out_dir)

  registry.SubRespire(
      WriteResults,
      results=chain_results1, out_file=chain_results_1_path,
              out_dir=out_dir)

  registry.SubRespire(
      WriteResults,
      results=chain_results2, out_file=chain_results_2_path,
              out_dir=out_dir)

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'chain.TestBuild.count'))


def Chain(registry, out_dir, input, number, base_out_file):
  out_file = base_out_file + str(number) + '.txt'
  registry.SystemCommand(
      inputs=[input],
      outputs=[out_file],
      command=utils.CatCommand([input, input], out_file))
  return_value = [[out_file, number]]
  if number > 1:
    return_value.append(
        registry.SubRespire(Chain, input=out_file,
                                   base_out_file=base_out_file,
                                   number=number - 1,
                                   out_dir=out_dir))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'Chain.count'))

  return return_value


def WriteResults(registry, out_dir, results, out_file):
  registry.SystemCommand(
      inputs=[],
      outputs=[out_file],
      command=utils.EchoCommand(
                  str(results[1][1][0][1]), out_file))

  # Track how many times this python is executed.
  utils.AddCount(os.path.join(out_dir, 'WriteResults.count'))
