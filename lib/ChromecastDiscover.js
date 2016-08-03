//jshint esversion: 6
const mdns = require('mdns-js');
const event = require('events');

const ChromecastDevice = require('./ChromecastDevice');

function getfn(txt) {
    for(let entry of txt) {
        if(entry.indexOf('fn=') === 0) {
            return entry.substr(3);
        }
    }
}

class ChromecastDiscover extends event.EventEmitter {
    constructor() {
        super();
        this._devices = {};
        this._devicesList = [];
        this._browser = mdns.createBrowser(mdns.tcp('googlecast'));
        this._browser.on('update', (s) => {
            if(s.type[0].name === 'googlecast') {
                let name = getfn(s.txt);
                this._devices[name] = s;
                this._devicesList.push(name);
                this.emit('deviceUp', name);
            }
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
        this._browser.on('ready', () => {
            this._browserReady = true;
        });
    }

    start() {
        if(!this._browserReady) {
            this._browser.on('ready', () => {
                this._browser.discover();
            });
        } else {
            this._browser.discover();
        }
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

    createClient(name) {
        return new ChromecastDevice(this._devices[name]);
    }
}

module.exports = ChromecastDiscover;
