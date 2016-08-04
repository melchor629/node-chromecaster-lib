'use strict';

var _createClass = function () { function defineProperties(target, props) { for (var i = 0; i < props.length; i++) { var descriptor = props[i]; descriptor.enumerable = descriptor.enumerable || false; descriptor.configurable = true; if ("value" in descriptor) descriptor.writable = true; Object.defineProperty(target, descriptor.key, descriptor); } } return function (Constructor, protoProps, staticProps) { if (protoProps) defineProperties(Constructor.prototype, protoProps); if (staticProps) defineProperties(Constructor, staticProps); return Constructor; }; }();

function _classCallCheck(instance, Constructor) { if (!(instance instanceof Constructor)) { throw new TypeError("Cannot call a class as a function"); } }

function _possibleConstructorReturn(self, call) { if (!self) { throw new ReferenceError("this hasn't been initialised - super() hasn't been called"); } return call && (typeof call === "object" || typeof call === "function") ? call : self; }

function _inherits(subClass, superClass) { if (typeof superClass !== "function" && superClass !== null) { throw new TypeError("Super expression must either be null or a function, not " + typeof superClass); } subClass.prototype = Object.create(superClass && superClass.prototype, { constructor: { value: subClass, enumerable: false, writable: true, configurable: true } }); if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass; }

//jshint esversion: 6
var mdns = require('mdns');
var event = require('events');

var ChromecastDevice = require('./ChromecastDevice');

var ChromecastDiscover = function (_event$EventEmitter) {
    _inherits(ChromecastDiscover, _event$EventEmitter);

    function ChromecastDiscover() {
        _classCallCheck(this, ChromecastDiscover);

        var _this = _possibleConstructorReturn(this, Object.getPrototypeOf(ChromecastDiscover).call(this));

        _this._devices = {};
        _this._devicesList = [];
        _this._browser = mdns.createBrowser(mdns.tcp('googlecast'));
        _this._browser.on('serviceUp', function (s) {
            _this._devices[s.name] = s;
            _this._devicesList.push(s.name);
            _this.emit('deviceUp', s.name);
        });
        _this._browser.on('serviceDown', function (s) {
            var pos = _this._devicesList.indexOf(s.name);
            delete _this._devices[s.name];

            for (var i = pos; i < _this._devicesList.length; i++) {
                _this._devicesList[i] = _this._devicesList[i + 1];
            }
            _this._devicesList.pop();
            _this.emit('deviceDown', s.name);
        });
        return _this;
    }

    _createClass(ChromecastDiscover, [{
        key: 'start',
        value: function start() {
            this._browser.start();
        }
    }, {
        key: 'stop',
        value: function stop() {
            this._browser.stop();
        }
    }, {
        key: 'getDeviceAddress',
        value: function getDeviceAddress(name) {
            return this._devices[name].addresses[0];
        }
    }, {
        key: 'getDeviceNameForNumber',
        value: function getDeviceNameForNumber(num) {
            return this._devicesList[num];
        }
    }, {
        key: 'forEachClient',
        value: function forEachClient(doSomething) {
            for (var name in this._devices) {
                doSomething(name);
            }
        }
    }, {
        key: 'createClient',
        value: function createClient(name) {
            return new ChromecastDevice(this._devices[name]);
        }
    }]);

    return ChromecastDiscover;
}(event.EventEmitter);

module.exports = ChromecastDiscover;