var filename = __filename;

exports.greet = function() {
    console.log("Hello from module test.js");
    console.log("The module is located in ", filename);
}
