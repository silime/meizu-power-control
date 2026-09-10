project(
  'meizu-power-control',
  'c',
  version: '0.1.0',
  meson_version: '>= 0.62.0',
  default_options: ['warning_level=2', 'c_std=gnu11'],
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
