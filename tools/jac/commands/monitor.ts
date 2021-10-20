import SerialPort from "serialport"
import { PassThrough, Readable, Writable } from "stream"
import { ChannelDemuxer, ChannelIdEnhancer, FrameEncoder, FrameParser, StreamLogger } from "../src/mux/index.js"
import ReadLine from "readline"
import { once } from "events"

function setupIO(portName: string, inputChannel: number, outputChannel: number): [Readable, Writable] {
    const port = new SerialPort(portName, {
        baudRate: 921600,
        autoOpen: false,
        hupcl: false
    } as any )

    const parser = port.pipe(new FrameParser)
    const demuxer = parser.pipe(new ChannelDemuxer)
    const input = demuxer.pipeChannel(new StreamLogger('R'), inputChannel).pipe(new PassThrough)

    const output = new StreamLogger('W')
    const idEnhancer = new ChannelIdEnhancer(outputChannel)
    const encoder = new FrameEncoder()
    output.pipe(idEnhancer).pipe(encoder).pipe(port as any)
    return [input, output]
}

async function monitor(args: any) {
    console.log(args)
    let [input, output] = setupIO(args.port, args.inputChannel, args.outputChannel)
    
    let rl = ReadLine.createInterface({
        input: process.stdin,
        output: process.stdout,
        terminal: false
    });
    rl.on('line', (line) => {
        output.write(line + "\n")
    });
    await once(rl, 'close');
}


export { monitor }