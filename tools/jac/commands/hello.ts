import { Command } from "commander"

const program = new Command();

program
    .option("-f, --force", "force")
    .action(()=> console.log(program.opts().force ? "Fine... Hello!" : "I don't want to!"))
    .description("Say Hello")

program.parse();
