const SerialPort = require('serialport')
import { Stream } from "stream"
import { ChannelDemuxer } from "./ChannelDemuxer"
import { ChannelIdEnhancer } from "./ChannelIdEnhancer"
import { FrameEncoder } from "./FrameEncoder"
import { FrameParser } from './FrameParser'
import { StreamLogger } from "./StreamLogger"
const readline = require("readline")
const port = SerialPort('COM3', {
    baudRate: 921600,
    autoOpen: false,
    hupcl: false
})

const parser = port.pipe(new FrameParser())
const demuxer = parser.pipe(new ChannelDemuxer())
const uploaderInput = demuxer.pipeChannel(new StreamLogger('UPL'), 2)
const runtimeLogInput = demuxer.pipeChannel(new StreamLogger('LOG'), 3)

const output = new StreamLogger('W')
const idEnhancer = new ChannelIdEnhancer(2)
const encoder = new FrameEncoder()
output.pipe(idEnhancer).pipe(encoder).pipe(port)

// uploaderInput.on('data', function(chunk: Buffer) {
//     console.log('UP', chunk.toString())
// })
// 
// runtimeLogInput.on('data', function(chunk: Buffer) {
//     console.log('LOG', chunk.toString())
// })

port.open(function (err: any) {
    if (err) {
        return console.log('Error opening port: ', err.message)
    }
})

readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
}).on('line', function (line: string) {
    let buf = Buffer.from(line + '\n')
    output.write(buf)
})
