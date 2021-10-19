

function setTimeout(callback, timeoutMs) {
    if(arguments.length > 2) {
        var argsArray = Array.prototype.slice.call(arguments);
        argsArray.splice(0, 2);
        return createTimer(timeoutMs, true, function() {
            callback.apply(null, argsArray);
        });
    } else {
        return createTimer(timeoutMs, true, callback);
    }
}

function clearTimeout(timerId) {
    deleteTimer(timerId);
}

function setInterval(callback, timeoutMs) {
    if(arguments.length > 2) {
        var argsArray = Array.prototype.slice.call(arguments);
        argsArray.splice(0, 2);
        return createTimer(timeoutMs, false, function() {
            callback.apply(null, argsArray);
        });
    } else {
        return createTimer(timeoutMs, false, callback);
    }
}

function clearInterval(timerId) {
    deleteTimer(timerId);
}

function delay(delayMs) {
    return new Promise(function(resolve) {
        setTimeout(resolve, delayMs);
    })
}
