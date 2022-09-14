project('obmc-yadro-fssync', 'cpp',
  default_options: [
    'warning_level=3',
    'werror=true',
    'cpp_std=c++17',
  ],
  license: 'Apache-2.0',
  version: '0.1',
)

conf = configuration_data()
conf.set_quoted('PROJECT_VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: conf)

fmt_dep = dependency('fmt')
sdeventplus_dep = dependency('sdeventplus')

executable(
  'fssyncd',
  [
    'src/main.cpp',
    'src/watch.cpp',
  ],
  dependencies: [
    fmt_dep,
    sdeventplus_dep,
  ],
  install: true,
)
