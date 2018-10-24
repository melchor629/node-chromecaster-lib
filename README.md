node-chromecaster-lib
===================
A library to send your input sound to the Chromecast.

Offers a simple way to get audio from system, discovering Chromecasts and sending audio to it. Audio codec is in your hands.

This version is in (_kind of_) beta, so any help is welcome.

Installation
----------------
This library currently works on OS X and Linux and Windows.
```
$ npm install chromecaster-lib
```

It is not needed to have installed portaudio to compile the library. But you must provide the library either by installing it using the aproppiate package manager (on Linux and macOS) or providing the library manually (on Windows and macOS).

For electron 2.0.10 and 3.0.2, node 8 and 10, it will download a compiled version from Github. Uses the library portaudio. To execute, the library can be put on an accessible path or can be put anywhere and load it using `AudioInput.loadNativeLibrary(path: string)`.

**Mac Users**: You should install portaudio using `brew install portaudio` if you have troubles.

**Linux Users**: You should install `portaudio19-dev` (or equivalent) package before installing this one.

**Windows Users**:
 - If you have problems compiling this library (or one of its dependencies), [see this issue](https://github.com/nodejs/node-gyp/issues/972).
 - You can compile [portaudio](http://portaudio.com) for 64bit or search a `.dll` on the internet. **(optionally)** Follow the instructions in `build/msvc/readme.txt` for compiling. Use release version. To test/use the library, copy the `portaudio_x64.dll` into the root of the project.

**Compiling note**: If you have any troubles for compiling, you can see these [requirements](https://github.com/nodejs/node-gyp#installation) for compiling in node. Really recommended for __Windows__ users.

Example
------------
A quick, example (_using lame encoder_):

```javascript
const { AudioInput, ChromecastDiscover, Webcast } = require('chromecaster-lib');
const lame = require('lame');

const cd = new ChromecastDiscover();
cd.on('device', function(ca) {
    cd.stop();
    const client = cd.createClient(ca);
    const encoder = new lame.Encoder({
        channels: 2,
        bitDepth: 16,
        sampleRate: 44100,
        bitRate: 320,
        outSampleRate: 44100,
        mode: lame.JOINTSTEREO
    });
    const audioInput = new AudioInput();
    const webcast = new Webcast({ port: 8080 });

    //Sometimes, pipe does not work well with the first one
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
## AudioInput
inherits from EventEmitter

**constructor([options])**
Creates the object passing some options. `options` object can contain the following fields, and its default values

- `samplerate` *Sample rate of the input audio stream. Valid values are: 44100, 48000, 88200, 96000* [44100]
- `bps` *Bitdepth for sample. Could be 8, 16, 24, 32* [16]
- `channels` *Number of channels of the stream, 1 (mono) or 2 (stereo)* [2]
- `deviceName` *name of the device which capture the audio* [system default]
- `timePerFrame` *number of milliseconds to capture per frame* [100ms]

 > **NOTE:** Invalid values in the above options will use the default value.

 > **NOTE:** For `deviceName`, try with any value from `AudioInput.getDevices()`.

 > **Observation:** If the native library is not loaded, the constructor will throw an Error.

**Number open()**
Opens the Input Audio Stream. If the return value is different from 0, then an error has occurred. In this case, see `AudioInput.Error`.

**close()**
Closes the Input Stream, in case it was opened.

**pause()**
(Un)Pauses the Input Stream.

**isOpen(): boolean**
Returns `true` if the stream is open, `false` otherwise.

**isPaused(): boolean**
Returns `true` if the stream is open and paused, or is closed.

**event 'data'**
Every processed frame, will be emitted on this event. Event has only one argument: the interleaved audio buffer.

### AudioInput.error(code: number): string
Converts the error returned in `Number AudioInput.open()` into a string.

### AudioInput.getDevices(): string[]
Returns the devices available in the system. Useful to change the input device
when creating an `AudioInput`.

### AudioInput.loadNativeLibrary(path: string): boolean
Tries to load the native library `portaudio` from the path given. If the library is already loaded or cannot be found, it will throw an Error.

### AudioInput.isNativeLibraryLoaded(): boolean
Returns `true` if the native library is loaded.

## Webcast
inherits from stream.Writable

**constructor([options])**
Creates a web server to send the input audio to the Chromecast (_or something else_), and opens the server. The options object and its default values:

- `port` *port to listen on* [3000]
- `contentType` *MIME type of the input stream* [audio/mp3]

**stop()**
Closes the server

**write(buffer: Buffer | string, encoding?: string, cbk?: () => void)**
Writes some bytes to the clients that are listening. Encoding is usually omitted.

**localIp: string**
Obtains the ip of the machine in the local network

**contentType: string**
Obtains the contentType of the input stream, that is, the stream that will output to the server.

**port: number**
Gets the port the server is listening on.

**event 'connect'**
When some client is connected to the local web server. The event passes (as object) these attributes:

 - id: _some kind of id for the client connected_ the position on the internal clients array
 - address: _the address of the device's endpoint_
 - port: _the port of the device's endpoint_
 - family: _IPv6 or IPv4_ (it's a string, see node's socket documentation)
 - headers: _object with the headers of the request_

**event 'disconnect'**
When a client closes the connexion, this event is emitted passing the same object as in `connect` event.

## ChromecastDiscover
inherits from events.EventEmitter

**constructor()**
Prepares the discovering of Chromecasts. No options required.

**start()**
Starts searching for Chromecasts.

**stop()**
Stops searching Chromecasts.

**getDeviceAddress(name: string): string | null**
Gets the IP address of the device named.

**getDeviceNameForNumber(pos: number): string**
Gets the device number for the nth device that was found. `number` goes from 0 to devicesFound - 1.

**forEachClient(cbk: (devName: string) => void)**
Does a forEach on every device found passing its name to the callback.

**createClient(nameOrDevice: string | ChromecastDeviceInfo): ChromecastClient**
Creates a ChromecastDevice object for the named device.

**event 'device'**
Event emitted when a device has been found. Argument is the name of the device.

## ChromecastDevice
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
