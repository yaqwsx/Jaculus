const gpio = require("gpio");

var outputPin = new gpio.Gpio(5);
outputPin.setMode("output");

var inputPin = new gpio.Gpio(16);
inputPin.setMode("input");
inputPin.onChange(function(pin, level) {
    console.log(pin, level);
});
