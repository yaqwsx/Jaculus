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

function cobsEncode(buf: Buffer, dest: Buffer): number {
    let maxDest = buf.length + ((buf.length) + 253)/254
    let length = buf.length
    // let dest = Buffer.alloc(maxDest)
	let destInd = 1
	let code = 1
	let codeInd = 0

	for (let bufInd = 0; length-- > 0; ++bufInd)
	{
		if (buf[bufInd] != 0) {
			dest[destInd++] = buf[bufInd]
            ++code
        }

		if (buf[bufInd] == 0 || code == 0xff) {
            // Input is zero or block completed, restart
			dest[codeInd] = code
            code = 1
            codeInd = destInd;
			if (buf[bufInd] == 0 || length > 0) {
                ++destInd
            }
		}
	}
	dest[codeInd] = code

	return destInd
}

export { cobsDecode, cobsEncode };
