//jshint esversion: 6
const Client = require('castv2-client').Client;
const DefaultMediaReceiver = require('castv2-client').DefaultMediaReceiver;

const events = require('events');

class ChromecastDevice extends events.EventEmitter {

    constructor(device) {
        super();
        this._device = device;
        this._client = new Client();
        this._client.on('error', (err) => {
            this._client.close();
            this.emit('error', err);
        });
        this._client.on('status', (status) => {
            this.emit('status', status);
        });
    }

    setWebcast(webcast) {
        this._webcast = webcast;
    }

    connect(streamName, cbk) {
        if(!this._webcast) throw new Error("No webcast client connected");

        cbk = 'function' === typeof streamName ? streamName : cbk;
        streamName = 'string' === typeof streamName ? streamName : 'Chromecaster lib stream';
        this._client.connect(this._device.addresses[0], () => {
            this._client.launch(DefaultMediaReceiver, (err, player) => {
                if(err) cbk(err);
                else {
                    player.load({
                        contentId: `http://${this._webcast.localIp}:${this._webcast.port}/`,
                        contentType: this._webcast.contentType,
                        streamType: 'BUFFERED',
                        metadata: {
                            metadataType: 0,
                            title: streamName
                        }
                    }, { autoplay: true }, (err, status) => {
                        this._player = player;
                        if(err) cbk(err);
                        else cbk(null, status);
                    });
                }
            });
        });
    }

    getVolume(cbk) {
        if(this._client) {
            this._client.getVolume(function(err, volume) {
                cbk(err, volume.level, volume.muted);
            });
        } else {
            cbk(null, null);
        }
    }

    setVolume(volume, cbk) {
        if(this._client) {
            volume = Math.max(0, Math.min(volume, 1));
            this._client.setVolume({level: volume}, function(err, volume) {
                (cbk||function(){})(err, volume.level);
            });
        } else {
            (cbk||function(){})(null, null);
        }
    }

    isMuted(cbk) {
        if(this._client) {
            this._client.getVolume(function(err, volume) {
                cbk(err, volume.muted);
            });
        } else {
            cbk(null, null);
        }
    }

    setMuted(muted, cbk) {
        if(this._client) {
            this._client.setVolume({muted: !!muted}, function(err, volume) {
                (cbk||function(){})(err, volume.muted, volume.level);
            });
        } else {
            (cbk||function(){})(null, null);
        }
    }

    play(cbk) {
        if(this._client) {
            this._client.play(cbk);
        } else {
            (cbk||function(){})(null);
        }
    }

    pause(cbk) {
        if(this._client) {
            this._client.pause(cbk);
        } else {
            (cbk||function(){})(null);
        }
    }

    stop(cbk) {
        if(this._client) {
            this._client.stop(cbk);
        } else {
            (cbk||function(){})(null);
        }
    }

    close() {
        this._client.close();
        this._client = null;
    }
}

module.exports = ChromecastDevice;
