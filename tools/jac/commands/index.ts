import { Command } from "commander"
import * as Uploader from "./uploader.js"
import * as Monitor from "./monitor.js"

const program = new Command();

program
    .description("whatever")
    .option("-p --port <string>", "Serial port name")
    .option("-s --source <string>", "File to push")
    .option("-t --target <string>", "Filename to push to")
    .option("-i --input-channel <number>", "")
    .option("-o --output-channel <number>", "")

program
    .command("push")
    .action(() => Uploader.push(program.opts()))

program
    .command("monitor")
    .action(() => Monitor.monitor(program.opts()))

program
    .command("pull").action(Uploader.pull)
    .command("hello", "Say hello", {executableFile: "dist/commands/hello.js"})
    .command("hi", "Say hi", {executableFile: "dist/commands/hi.js"})

program.parse();
