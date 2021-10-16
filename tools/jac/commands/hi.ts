import { Command } from "commander"

const program = new Command();

program
    .command("hi")
    .action(()=> console.log("hi!"))
    // .description('clone a repository into a newly created directory')
        // .action(() => {console.log("Hello World!")});

program.parse(process.argv);

const a = 100

export { a, program }