project(
    'nanopb',
    'c',
    license: 'zlib',
    version: '1.0.0',
    meson_version: '>=1.4.0'
)

c_args = []
link_with = []
include_directories = include_directories('.')

sources = []

build_common = false

if get_option('encoder').enabled()
    sources += files('pb_encode.c')
    build_common = true
endif

if get_option('decoder').enabled()
    sources += files('pb_decode.c')
    build_common = true
endif

if build_common
    sources += files('pb_common.c')
endif

nanopb_lib = static_library(
    'nanopb',
    sources,
    c_args: c_args,
    include_directories: include_directories
)

if get_option('generator').enabled()
    python = import('python')
    pyinstall = python.find_installation(
        modules: [ 'protobuf', 'grpcio-tools' ],
        required: false
    )
    if pyinstall.found()
        python_exe = find_program(pyinstall.full_path(), required: false)
    endif

    if not pyinstall.found() or not python_exe.found()
        # Fallback for the system python version, meson may find it's own python installation without modules
        python_exe = find_program(
            'python3'
        )
    endif

    generator_path = [ python_exe.full_path(), meson.current_source_dir() / 'generator/nanopb_generator' ]
endif

link_with += nanopb_lib

nanopb_dep = declare_dependency(
    include_directories: include_directories,
    link_with: link_with,
)

if get_option('examples').enabled()
    subdir('examples/meson_simple')
endif

meson.override_dependency('nanopb', nanopb_dep)
