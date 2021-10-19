import { Transform } from "stream"
import { Buffer } from "buffer"

import type { TransformOptions, TransformCallback } from "stream"

class StreamLogger extends Transform {
    private prefix
    constructor(prefix: string, opts?: TransformOptions) {
        super({ ...opts})
        this.prefix = prefix
    }

    _transform(chunk: any, encoding: BufferEncoding, callback: TransformCallback): void {        
        console.log(+Date.now() % 1000, this.prefix, chunk.toString())
        this.push(chunk)
        callback()
    }
}

export { StreamLogger }
