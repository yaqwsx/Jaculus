import SerialPort from "serialport"
import { Readable, Writable, PassThrough } from "stream"
import { FrameEncoder, FrameParser } from "./mux/index.js"

async function createJacChannelIO(portName: string): Promise<[Readable, Writable]> {
    const port = new SerialPort(portName, {
        baudRate: 921600,
        autoOpen: false,
        hupcl: false
    } as any )

    const parser = port.pipe(new FrameParser)
    const encoder = new FrameEncoder
    encoder.pipe(port as Writable)
    
    await new Promise((resolve, reject) =>
        port.open(err => err === undefined ? reject() : resolve(0))
    )
    return [parser, encoder]
}

export { createJacChannelIO }