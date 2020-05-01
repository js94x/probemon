project('probemon', 'c', version:'0.1')

src = [ 'probemon.c', 'parsers.c', 'queue.c', 'radiotap.c', 'logger_thread.c', 'db.c', 'manuf.c']
pcap_dep = dependency('pcap', version: '>1.0')
pthread_dep = dependency('threads')
sqlite3_dep = dependency('sqlite3')

executable('probemon', src,
  dependencies: [pcap_dep, pthread_dep, sqlite3_dep],
  install: true)
