const { Transform } = require('stream')
// const crc16xmodem = require('crc/crc16xmodem')
const crc = require('./crc')
const { cobsDecode } = require('./cobs')


/**
 * A transform stream that emits data each time a frame with correct CRC is received.
 * @extends Transform
 * @example
const SerialPort = require('serialport')
const FrameParser = require('./frameparser')
const port = new SerialPort('/dev/tty-usbserial1')
const parser = port.pipe(new FrameParser({ }))
parser.on('data', console.log)
 */
class FrameParser extends Transform {
    constructor(opts) {
        super({ ...opts, readableObjectMode: true, writeableObjectMode: true })

        this.buffer = Buffer.alloc(0)
        this.frameRem = 0
        this.awaitLen = false
    }

    processFrame() {
        let cobsDec = cobsDecode(this.buffer)
        if (cobsDec.length >= 3) {
            let crcRem = crc.crc16xmodem(cobsDec)
            // CRC of data including the CRC yields a 0 remainder:
            if (crcRem == 0) {
                let channelId = cobsDec[0]
                this.push({ data: cobsDec.slice(1, cobsDec.length - 2), channelId: channelId })
            }
        }
    }

    _transform(chunk, encoding, cb) {
        let position = 0
        // console.log('chunk', chunk.length)

        while (position < chunk.length) {
            if (this.awaitLen) {
                this.frameRem = chunk.readUint8(position)
                if (this.frameRem != 0) {
                    // console.log('new len', this.frameRem)
                    this.awaitLen = false
                }
                position += 1
                continue
            }
            let zeroSpanEnd = this.frameRem > 0 ? position + this.frameRem : chunk.length
            let zeroSpan = chunk.slice(position, zeroSpanEnd)
            let zeroSpanPos = zeroSpan.indexOf(0)
            if (zeroSpanPos !== -1) {
                this.awaitLen = true
                this.buffer = Buffer.alloc(0)
                position += zeroSpanPos + 1
                continue
            }
            if (this.frameRem > 0) {
                let nextLen = Math.min(this.frameRem, chunk.length - position)
                let nextSlice = chunk.slice(position, position + nextLen)
                this.buffer = Buffer.concat([this.buffer, nextSlice])
                this.frameRem -= nextLen
                position += nextLen
                // console.log('next, buf', this.buffer, this.frameRem)
                if (this.frameRem === 0) {
                    this.processFrame()
                    this.frameRem = 0
                    this.awaitLen = false
                    this.buffer = Buffer.alloc(0)
                }
            }
        }

        cb()
    }

    _flush(cb) {
        this.push(this.buffer)
        this.buffer = Buffer.alloc(0)
        this.frameRem = 0
        this.awaitLen = false
        cb()
    }
}

module.exports = FrameParser
