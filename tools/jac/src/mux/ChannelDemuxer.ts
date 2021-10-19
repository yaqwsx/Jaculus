import { PassThrough, Readable, Writable } from "stream"
import { ReadableOptions, WritableOptions } from "stream"
import { Buffer } from "buffer"

class ChannelDemuxer extends Writable {
    private channels = new Map<number, Writable>()
    constructor(opts?: WritableOptions) {
        super({ ...opts, objectMode: true })
    }

    public pipeChannel(channel: Writable, channelId: number): Writable {
        this.channels.set(channelId, channel)
        return channel
    }

    _write(chunk: { data: Buffer, channelId: number }, encoding: BufferEncoding, callback: (error?: Error | null) => void): void {
        let channel = this.channels.get(chunk.channelId)
        if (channel === undefined) {
            console.warn("Demux channel %d unassigned", chunk.channelId)
        }
        else if (!channel.write(chunk.data, encoding, callback)) {
            console.info("Demux channel %d full", chunk.channelId)
        }

        // callback()
    }
}

export { ChannelDemuxer }
