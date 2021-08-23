const gpio = require("gpio");

function delay(time) {
    return new Promise(function (resolve, reject) {
        createTimer(time, true, function() { resolve(time); });
    });
};

async function main() {
    var pin = new gpio.Gpio(5);
    pin.setMode("output");

    while (true) {
        pin.digitalWrite( false );
        await delay(1000);
        pin.digitalWrite( true );
        await delay(1000);
    }
}

main();
