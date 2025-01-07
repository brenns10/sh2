project(
  'sh2', 'c',
  version : '0.0.0',
)

libsc_collections_dep = dependency(
  'libsc-collections',
  fallback: ['sc-collections', 'libsc_collections_dep'],
  version: '>=0.9.0',
)

sources = [
  'src/main.c',
]

executable(
  'sh2',
  sources,
  dependencies : [libsc_collections_dep],
)
