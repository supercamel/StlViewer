project('stlviewer-native', 'c', version: '0.1.0')

gtk_dep = dependency('gtk4')
epoxy_dep = dependency('epoxy')
gio_dep = dependency('gio-2.0')
math_dep = meson.get_compiler('c').find_library('m', required: false)

stlviewer_sources = [
  'stl-viewer-gl-area.c',
  'stl-viewer-gl-area.h',
]

stlviewer_lib = shared_library(
  'stlviewer-1.0',
  stlviewer_sources,
  dependencies: [gtk_dep, epoxy_dep, gio_dep, math_dep],
  include_directories: include_directories('.'),
  install: false,
)

if get_option('generate_gir')
  gnome = import('gnome')
  gnome.generate_gir(
    stlviewer_lib,
    sources: stlviewer_sources,
    namespace: 'StlViewer',
    nsversion: '1.0',
    symbol_prefix: 'stl_viewer',
    identifier_prefix: 'StlViewer',
    includes: ['Gtk-4.0'],
    dependencies: [gtk_dep, gio_dep],
    install: false,
  )
endif
