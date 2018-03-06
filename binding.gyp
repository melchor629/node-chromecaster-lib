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
                    "sources": ["src/PortAudioInput.cpp"]
                }],
                ['OS=="mac"', {
                    "sources": ["src/PortAudioInput.cpp"],
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
                    "include_dirs": [
                        "<(PORTAUDIO_DIR)\\include"
                    ]
                }]
            ]
        }
    ]
}
