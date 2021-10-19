import { Transform } from "stream"

import type { TransformOptions, TransformCallback } from "stream"

class ChannelIdEnhancer extends Transform {
    private channelId
    constructor(channelId: number, opts?: TransformOptions) {
        super({ ...opts, readableObjectMode: true})
        this.channelId = channelId
    }

    _transform(chunk: any, encoding: BufferEncoding, callback: TransformCallback): void {        
        this.push({ data: chunk, channelId: this.channelId })
        callback()
    }
}

export { ChannelIdEnhancer }
