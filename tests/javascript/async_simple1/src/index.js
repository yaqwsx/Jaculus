function delay(time) {
    return new Promise(function (resolve, reject) {
        createTimer(time, true, function() { resolve(time); });
    });
};

async function main() {
    while (true) {
        console.log("ON");
        await delay(1000);
        console.log("OFF");
        await delay(1000);
    }
}

main();
