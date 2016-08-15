'use strict';

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

//jshint esversion: 6
var express = require('express');
var interfaces = require('os').networkInterfaces();
var stream = require('stream');

var Webcast = function (_stream$Writable) {
    _inherits(Webcast, _stream$Writable);

    function Webcast(opt) {
        _classCallCheck(this, Webcast);

        var _this = _possibleConstructorReturn(this, Object.getPrototypeOf(Webcast).call(this, opt));

        opt = opt || {};
        _this._port = opt.port || 3000;
        _this._contentType = opt.contentType || 'audio/mp3';
        _this._app = express();
        _this._connectedClients = [];

        _this._app.get('/', function (req, res) {
            res.set({
                'Content-Type': _this._contentType,
                'Connection': 'close'
            });

            var close = function close() {
                var idx = this._connectedClients.indexOf(res);
                this._connectedClients.splice(idx, 1);
                this.emit('disconnected', idx);
            };

            _this._connectedClients.push(res);
            res.on('close', close.bind(_this));
            res.on('finish', close.bind(_this));
            _this.emit('connected', _this._connectedClients.length - 1, req, res);
        });

        _this._app.head('/', function (req, res) {
            res.set({
                'Content-Type': _this._contentType
            });
        });

        _this._server = _this._app.listen(_this.port);
        return _this;
    }

    _createClass(Webcast, [{
        key: '_write',
        value: function _write(buffer, enc, cbk) {
            var _this2 = this;

            if (buffer !== undefined && buffer !== null) {
                var cbkCalled;

                (function () {
                    cbkCalled = _this2._connectedClients.length + 1;

                    var _cbk = function _cbk() {
                        if (--cbkCalled === 0) {
                            cbk();
                        }
                    };

                    _this2._connectedClients.forEach(function (res) {
                        res.write(buffer, enc, _cbk);
                    });
                    _cbk();
                })();
            } else {
                cbk();
            }
        }
    }, {
        key: '_writev',
        value: function _writev(inputs, cbk) {
            var _this3 = this;

            var cbkCalled = inputs.length + 1;
            var _cbk = function _cbk() {
                if (--cbkCalled === 0) {
                    cbk();
                }
            };

            inputs.forEach(function (obj) {
                _this3._write(obj.chunk, obj.encoding, _cbk);
            });
            _cbk();
        }
    }, {
        key: 'stop',
        value: function stop() {
            this._server.close();
        }
    }, {
        key: 'localIp',
        get: function get() {
            var ip = null;
            var eachIp = function eachIp(a) {
                if (a.family === 'IPv4' && a.internal === false && !ip) {
                    ip = a.address;
                }
            };

            for (var dev in interfaces) {
                interfaces[dev].forEach(eachIp);
            }

            return ip;
        }
    }, {
        key: 'contentType',
        get: function get() {
            return this._contentType;
        }
    }, {
        key: 'port',
        get: function get() {
            return this._port;
        }
    }]);

    return Webcast;
}(stream.Writable);

module.exports = Webcast;