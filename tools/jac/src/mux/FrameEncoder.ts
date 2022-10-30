import { Transform } from "stream"
import crc from "crc"
import { cobsEncode } from "./cobs.js"
import { Buffer } from "buffer"

import type { TransformOptions, TransformCallback } from "stream"

const packetDataMaxLen = 250
const frameMaxLen = packetDataMaxLen + 7
class FrameEncoder extends Transform {
    constructor(opts?: TransformOptions) {
        super({ ...opts, writableObjectMode: true })
    }

    _transform(chunk: { data: Buffer, channelId: number }, encoding: BufferEncoding, callback: TransformCallback): void {        
        let chunkInd = 0
        // console.log('Wchunk', chunk.data.length)
        while (chunkInd < chunk.data.length) {
            let packetDataLen = Math.min(chunk.data.length - chunkInd, packetDataMaxLen)
            let packet = Buffer.alloc(2 + packetDataLen + 2)

            // Report fully available RxWindow as we have infinite memory
            packet[0] = 0x0F
            // Prepend Channel ID to packet
            packet[1] = chunk.channelId
            chunk.data.copy(packet, 2, chunkInd, chunkInd + packetDataLen)

            // Append CRC to packet
            let crcValue = crc.crc16xmodem(packet.slice(0, packet.length - 2))
            packet.writeUInt16BE(crcValue, packet.length - 2)
            
            let frameBuf = Buffer.alloc(frameMaxLen)
            
            let cobsLen = cobsEncode(packet, frameBuf.slice(2, frameBuf.length))
            frameBuf[0] = 0
            frameBuf[1] = cobsLen
            let frame = frameBuf.slice(0, cobsLen + 2)
            // console.log(frame.toString())
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
