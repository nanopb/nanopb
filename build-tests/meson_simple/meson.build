nanopb_generator = generator(find_program(generator_path[0]), 
    arguments: [ generator_path[1], '-I', '@CURRENT_SOURCE_DIR@', '-D', '@BUILD_DIR@', '--strip-path', '@INPUT@' ],
    output: [ '@BASENAME@.pb.c', '@BASENAME@.pb.h' ]
)

generated_proto = nanopb_generator.process(files(
    'simple.proto'
), preserve_path_from: meson.current_source_dir())

executable(
    'simple',
    generated_proto,
    files(
        'simple.c'
    ),
    dependencies: nanopb_dep
)
