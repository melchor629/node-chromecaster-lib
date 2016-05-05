//jshint esversion: 6
const AudioInputNative = require('bindings')('AudioInputNative');
const events = require('events');
const util = require('util');

util.inherits(AudioInputNative.AudioInput, events.EventEmitter);

module.exports = AudioInputNative.AudioInput;
