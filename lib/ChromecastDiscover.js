//jshint esversion: 6
const mdns = require('mdns');
const event = require('events');

class ChromecastDiscover extends event.EventEmitter {
    constructor() {
        super();
        this._devices = {};
        this._devicesList = [];
        this._browser = mdns.createBrowser(mdns.tcp('googlecast'));
        this._browser.on('serviceUp', (s) => {
            this._devices[s.name] = s;
            this._devicesList.push(s.name);
            this.emit('deviceUp', s.name);
        });
        this._browser.on('serviceDown', (s) => {
            let pos = this._devicesList.indexOf(s.name);
            delete this._devices[s.name];

            for(let i = pos; i < this._devicesList.length; i++) {
                this._devicesList[i] = this._devicesList[i+1];
            }
            this._devicesList.pop();
            this.emit('deviceDown', s.name);
        });
    }

    start() {
        this._browser.start();
    }

    stop() {
        this._browser.stop();
    }

    getDeviceAddress(name) {
        return this._devices[name].addresses[0];
    }

    getDeviceNameForNumber(num) {
        return this._devicesList[num];
    }

    forEachClient(doSomething) {
        for(let name in this._devices) {
            doSomething(name);
        }
    }
}

module.exports = ChromecastDiscover;
