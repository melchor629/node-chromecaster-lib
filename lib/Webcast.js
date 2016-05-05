//jshint esversion: 6
const express = require('express');
const interfaces = require('os').networkInterfaces();
const stream = require('stream');

class Webcast extends stream.Writable {
    constructor(opt) {
        super({write: Webcast._write, writev: Webcast._writev});
        opt = opt || {};
        this.port = opt.port || 3000;
        this._app = express();
        this._connectedClients = [];

        this._app.get('/', (req, res) => {
            res.set({
                'Content-Type': opt.contentType || 'audio/wav',
                'Transfer-Encoding': 'chunked'
            });

            var close = function close() {
                var idx = this._connectedClients.indexOf(res);
                this._connectedClients.splice(idx, 1);
                console.log("Client has disconnected");
            };

            this._connectedClients.push(res);
            res.on('close', close.bind(this));
            res.on('finish', close.bind(this));
            console.log("Client has connected, there's %d clients", this._connectedClients.length);
        });

        this._app.head('/', (req, res) => {
            res.set({
                'Content-Type': opt.contentType || 'audio/mpeg3',
                'Transfer-Encoding': 'chunked'
            });
        });

        this._server = this._app.listen(this.port);
    }

    _write(buffer, enc, cbk) {console.log("paco");
        var cbkCalled = this._connectedClients.length + 1;
        let _cbk = () => {
            if(--cbkCalled === 0) {
                cbk();
            }
        };

        this._connectedClients.forEach((res) => {
            res.write(buffer, enc, _cbk);
        });
        _cbk();
    }

    _writev(inputs, cbk) {
        var cbkCalled = this.inputs.length + 1;
        let _cbk = () => {
            if(--cbkCalled === 0) {
                cbk();
            }
        };

        inputs.forEach((obj) => {
            this._write(obj.chunk, obj.encoding, _cbk);
        });
        _cbk();
    }

    stop() {
        this._server.close();
    }

    get localIp() {
        var ip = null;
        const eachIp = (a) => {
            if (a.family === 'IPv4' && a.internal === false && !ip) {
                ip = a.address;
            }
        };

        for(var dev in interfaces) {
            interfaces[dev].forEach(eachIp);
        }

        return ip;
    }
}

module.exports = Webcast;
