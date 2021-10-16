import { Command } from "commander"

const program = new Command();

program
    .description("whatever");

program
    .command("hello", "A", {executableFile: "dist/commands/hello.js"})
    .command("hi", "A", {executableFile: "dist/commands/hi.js"})
    // .description('clone a repository into a newly created directory')
        // .action(() => {console.log("Hello World!")});

program.parse(process.argv);

const a = 100

export { a }