import { Transform } from "stream"
import { crc16xmodem } from "crc"
import { cobsEncode } from "./cobs"
import { Buffer } from "buffer"

import type { TransformOptions, TransformCallback } from "stream"

const packetDataMaxLen = 251
const frameMaxLen = 257
class FrameEncoder extends Transform {
    constructor(opts?: TransformOptions) {
        super({ ...opts, writableObjectMode: true })
    }

    _transform(chunk: { data: Buffer, channelId: number }, encoding: BufferEncoding, callback: TransformCallback): void {        
        let chunkInd = 0
        // console.log('Wchunk', chunk.data.length)
        while (chunkInd < chunk.data.length) {
            let packetDataLen = Math.min(chunk.data.length - chunkInd, packetDataMaxLen)
            let packet = Buffer.alloc(1 + packetDataLen + 2)

            // Prepend Channel ID to packet
            packet[0] = chunk.channelId
            chunk.data.copy(packet, 1, chunkInd, chunkInd + packetDataLen)

            // Append CRC to packet
            let crc = crc16xmodem(packet.slice(0, packet.length - 2))
            packet.writeUInt16BE(crc, packet.length - 2)
            
            let frameBuf = Buffer.alloc(frameMaxLen)
            
            let cobsLen = cobsEncode(packet, frameBuf.slice(2, frameBuf.length - 2))
            frameBuf[0] = 0
            frameBuf[1] = cobsLen
            let frame = frameBuf.slice(0, cobsLen + 2)
            // console.log(frame)
            this.push(frame)
            chunkInd += packetDataLen
        }
        callback()
    }

    _flush(callback: TransformCallback) {
        callback()
    }
}

export { FrameEncoder }
