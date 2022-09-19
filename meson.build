project('obmc-yadro-fssync', 'cpp',
  default_options: [
    'warning_level=3',
    'werror=true',
    'cpp_std=c++17',
  ],
  license: 'Apache-2.0',
  version: run_command(
    '/usr/bin/git',
    '--git-dir=@0@/.git'.format(meson.source_root()),
    'describe', '--always', '--dirty', '--long',
    check: true,
  ).stdout().strip(),
)

conf = configuration_data()
conf.set_quoted('PROJECT_VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

fmt_dep = dependency('fmt')
sdeventplus_dep = dependency('sdeventplus')
phosphor_logging_dep = dependency('phosphor-logging')

executable(
  'fssyncd',
  [
    'src/main.cpp',
    'src/sync.cpp',
    'src/watch.cpp',
    'src/whitelist.cpp',
  ],
  dependencies: [
    fmt_dep,
    sdeventplus_dep,
    phosphor_logging_dep,
  ],
  install: true,
)
