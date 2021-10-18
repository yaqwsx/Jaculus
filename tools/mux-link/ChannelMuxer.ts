import { Transform } from "stream"

import type { TransformOptions, TransformCallback } from "stream"

class ChannelMuxer extends Transform {
    constructor(opts?: TransformOptions) {
        super({ ...opts})
    }

    _transform(chunk: any, encoding: BufferEncoding, callback: TransformCallback): void {
        console.log(chunk.toString())
        this.push(chunk)
        callback()
    }
}

export { ChannelMuxer }
