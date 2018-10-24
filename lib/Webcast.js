//jshint esversion: 6
const http = require('http');
const interfaces = require('os').networkInterfaces();
const stream = require('stream');

class Webcast extends stream.Writable {
    constructor(opt) {
        super(opt);
        opt = opt || {};
        this._port = opt.port || 3000;
        this._contentType = opt.contentType || 'audio/mp3';
        this._connectedClients = [];

        this._server = http.createServer((req, res) => {
            if(req.method === 'GET') {
                res.setHeader('Content-Type', this._contentType);
                res.setHeader('Cache-Control', 'no-cache');
                res.setHeader('Pragma', 'no-cache');
                res.setHeader('Date', new Date().toUTCString());
                res.setHeader('Expires', new Date(0).toUTCString());
                res.setHeader('Connection', 'close');

                const close = () => {
                    var idx = this._connectedClients.indexOf(res);
                    if(idx !== -1) {
                        this._connectedClients.splice(idx, 1);
                        this.emit('disconnected', {
                            id: idx,
                            address: req.socket.remoteAddress,
                            port: req.socket.remotePort,
                            family: req.socket.remoteFamily,
                            headers: req.headers
                        });
                    }
                };

                this._connectedClients.push(res);
                res.on('close', close);
                res.on('finish', close);
                this.emit('connected', {
                    id: this._connectedClients.length - 1,
                    address: req.socket.remoteAddress,
                    port: req.socket.remotePort,
                    family: req.socket.remoteFamily,
                    headers: req.headers
                });
            } else if(req.method === 'HEAD') {
                res.setHeader('Content-Type', this._contentType);
                res.setHeader('Cache-Control', 'no-cache');
                res.setHeader('Pragma', 'no-cache');
                res.setHeader('Date', new Date().toUTCString());
                res.setHeader('Expires', new Date(0).toUTCString());
                res.end();
            } else {
                res.status(400);
                res.end("Bad request");
            }
        });

        //Try ports until we get one that don't fail
        for(let i = 0; i < 1000; i++) {
            try {
                this._server.listen(this._port + i);
                this._port = this._port + i;
                break;
            } catch(e) {
                if(e.code !== 'EADDRINUSE' || i === 999) {
                    throw e;
                }
            }
        }
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
        for(let client of this._connectedClients) {
            client.end();
        }
        this._connectedClients = [];
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
