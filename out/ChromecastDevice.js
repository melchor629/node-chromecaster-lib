'use strict';

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

//jshint esversion: 6
var Client = require('castv2-client').Client;
var DefaultMediaReceiver = require('castv2-client').DefaultMediaReceiver;

var events = require('events');

var ChromecastDevice = function (_events$EventEmitter) {
    _inherits(ChromecastDevice, _events$EventEmitter);

    function ChromecastDevice(device) {
        _classCallCheck(this, ChromecastDevice);

        var _this = _possibleConstructorReturn(this, Object.getPrototypeOf(ChromecastDevice).call(this));

        _this._device = device;
        _this._client = new Client();
        _this._client.on('error', function (err) {
            _this._client.close();
            _this.emit('error', err);
        });
        _this._client.on('status', function (status) {
            _this.emit('status', status);
        });
        return _this;
    }

    _createClass(ChromecastDevice, [{
        key: 'setWebcast',
        value: function setWebcast(webcast) {
            this._webcast = webcast;
        }
    }, {
        key: 'connect',
        value: function connect(streamName, cbk) {
            var _this2 = this;

            if (!this._webcast) throw new Error("No webcast client connected");

            cbk = 'function' === typeof streamName ? streamName : cbk;
            streamName = 'string' === typeof streamName ? streamName : 'Chromecaster lib stream';
            this._client.connect(this._device.addresses[0], function () {
                _this2._client.launch(DefaultMediaReceiver, function (err, player) {
                    if (err) cbk(err);else {
                        player.load({
                            contentId: 'http://' + _this2._webcast.localIp + ':' + _this2._webcast.port + '/',
                            contentType: _this2._webcast.contentType,
                            streamType: 'LIVE',
                            metadata: {
                                type: 0,
                                metadataType: 3,
                                title: streamName
                            }
                        }, { autoplay: true }, function (err, status) {
                            _this2._player = player;
                            if (err) cbk(err);else cbk(null, status);
                        });
                    }
                });
            });
        }
    }, {
        key: 'getVolume',
        value: function getVolume(cbk) {
            if (this._client) {
                this._client.getVolume(function (err, volume) {
                    cbk(err, volume.level, volume.muted);
                });
            } else {
                cbk(null, null);
            }
        }
    }, {
        key: 'setVolume',
        value: function setVolume(volume, cbk) {
            if (this._client) {
                volume = Math.max(0, Math.min(volume, 1));
                this._client.setVolume({ level: volume }, function (err, volume) {
                    (cbk || function () {})(err, volume.level);
                });
            } else {
                (cbk || function () {})(null, null);
            }
        }
    }, {
        key: 'isMuted',
        value: function isMuted(cbk) {
            if (this._client) {
                this._client.getVolume(function (err, volume) {
                    cbk(err, volume.muted);
                });
            } else {
                cbk(null, null);
            }
        }
    }, {
        key: 'setMuted',
        value: function setMuted(muted, cbk) {
            if (this._client) {
                this._client.setVolume({ muted: !!muted }, function (err, volume) {
                    (cbk || function () {})(err, volume.muted, volume.level);
                });
            } else {
                (cbk || function () {})(null, null);
            }
        }
    }, {
        key: 'play',
        value: function play(cbk) {
            if (this._client) {
                this._client.play(cbk);
            } else {
                (cbk || function () {})(null);
            }
        }
    }, {
        key: 'pause',
        value: function pause(cbk) {
            if (this._client) {
                this._client.pause(cbk);
            } else {
                (cbk || function () {})(null);
            }
        }
    }, {
        key: 'stop',
        value: function stop(cbk) {
            if (this._client) {
                this._client.stop(cbk);
            } else {
                (cbk || function () {})(null);
            }
        }
    }, {
        key: 'close',
        value: function close() {
            this._client.close();
            this._client = null;
        }
    }]);

    return ChromecastDevice;
}(events.EventEmitter);

module.exports = ChromecastDevice;