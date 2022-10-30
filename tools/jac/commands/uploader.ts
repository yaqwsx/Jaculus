import { ChannelDemuxer, ChannelIdEnhancer, StreamLogger } from "../src/mux/index.js"
import * as fs from "fs"
import { createJacChannelIO, selectJacPortName } from "./io.js"
import { assert } from "console"
import SerialPort from "serialport"
// import Readline from ''
import { once } from "events"

async function push(args: any) {
    assert(args.source as string)
    assert(args.target as string)
    let portName = await selectJacPortName(args.port)
    let [parser, encoder, port] = await createJacChannelIO(portName)

    const demuxer = parser.pipe(new ChannelDemuxer)
    const uploaderInput = demuxer.pipeChannel(new StreamLogger('UPL'), 2)
    const runtimeLogInput = demuxer
        .pipeChannel(new SerialPort.parsers.Readline({ delimiter: '\n' }), 3)
        .pipe(new StreamLogger('LOG'))

    const uploaderOutput = new ChannelIdEnhancer(2)
    uploaderOutput.pipe(encoder)
    
    let content = fs.readFileSync(args.source).toString("base64")
    let message = `\nPUSH ${args.target} ${content}\n`
    const CHUNK_SIZE = 250
    console.log("NOW")

    for (let i = 0; i < message.length; i += CHUNK_SIZE) {
        const chunk = message.slice(i, i + CHUNK_SIZE)
        await new Promise(resolve => setTimeout(resolve, 250))
        if (!uploaderOutput.write(chunk)) {
            await once(uploaderOutput, "drain")
            console.log("drain")
        }
        console.log(chunk)
    }
    await new Promise(resolve => setTimeout(resolve, 1000))
}

async function pull(args: any) {
    assert(args.source as string)
    assert(args.target as string)
    let portName = await selectJacPortName(args.port)
    let [parser, encoder] = await createJacChannelIO(portName)

    const demuxer = parser.pipe(new ChannelDemuxer)
    const uploaderInput = demuxer
        .pipeChannel(new StreamLogger('UPL'), 2)
    const runtimeLogInput = demuxer
        .pipeChannel(new StreamLogger('LOG'), 3)

    const uploaderOutput = new ChannelIdEnhancer(2)
    uploaderOutput.pipe(encoder)

    let target = fs.createWriteStream(args.target)
    uploaderInput
    .pipe(new SerialPort.parsers.Readline({ delimiter: "\n" }))
    .on('data', data => {
        let content = Buffer.from(data, "base64")
        target.write(content)
        target.close()
    })
    uploaderOutput.write(`PULL ${args.source}\n`)
    await once(target, 'close')
}

export { push, pull }