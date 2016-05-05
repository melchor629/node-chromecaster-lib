//jshint esversion: 6
const minimist = require('minimist');

module.exports = function ParseArguments(args) {
    let opt = {
        alias: {
            port: 'p',
            bitrate: 'b',
            samplerate: 's',
            name: 'n',
            device: 'd',
            help: 'h',
            list: 'l',
            iselect: 'i'
        },
        default: {
            port: 3000,
            bitrate: 320,
            samplerate: 44100,
            name: 'Chromecaster Audio Stream',
            searchDuration: 5
        },
        boolean: ['version', 'help', 'iselect', 'list'],
        string: ['name', 'device'],
        integer: ['bitrate', 'port', 'samplerate', 'searchDuration']
    };

    let s = args[0].indexOf('node') !== -1 ? args[1] : args[0];
    let helpMessage =
`Usage: ${s} [options]

Options:
    -p, --port       The port that the stream will listen on. [3000]
    -b, --bitrate    Bitrate of the encoded mp3 stream [320]
    -s, --samplerate Samplerate of the encoded mp3 stream [44.1KHz]
    -n, --name       Name of the stream to be shown (only Chromecast)
    -d, --device     Name of the device to stream (by default, first one)
    -h, --help       Shows this help
    -l, --list       Lists all Chromecast & Chromecast Audio devices
    -i, --iselect    Lets you select which device to cast to
    -d, --searchDuration Time searching for a Chromecast [5 sec]`;

    let m = minimist(args, opt);
    m.showHelp = function showHelp() {
        console.log(helpMessage);
    };

    opt.boolean.forEach((i) => {
        if(m === null) return;
        if('undefined' !== typeof m[i] && 'boolean' !== typeof m[i]) {
            console.error("Expected %s to be true/false value", i);
            m = null;
        }
    });

    opt.integer.forEach((i) => {
        if(m === null) return;
        if('undefined' !== typeof m[i] && 'number' !== typeof m[i]) {
            console.error("Expected %s to be a number value", i);
            m = null;
        }
    });

    return m;
};
