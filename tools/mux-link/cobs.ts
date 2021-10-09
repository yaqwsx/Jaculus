import { Buffer } from "buffer"

function cobsDecode(buf: Buffer): Buffer {
    const maxDest = (buf.length == 0) ? 0 : (buf.length - 1)
    let dest = Buffer.alloc(maxDest)
    let destInd = 0

    for (let i = 0; i < buf.length;) {
        let code = buf[i++]
        for (let j = 1; j < code; j++) {
            dest[destInd++] = buf[i++]
        }
        if (code < 0xFF && i < buf.length) {
            dest[destInd++] = buf[i++]
        }
    }
    return destInd == maxDest ? dest : dest.slice(0, destInd + 1)
}

export { cobsDecode };
