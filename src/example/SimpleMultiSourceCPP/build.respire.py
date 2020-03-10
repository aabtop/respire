import os
import respire.buildlib.cc as cc 
import respire.buildlib.cc_toolchains.discovery as cc_discovery
import respire.buildlib.modules as modules

def EntryPoint(registry, out_dir):
  toolchain = cc_discovery.DiscoverHostToolchain()
  configured_toolchain = cc.ToolchainWithConfiguration(
      toolchain, cc.Configuration())

  main_module = modules.ExecutableModule(
      'simple_multi_source_cpp', registry, out_dir, configured_toolchain,
      sources=[
        'main.cc',
        'database.cc',
        'database.h',
      ])

  for output in main_module.GetOutputFiles():
    registry.Build(output)
