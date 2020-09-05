project('probemon', 'c', version: '0.2')

cc = meson.get_compiler('c')

conf_data = configuration_data()
conf_data.set('version', meson.project_version())
configure_file(input : 'config.h.in',
               output : 'config.h',
               configuration : conf_data)

src = ['probemon.c', 'parsers.c', 'queue.c', 'radiotap.c',
  'logger_thread.c', 'db.c', 'manuf.c', 'config_yaml.c', 'base64.c', 'lruc.c']
pcap_dep = dependency('pcap', version: '>1.0')
pthread_dep = dependency('threads')
sqlite3_dep = dependency('sqlite3')
yaml_dep = dependency('yaml-0.1')

if cc.has_header('sys/stat.h')
  add_project_arguments('-DHAS_SYS_STAT_H', language: 'c')
endif

executable('probemon', src,
  dependencies: [pcap_dep, pthread_dep, sqlite3_dep, yaml_dep],
  install: true)
