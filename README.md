node-chromecaster-lib
===================
A library to send your input sound to the Chromecast.

Offers a simple way to get audio from system, discovering Chromecasts and sending audio to it. Audio codec is in your hands.

This version is in (_really_) alpha, so any help is welcome.

Installation
----------------
This library currently only works on OS X and Linux.
```
$ npm install chromecaster-lib
```

**Linux Users**: You must install *libpulse-dev* and *libavahi-compat-libdnssd-dev* packages before installing this one.

Example
------------
An example is the [CLI I created](https://github.com/melchor629/node-chromecaster-cli) to simply pass audio to the Chromecast via mp3 stream.

Another, quick, example (_using lame encoder_):

```javascript
const AudioInput = require('chromecaster-lib').AudioInput;
const Webcast = require('chromecaster-lib').Webcast;
const ChromecastDiscover = require('chromecaster-lib').ChromecastDiscover;
const lame = require('lame');

let cd = new ChromecastDiscover();
cd.on('deviceUp', function(name) {
    cd.stop();
    let client = cd.createClient(name);
    let encoder = new lame.Encoder({
        channels: 2,
        bitDepth: 16,
        sampleRate: 44100,
        bitRate: 320,
        outSampleRate: 44100,
        mode: lame.STEREO
    });
    let audioInput = new AudioInput();
    let webcast = new Webcast({ port: 8080 });

    audioInput.on('data', encoder.write.bind(encoder));
    encoder.on('data', webcast.write.bind(webcast));

    client.setWebcast(webcast);
    client.connect((err, status) => {
        if(err) {
            console.log("Could not connect to CA: %s", err);
        } else {
            console.log("Connected to CA");
        }
    });
});
cd.start();
```

API
-----
### AudioInput
inherits from EventEmitter

**constructor([options])**
Creates the object passing some options. `options` object can contain the following fields, and its default values

- `samplerate` *Sample rate of the input audio stream. Valid values are: 44100, 48000, 88200, 96000* [44100]
- `bps` *Bitdepth for sample. Could be 8, 16, 24, 32* [16]
- `channels` *Number of channels of the stream, 1 (mono) or 2 (stereo)* [2]
- `deviceName` *name of the device which capture the audio* [system default]
- `timePerFrame` *number of milliseconds to capture per frame* [100ms]

NOTE: Invalid values in the above options, will derive to default value.

**Number open()**
Opens the Input Audio Stream. If the return value is different from 0, then an error has occurred. In this case, see `AudioInput.AudioInputErrors`.

**close()**
Closes the Input Stream, in case it was opened.

**pause()**
(Un)Pauses the Input Stream.

**Boolean isOpen()**
Returns `true` if the stream is open, `false` otherwise.

**Boolean isPaused()**
Returns `true` if the stream is open and paused, or is closed.

**event 'data'**
Every frame is received, will be sent via this event. Event has only one argument: the interleaved audio buffer.

### Webcast
inherits from stream.Writable

**constructor([options])**
Creates a web server to send the input audio to the Chromecast (_or something else_), and opens the server. The options object and its default values:

- `port` *port to listen on* [3000]
- `contentType` *MIME type of the input stream* [audio/mp3]

**stop()**
Closes the server

**write(buffer [,  encoding, cbk])**
Writes some bytes to the clients that are listening. Encoding is usually omitted.

**get localIp**
Obtains the ip of the machine in the local network

**get contentType**
Obtains the contentType of the input stream, that is, the stream that will output to the server.

**get port**
Gets the port the server is listening on.

### ChromecastDiscover
inherits from events.EventEmitter

**constructor()**
Prepares the discovering of Chromecasts. No options required.

**start()**
Starts searching for Chromecasts.

**stop()**
Stops searching Chromecasts.

**String getDeviceAddress(name)**
Gets the IP address of the device named.

**String getDeviceNameForNumber(number)**
Gets the device number for the nth device that was found. `number` goes from 0 to devicesFound - 1.

**forEachClient(cbk)**
Does a forEach on every device found passing its name to the callback.

**ChromecastClient createClient(name)**
Creates a ChromecastDevice object for the named device.

**event 'deviceUp'**
Event emitted when a device has been found. Argument is the name of the device.

**event 'deviceDown'**
Event emitted when a device has been disconnected. Argument is the name of the device.

### ChromecastDevice
inherits from events.EventEmitter

**constructor(device)**
Is not recommended create ChromecastDevices directly, use instead `ChromecastDiscover.createClient()`.

**setWebcast(webcast)**
Sets the `Webcast` object that your are using. Is a **mandatory** call this method before  calling `connect` .

**connect(cbk)**
**connect(streamName, cbk)**
Connects to the Chromecast device and sets its stream name to `streamName` of default name (`Chromecaster lib stream`). Callback have a signature of `function(err, status)`.

Any error in the connexion, is reported calling the callback with `err` set to the error.

If the connexion succeeds, `cbk` will be called with `err` set to `null` and with the status of the device. The device will play automatically.

**getVolume(cbk)**
Gets the volume of the device asynchronously. The signature of `cbk` is `function(err, volume)` where `volume` is a number between 0 and 1. If the client is not connected, `err` and `volume` are null.

**setVolume(volume [, cbk])**
Sets the volume of the device asynchronously.  Volume is a number [0,1]. Values not in the range are clamped. The signature of `cbk` is `function(err, volume)` where `volume` is a number between 0 and 1. If the client is not connected, `err` and `volume` are null.

**isMuted(cbk)**
Gets if the device is muted or not asynchronously. `cbk` has signature of `function(err, muted)`.

**setMute(muted [, cbk])**
Change mute of the device asynchronously. `cbk` has signature of `function(err, muted)`.

**play([cbk])**
Notifies the Chromecast to start playing. By default, the Chromecast is always playing.

**pause([cbk])**
Notifies the Chromecast to pause the stream.

**stop([cbk])**
Notifies the Chromecast to stop the stream.

**close()**
Stops the stream and closes the connexion to the device.
