{
    "targets": [
        {
            "target_name": "AudioInputNative",
            "sources": ["src/wrappers.cpp"],
            "cflags": ["-std=c++11"],
            "include_dirs": [
                "src",
                "<!(node -e \"require('nan')\")"
            ],
            "xcode_settings": {
                "OTHER_FLAGS": ["-std=c++11"]
            },
            "conditions": [
                ['OS=="linux"', {
                    "sources": ["src/PortAudioInput.cpp"],
                    "link_settings": {
                        "libraries": [
                            "-lportaudio"
                        ]
                    }
                }],
                ['OS=="mac"', {
                    "sources": ["src/PortAudioInput.cpp"],
                    "link_settings": {
                        "libraries": [
                            '-L/usr/local/lib',
                            "-lportaudio"
                        ]
                    },
                    "include_dirs": [
                        "/usr/local/include",
                    ],
                    'xcode_settings': {
                        'MACOSX_DEPLOYMENT_TARGET': '10.7',
                        'OTHER_CPLUSPLUSFLAGS': ['-std=c++11','-stdlib=libc++']
                    }
                }],
                ['OS=="win"', {
                    "sources": ["src/PortAudioInput.cpp"],
                    "link_settings": {
                        "libraries": [
                            "<(PORTAUDIO_DIR)\\build\\msvc\\x64\\Release\\portaudio_x64.lib"
                        ]
                    },
                    "include_dirs": [
                        "<(PORTAUDIO_DIR)\\include"
                    ],
                    "defines": [
                        "UNICODE", "_UNICODE"
                    ]
                }]
            ]
        }
    ]
}
