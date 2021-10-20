import SerialPort from "serialport"
import { PassThrough, Readable, Writable } from "stream"
import { ChannelDemuxer, ChannelIdEnhancer, FrameEncoder, FrameParser, StreamLogger } from "../src/mux/index.js"
import * as base64 from "base-64"
import * as fs from "fs"

function setupIO(portName: string): [Readable, Writable] {
    const port = new SerialPort(portName, {
        baudRate: 921600,
        autoOpen: false,
        hupcl: false
    } as any )

    const parser = port.pipe(new FrameParser())
    const demuxer = parser.pipe(new ChannelDemuxer())
    const input = demuxer.pipeChannel(new StreamLogger('UPL'), 2).pipe(new PassThrough())
    const runtimeLogInput = demuxer.pipeChannel(new StreamLogger('LOG'), 3)

    const output = new StreamLogger('W')
    const idEnhancer = new ChannelIdEnhancer(2)
    const encoder = new FrameEncoder()
    output.pipe(idEnhancer).pipe(encoder).pipe(port as any)
    return [input, output]
}

async function push(args: any) {
    console.log(args)
    let [input, output] = setupIO(args.port) 
        
    let content = fs.readFileSync(args.source).toString("base64")
    let message = `PUSH ${args.target} ${content}\n`
    const CHUNK_SIZE = 1024
    for (let i = 0; i < message.length; i += CHUNK_SIZE) {
        output.write(message.slice(i, i + CHUNK_SIZE))
        await new Promise(resolve => setTimeout(resolve, 100));
    }
    console.log("Fuck thiis w're done")
    // print(input.read())
    // print(exitUploader(s))
}

function pull(...args: any[]) {

}

export { push, pull }