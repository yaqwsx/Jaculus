import { Command } from "commander"

const program = new Command();

program
    .option("-a, --add", "Add smiley face")
    .action(() => console.log(`Hi!${program.opts().add ? ":)" : ""}`))
    .description("Say hello")

program.parse();
