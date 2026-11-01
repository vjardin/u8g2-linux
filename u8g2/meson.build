# enforce C compiler instead of C++ that is pulled by default by CMake
u8g2_csrc = u8g2_src / 'csrc'
u8g2_srcs = run_command('sh', '-c', 'ls "$1"/*.c', 'sh', u8g2_csrc, check : true).stdout().split()

u8g2_hdrs = ['u8g2.h', 'u8x8.h']
foreach h : u8g2_hdrs
  configure_file(input : u8g2_csrc / h, output : h, copy : true)
  install_headers(u8g2_csrc / h, subdir : 'u8g2')
endforeach

u8g2_lib = static_library('u8g2', u8g2_srcs,
  include_directories : include_directories(u8g2_csrc),
  c_args : ['-ffunction-sections', '-fdata-sections', '-w'],
  override_options : ['warning_level=0'],
  install : true,
)
u8g2 = declare_dependency(link_with : u8g2_lib,
  include_directories : include_directories('..'))
