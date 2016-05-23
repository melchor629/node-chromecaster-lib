'use strict';

//jshint esversion: 6
var AudioInputNative = require('bindings')('AudioInputNative');
var events = require('events');
var util = require('util');

util.inherits(AudioInputNative.AudioInput, events.EventEmitter);
AudioInputNative.AudioInput.Error = AudioInputNative.AudioInputError;
AudioInputNative.AudioInput.getDevices = AudioInputNative.GetDevices;

module.exports = AudioInputNative.AudioInput;