console.log("Evaluation of main");

nestedModule = require("./nested/nestedModule.js");
nestedModule.run();

rootModule = require("/test.js");
rootModule.greet();
