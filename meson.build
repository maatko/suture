project('suture', 'c', 'java', default_options : ['c_std=c11'])

jni = dependency('jni', version: '>=1.8', modules: ['jvm'], required: true)

sources = files('.clang-format')

subdir('include')
subdir('lib')

suture = shared_library(
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