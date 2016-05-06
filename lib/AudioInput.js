//jshint esversion: 6
const AudioInputNative = require('bindings')('AudioInputNative');
const events = require('events');
const util = require('util');

util.inherits(AudioInputNative.AudioInput, events.EventEmitter);
AudioInputNative.AudioInput.Error = AudioInputNative.AudioInputError;

module.exports = AudioInputNative.AudioInput;
