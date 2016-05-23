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
                    "sources": ["src/LinuxAudioInput.cpp"],
                    "include_dirs": [

                    ],
                    "link_settings": {
                        "libraries": [
                            "-lpulse"
                        ]
                    }
                }],
                ['OS=="mac"', {
                    "sources": ["src/OSXAudioInput.cpp"],
                    "link_settings": {
                        "libraries": [
                            "-framework CoreFoundation",
                            "-framework CoreAudio",
                            "-framework AudioToolbox"
                        ]
                    },
                    "include_dirs": [
                        "System/Library/Frameworks/CoreFoundation.framework/Headers",
                        "System/Library/Frameworks/AudioToolbox.framework/Headers",
                        "System/Library/Frameworks/CoreAudio.framework/Headers",
                    ],
                    'xcode_settings': {
                        'MACOSX_DEPLOYMENT_TARGET': '10.7',
                        'OTHER_CPLUSPLUSFLAGS': ['-std=c++11','-stdlib=libc++']
                    }
                }]
            ]
        }
    ]
}
