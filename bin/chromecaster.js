//jshint esversion: 6
const AudioInput = require('../lib/AudioInput');
const Webcast = require('../lib/Webcast.js');
const ChromecastDiscover = require('../lib/ChromecastDiscover');
const args = require('../lib/ParseArguments')(process.argv);

const lame = require('lame');
const fs = require('fs');
const Client = require('castv2-client').Client;
const DefaultMediaReceiver = require('castv2-client').DefaultMediaReceiver;

if(args === null) return;

class Chromecaster {
    constructor() {
        process.on('SIGINT', this.destructor.bind(this));
        this.enc = new lame.Encoder({
            channels: 2,
            bitDepth: 16,
            sampleRate: args.samplerate,

            bitRate: args.bitrate,
            outSampleRate: args.samplerate,
            mode: lame.STEREO
        });
        this.audioInput = new AudioInput({ samplerate: args.samplerate });
        this.webcast = new Webcast({ port: args.port });
        this.chromecastDiscover = new ChromecastDiscover();
        this.exitCode = 0;

        this.audioInput.on('data', this.enc.write.bind(this.enc));
        this.enc.on('data', this.webcast.write.bind(this.webcast));

        if(args.list) {
            this.list(this.destructor.bind(this));
        } else if(args.device) {
            this.castToTheDevice(args.device);
        } else if(args.iselect) {
            this.list();
        } else if(args.help) {
            args.showHelp();
            this.destructor();
        } else {
            this.castToTheFirstUp();
        }
    }

    list(done) {
        let i = 0;
        console.log("Searching Chromcasts...");
        this.chromecastDiscover.start();
        this.chromecastDiscover.on('deviceUp', function(name) {
            console.log("  %d. %s", i++, name);
        });
        setTimeout(() => {
            this.chromecastDiscover.stop();
            done();
        }, args.searchDuration * 1000);
    }

    castToTheFirstUp() {
        console.log("Searching for a Chromecast...");
        this.chromecastDiscover.on('deviceUp', (name) => {
            this.chromecastDiscover.stop();
            this.connect(name);
        });
        this.chromecastDiscover.start();
        setTimeout(() => {
            this.destructor();
        }, args.searchDuration * 1000);
    }

    castToTheDevice(name) {
        this.chromecastDiscover.on('deviceUp', (named) => {
            if(name.toLowerCase() === named.toLowerCase()) {
                this.chromecastDiscover.stop();
                this.connect(named);
            }
        });
        this.chromecastDiscover.start();
    }

    connect(name) {
        console.log("Trying to cast to %s", name);
        this.client = new Client();
        this.client.on('error', (err) => {
            console.error("An error has ocurred while playing");
            this.destructor();
        });
        this.client.connect(this.chromecastDiscover.getDeviceAddress(name), () => {
            this.client.launch(DefaultMediaReceiver, (err, player) => {
                if(err) {
                    console.error("Something strange happend when trying to connect to the Chromecast: %s", err.message);
                    this.exitCode = 1;
                    this.destructor();
                } else {
                    player.on('status', function(status) {
                        console.log('Status of the cast: %s', status.playerState);
                    });
                    player.load({
                        contentId: 'http://' + this.webcast.localIp + ":" + this.webcast.port + "/",
                        contentType: 'audio/mpeg3',
                        streamType: 'LIVE',
                        metadata: {
                            type: 0,
                            metadataType: 0,
                            title: args.name
                        }
                    }, { autoplay: true }, (err, status) => {
                        if(err) {
                            console.error("Something strange happend when trying to play your sound: %s", err.message);
                            this.exitCode = 1;
                            this.destructor();
                        } else {
                            this.audioInput.start();
                            console.log('Cast has started (%s)', status.playerState);
                            console.log("^C to stop casting...");
                        }
                    });
                }
            });
        });
    }

    destructor() {
        this.audioInput.close();
        this.webcast.stop();
        this.chromecastDiscover.stop();
        if(this.client) this.client.close();
        process.exit(this.exitCode);
    }
}

new Chromecaster();
