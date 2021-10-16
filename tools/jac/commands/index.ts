import { Command } from "commander"

const program = new Command();

program
    .description("whatever");

program
    .command("hello", "Say hello", {executableFile: "dist/commands/hello.js"})
    .command("hi", "Say hi", {executableFile: "dist/commands/hi.js"})

program.parse();
