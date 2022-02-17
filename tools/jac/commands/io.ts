import { InvalidArgumentError } from "commander"
import SerialPort from "serialport"
import { Readable, Writable, PassThrough } from "stream"
import { FrameEncoder, FrameParser } from "../src/mux/index.js"

async function createJacChannelIO(portName: string): Promise<[Readable, Writable]> {
    const port = new SerialPort(portName, {
        baudRate: 921600,
        autoOpen: false,
        hupcl: false
    } as any )

    const parser = port.pipe(new FrameParser)
    const encoder = new FrameEncoder
    encoder.pipe(port as Writable)
    
    await new Promise((resolve, reject) => {
        port.open(err => err === undefined ? reject() : resolve(0))
    })
    return [parser, encoder]
}

async function selectJacPortName(port: string|undefined): Promise<string> {
    if (port !== undefined) {
        return port
    }
    let portInfos = await SerialPort.list()
    if (portInfos.length == 1) {
        return portInfos[0].path
    }
    throw new InvalidArgumentError("Multiple/no serial ports found, select one")
}

export { createJacChannelIO, selectJacPortName }
