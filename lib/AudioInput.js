//jshint esversion: 6
const AudioInputNative = require('bindings')('AudioInputNative');
const events = require('events');
const util = require('util');

util.inherits(AudioInputNative.AudioInput, events.EventEmitter);
AudioInputNative.AudioInput.error = AudioInputNative.AudioInputError;
AudioInputNative.AudioInput.getDevices = AudioInputNative.GetDevices;
AudioInputNative.AudioInput.loadNativeLibrary = AudioInputNative.loadPortaudioLibrary;

module.exports = AudioInputNative.AudioInput;
