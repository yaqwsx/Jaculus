
import { createJacChannelIO, selectJacPortName } from "./io.js"
import ReadLine from "readline"
import { once } from "events"
import { ChannelDemuxer, ChannelIdEnhancer, StreamLogger } from "../src/mux/index.js"
import { PassThrough } from "stream"

async function monitor(args: any) {
    let portName = await selectJacPortName(args.port)
    let [parser, encoder] = await createJacChannelIO(portName)

    const demuxer = parser.pipe(new ChannelDemuxer)
    const uploaderInput = demuxer.pipeChannel(new StreamLogger('UPL'), 2)
    const runtimeLogInput = demuxer.pipeChannel(new StreamLogger('LOG'), 3)

    const uploaderOutput = new ChannelIdEnhancer(2)
    uploaderOutput.pipe(encoder)
    
    let rl = ReadLine.createInterface({
        input: process.stdin,
        output: process.stdout,
        terminal: false
    });
    rl.on('line', (line) => {
        uploaderOutput.write(line + "\n")
    });
    await once(rl, 'close');
}

export { monitor }
