project('u8g2-linux', 'c',
  version : '1.1.0',
  license : 'BSD-3-Clause',
  meson_version : '>= 0.56',
  default_options : [
    'c_std=c11',
    'warning_level=3',
    'buildtype=release',
  ],
)

add_project_arguments(
  '-Wshadow',
  '-Wstrict-prototypes',
  language : 'c',
)

cc = meson.get_compiler('c')

# u8g2 is built as a static library
# the purpose is to keep only the fonts and display setups a
# program uses
u8g2 = cc.find_library('u8g2', static : true)
if not cc.has_header('u8g2/u8g2.h')
  error('u8g2/u8g2.h not found')
endif
gpiod = dependency('libgpiod', version : '>= 2.0')

inc = include_directories('include')

lib = static_library('u8g2-linux',
  'src/u8g2-linux.c',
  include_directories : inc,
  dependencies : [gpiod],
  install : true,
)
install_headers('include/u8g2-linux.h')

import('pkgconfig').generate(lib,
  description : 'u8g2 callbacks for Linux I2C and GPIO',
  requires : gpiod,
  libraries : ['-lu8g2'],
)

executable('u8g2-oled',
  'src/u8g2-oled.c',
  include_directories : inc,
  link_with : lib,
  dependencies : [u8g2, gpiod],
  link_args : ['-Wl,--gc-sections'],
  install : true,
)
