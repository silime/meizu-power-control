project(
  'meizu-power-control',
  'c',
  version: '0.1.0',
  meson_version: '>= 0.62.0',
  default_options: ['warning_level=2', 'c_std=gnu11'],
)

i18n = import('i18n')

add_project_arguments(
  '-DGETTEXT_PACKAGE="' + meson.project_name() + '"',
  '-DLOCALEDIR="' + get_option('prefix') / get_option('localedir') + '"',
  language: 'c',
)

subdir('src')

install_data(
  'data/io.github.chalkin.MeizuPowerControl.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/io.github.chalkin.MeizuPowerControl.policy',
  install_dir: get_option('datadir') / 'polkit-1' / 'actions',
)

subdir('po')
