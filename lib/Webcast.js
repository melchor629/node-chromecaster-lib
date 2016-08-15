//jshint esversion: 6
const express = require('express');
const interfaces = require('os').networkInterfaces();
const stream = require('stream');

class Webcast extends stream.Writable {
    constructor(opt) {
        super(opt);
        opt = opt || {};
        this._port = opt.port || 3000;
        this._contentType = opt.contentType || 'audio/mp3';
        this._app = express();
        this._connectedClients = [];

        this._app.get('/', (req, res) => {
            res.set({
                'Content-Type': this._contentType,
                'Connection': 'close'
            });

            var close = function close() {
                var idx = this._connectedClients.indexOf(res);
                this._connectedClients.splice(idx, 1);
                this.emit('disconnected', idx);
            };

            this._connectedClients.push(res);
            res.on('close', close.bind(this));
            res.on('finish', close.bind(this));
            this.emit('connected', this._connectedClients.length - 1, req, res);
        });

        this._app.head('/', (req, res) => {
            res.set({
                'Content-Type': this._contentType
            });
        });

        this._server = this._app.listen(this.port);
    }

    _write(buffer, enc, cbk) {
        if(buffer !== undefined && buffer !== null) {
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
        } else {
            cbk();
        }
    }

    _writev(inputs, cbk) {
        var cbkCalled = inputs.length + 1;
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

    get contentType() {
        return this._contentType;
    }

    get port() {
        return this._port;
    }
}

module.exports = Webcast;
