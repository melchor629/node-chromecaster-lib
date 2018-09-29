//jshint esversion: 6
const mdns = require('multicast-dns');
const event = require('events');
const os = require('os');

const ChromecastDevice = require('./ChromecastDevice');

class ChromecastDiscover extends event.EventEmitter {
    constructor() {
        super();
        this._devices = {};
        this._devicesList = [];
        this._browser = null;
    }

    start() {
        this._devices = {};
        this._devicesList = [];
        this._browserv4 = mdns();
        this._browserv6 = Object.keys(ChromecastDiscover._getIPv6netif()).map((ifname) => mdns({ ip: 'ff02::fb', type: 'udp6', interface: `::%${ifname}` }));
        this._startBrowser(this._browserv4);
        this._browserv6.forEach(browser => this._startBrowser(browser));
    }

    stop() {
        this._browserv4.destroy();
        this._browserv4 = null;
        this._browserv6.forEach(browser => browser.destroy());
        this._browserv6 = null;
    }

    getDeviceAddress(name) {
        if(this._devices[name]) {
            return this._devices[name].addresses[0];
        }
        return null;
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
        if(typeof name === 'string' && this._devices[name]) {
            return new ChromecastDevice(this._devices[name]);
        } else if(typeof name === 'object' && name.addresses) {
            return new ChromecastDevice(name);
        }
        throw new ReferenceError(`Device named ${name} does not exist`);
    }

    _convertResponse(res) {
        const resp = {
            answers: res.answers,
            additionals: res.additionals.map(d => d.type === 'TXT' ? { ...d, data: d.data.map(buf => buf.toString('UTF-8')) } : d),
        };

        const converted = { addresses: [] };
        for(let entry of resp.additionals) {
            if(entry.type === 'TXT') {
                for(let data of entry.data) {
                    if(data.startsWith('md=')) {
                        converted.type = data.substr(3);
                    } else if(data.startsWith('fn=')) {
                        converted.name = data.substr(3);
                    }
                }
            } else if(entry.type === 'A') {
                converted.addresses = converted.addresses.concat(entry.data);
            } else if(entry.type === 'AAAA') {
                converted.addresses = converted.addresses.concat(entry.data);
            }
        }

        return converted;
    }

    _startBrowser(browser) {
        browser.on('response', (res) => {
            const dev = this._convertResponse(res);
            this._devices[dev.name] = dev;
            this._devicesList.push(dev);
            this.emit('deviceUp', dev.name); //TODO remove some day (obsolete)
            this.emit('device', dev);
        });
        browser.query({
            questions: [{
                name: '_googlecast._tcp.local',
                type: 'PTR',
            }],
        });
    }

    static _getIPv6netif() {
        const ifaces = os.networkInterfaces();
        const o = {};
        Object.keys(ifaces).forEach((ifname) => {
            o[ifname] = ifaces[ifname]
                .filter((iface) => 'IPv6' === iface.family && !iface.internal)
                .map((iface) => iface.address);
        });
        return o;
    }
}

module.exports = ChromecastDiscover;
