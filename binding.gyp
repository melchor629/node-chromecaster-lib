{
    "targets": [
        {
            "target_name": "AudioInputNative",
            "sources": ["src/wrappers.cpp", "src/OSXAudioInput.cpp"],
            "cflags": ["-std=c++11"],
            "xcode_settings": {"OTHER_FLAGS": ["-std=c++11"]},
            "include_dirs": [
                "src",
                "System/Library/Frameworks/CoreFoundation.framework/Headers",
                "System/Library/Frameworks/AudioToolbox.framework/Headers",
                "System/Library/Frameworks/CoreAudio.framework/Headers",
                "<!(node -e \"require('nan')\")"
            ],
            "link_settings": {
                "libraries": [
                    "-framework CoreFoundation",
                    "-framework CoreAudio",
                    "-framework AudioToolbox"
                ]
            }
        }
    ]
}
