{
  'targets': [
    {
      'target_name': 'webserial',
      'sources': [
        'src/serial-handle.cc',
        'src/webserial.cc',
      ],
      'include_dirs': ['libserialport'],
      'dependencies': ['libserialport'],
    },

    # libserialport
    {
      'target_name': 'libserialport',
      'product_prefix': 'lib',
      'type': 'static_library',
      'include_dirs': ['libserialport'],
      # 'dependencies': ['libserialport-configure'],
      'sources': [
        'libserialport/config.h',
        'libserialport/serialport.c',
        'libserialport/timing.c',
      ],
      'link_settings': {
        'libraries': [
          '-framework IOKit',
          '-framework CoreFoundation',
        ],
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAs': 2,
        },
      },
      'defines': [
        'LIBSERIALPORT_ATBUILD',
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'linux_termios.c',
          ],
          'libraries': [
            '-ludev',
          ],
          'defines': [
            'HAVE_LIBUDEV=1',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'macosx.c',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'disphelper',
          ],
        }],
      ],
    },

    # libserialport-configure
    {
      'target_name': 'libserialport-configure',
      'type': 'none',
      'actions': [
        {
          'action_name': 'configure',
          'message': 'running configure',
          'inputs': [''],
          'outputs': ['libserialport/config.h'],
          'action': [
            'eval',
            'cd libserialport && ./autogen.sh && ./configure',
          ],
        },
      ],
    },
  ],
}
