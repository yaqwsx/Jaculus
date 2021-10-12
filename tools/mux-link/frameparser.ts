import { Transform } from "stream"
import { crc16xmodem } from "crc"
import { cobsDecode } from "./cobs"
import { Buffer } from "buffer"

import type { TransformOptions, TransformCallback } from "stream"
// import type { BufferEncoding } from "buffer"
/**
 * A transform stream that emits data each time a frame with correct CRC is received.
 * @extends Transform
 * @example
const SerialPort = require("serialport")
const FrameParser = require("./frameparser")
const port = new SerialPort("/dev/tty-usbserial1")
const parser = port.pipe(new FrameParser({ }))
parser.on("data", console.log)
 */
class FrameParser extends Transform {
    private _buffer: Buffer;
    private _frameRem: number;
    private _awaitLen: boolean;

    constructor(opts?: TransformOptions) {
        super({ ...opts, readableObjectMode: true, writableObjectMode: true })

        this._buffer = Buffer.alloc(0)
        this._frameRem = 0
        this._awaitLen = false
    }

    processFrame() {
        let cobsDec = cobsDecode(this._buffer)
        if (cobsDec.length >= 3) {
            let crcRem = crc16xmodem(cobsDec)
            // CRC of data including the CRC yields a 0 remainder:
            if (crcRem == 0) {
                let channelId = cobsDec[0]
                this.push({ data: cobsDec.slice(1, cobsDec.length - 2), channelId: channelId })
            }
        }
    }

    _transform(chunk: any, encoding: BufferEncoding, callback: TransformCallback): void {
        let position = 0
        // console.log("chunk", chunk.length)

        while (position < chunk.length) {
            if (this._awaitLen) {
                this._frameRem = chunk.readUint8(position)
                if (this._frameRem != 0) {
                    // console.log("new len", this._frameRem)
                    this._awaitLen = false
                }
                position += 1
                continue
            }
            let zeroSpanEnd = this._frameRem > 0 ? position + this._frameRem : chunk.length
            let zeroSpan = chunk.slice(position, zeroSpanEnd)
            let zeroSpanPos = zeroSpan.indexOf(0)
            if (zeroSpanPos !== -1) {
                this._awaitLen = true
                this._buffer = Buffer.alloc(0)
                position += zeroSpanPos + 1
                continue
            }
            if (this._frameRem > 0) {
                let nextLen = Math.min(this._frameRem, chunk.length - position)
                let nextSlice = chunk.slice(position, position + nextLen)
                this._buffer = Buffer.concat([this._buffer, nextSlice])
                this._frameRem -= nextLen
                position += nextLen
                // console.log("next, buf", this._buffer, this._frameRem)
                if (this._frameRem === 0) {
                    this.processFrame()
                    this._frameRem = 0
                    this._awaitLen = false
                    this._buffer = Buffer.alloc(0)
                }
            }
        }

        callback()
    }

    _flush(callback: TransformCallback): void {
        this.push(this._buffer)
        this._buffer = Buffer.alloc(0)
        this._frameRem = 0
        this._awaitLen = false
        callback()
    }
}

export { FrameParser }
