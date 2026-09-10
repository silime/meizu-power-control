gtk_dep = dependency('gtk4', version: '>= 4.10')
adw_dep = dependency('libadwaita-1', version: '>= 1.4')

executable(
  'meizu-power-control',
  'main.c',
  dependencies: [gtk_dep, adw_dep],
  install: true,
)

executable(
  'meizu-power-control-helper',
  'helper.c',
  install: true,
  install_dir: get_option('libexecdir'),
)
