project('suture', 'c', 'java', default_options : ['c_std=c11'])

cc = meson.get_compiler('c')

clang = find_program('clang', required: false)
if clang.found() and cc.get_id() != 'clang'
  message('clang found but not the active compiler — consider: CC=clang meson setup build')
endif

jni = dependency('jni', version: '>=1.8', modules: ['jvm'], required: true)

sources = files(
	'src/suture.c', 'src/hook.c',
	'src/flag.c', 'src/transform.c',
	'src/stream.c'
)

includes = files(
	'include/suture.h', 'include/suture/types.h',
	'include/suture/error.h', 'include/suture/hook.h',
	'include/suture/flag.h', 'include/suture/transform.h',
	'include/suture/stream.h', 'include/suture/opcodes.h'
)

suture = static_library(
	'suture',
	sources + includes,
	dependencies: jni, 
	include_directories: include_directories('include')
)

shared_library(
	'detour',
	files('examples/detour.c'),
	dependencies: jni,
	include_directories: include_directories('include'),
	link_with: suture
)