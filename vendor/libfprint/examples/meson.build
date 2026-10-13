
examples = [
    'enroll',
    'identify',
    'img-capture',
    'manage-prints',
    'verify',
]

foreach example: examples
    executable(example,
        [ example + '.c', 'storage.c', 'utilities.c' ],
        dependencies: [
            libfprint_dep,
            glib_dep,
            gusb_dep,
        ],
    )
endforeach

if 'synatlsmoc' in drivers
    executable('verimark-smoke',
        'verimark-smoke.c',
        dependencies: [
            libfprint_dep,
            glib_dep,
        ],
        install: false,
    )
endif

executable('cpp-test',
    'cpp-test.cpp',
    dependencies: libfprint_dep,
)

if installed_tests
    install_subdir('prints',
        install_dir: installed_tests_testdir)
endif
