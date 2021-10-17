// const log = require('why-is-node-running')
const SerialPort = require('serialport')
// import { cobsEncode } from "./cobs"
import { FrameEncoder } from "./FrameEncoder"
const { FrameParser } = require('./FrameParser')
const readline = require("readline")
const port = SerialPort('COM3', {
    baudRate: 921600,
    autoOpen: false,
    hupcl: false
})

const parser = port.pipe(new FrameParser({}))
const encoder = new FrameEncoder()
encoder.pipe(port)

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

parser.on('data', function(chunk: { data: Buffer, channelId: number }) {
    console.log('Read', chunk.channelId, chunk.data.toString())
})

port.open(function (err: any) {
    if (err) {
        return console.log('Error opening port: ', err.message)
    }
})

rl.on('line', function (line: string) {
    let buf = Buffer.from(line + '\n')
    console.log("Write", buf.toString())
    encoder.write({ data: buf, channelId: 1 })
})

setTimeout(function () {
    setTimeout(function () {
        // log() // logs out active handles that are keeping node running
    }, 500)
    
    //port.write(Buffer.from("0003FF1122", "hex"))
    
    // rl.question("", function (bla: string) {
    // });

}, 1000);
