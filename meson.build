project('suture', 'c', 'java', default_options : ['c_std=c11'])

cc = meson.get_compiler('c')

clang = find_program('clang', required: false)
if clang.found() and cc.get_id() != 'clang'
  message('clang found but not the active compiler — consider: CC=clang meson setup build')
endif

jni = dependency('jni', version: '>=1.8', modules: ['jvm'], required: true)

sources = files()

subdir('include')
subdir('lib')

suture = static_library(
	'suture',
	sources, 
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