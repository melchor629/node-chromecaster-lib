//jshint esversion: 6
const mdns = require('mdns');
const event = require('events');

class ChromecastDiscover extends event.EventEmitter {
    constructor() {
        super();
        this._devices = {};
        this._browser = mdns.createBrowser(mdns.tcp('googlecast'));
        this._browser.on('serviceUp', (s) => {
            this._devices[s.name] = s;
            this.emit('deviceUp', s.name);
        });
        this._browser.on('serviceDown', (s) => {
            delete this._devices[s.name];
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

    forEachClient(doSomething) {
        for(let name in this._devices) {
            doSomething(name);
        }
    }
}

module.exports = ChromecastDiscover;
