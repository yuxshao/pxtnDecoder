import Observable from "zen-observable";

import Memory from "./memory";
import textDecoder from "./textDecoder";
import waitUntilIdle from "./waitUntilIdle";

// emscripten import
import { ENVIRONMENT, buffer, _free, decodeNoise, createPxtone, releasePxtone, getPxtoneText, getPxtoneInfo, vomitPxtone } from "./emDecoder";

// constant
const TEMP_BUFFER_SIZE = 4096;
const HEAPU8 = new Uint8Array(buffer);

// main function
// type: noise | pxtone | stream
// inputBuffer: the input project/noise/tune file
// ch: # channels
// sps: samples per second?
// bps: bits per sample I think
async function decode(type, inputBuffer, ch, sps, bps) {
    // input buffer 
    const inputSize = inputBuffer.byteLength;

    const inputBufferMem = new Memory(inputSize);
    HEAPU8.set(new Uint8Array(inputBuffer), inputBufferMem.ptr);

    // output
    let outputBuffer = null, outputStream = null, data = null;

    switch(type) {
        case "noise": {
            const outputMem = new Memory("*"), outputSizeMem = new Memory("i32");

            const release = () => {
                outputMem.release();
                outputSizeMem.release();
            };

            await waitUntilIdle();

            if(!decodeNoise(
                inputBufferMem.ptr, inputSize, ch, sps, bps,
                outputMem.ptr, outputSizeMem.ptr
            )) {
                release();
                throw new Error("Decode Pxtone Noise Error.");
            }

            const outputStart = outputMem.getValue(), outputEnd = outputStart + outputSizeMem.getValue();
            outputBuffer = buffer.slice(outputStart, outputEnd);

            _free(outputStart);
            release();
            break;
        }

        case "pxtone": 
        case "stream": {
            // pxVomitMem points to the pxVomit instance. doc for pxwrDoc (some pointer to a ptcop file?)
            // this is allocation
            const pxVomitMem = new Memory("*"), docMem = new Memory("*");

            // create
            if(!createPxtone(
                inputBufferMem.ptr, inputSize, ch, sps, bps,
                pxVomitMem.ptr, docMem.ptr
            )) {
                pxVomitMem.release();
                docMem.release();	
                throw new Error("Create Pxtone Vomit Error.");
            }

            const releaseVomit = () => {
                releasePxtone(pxVomitMem.ptr, docMem.ptr);
                pxVomitMem.release();
                docMem.release();
            };

            // text
            let title = "", comment = "";
            {
                const titleMem = new Memory("*"), titleSizeMem = new Memory("i32");
                const commentMem = new Memory("*"), commentSizeMem = new Memory("i32");

                const release = () => {
                    titleMem.release();
                    titleSizeMem.release();
                    commentMem.release();
                    commentSizeMem.release();
                };

                if(!getPxtoneText(
                    pxVomitMem.ptr, 
                    titleMem.ptr, titleSizeMem.ptr,
                    commentMem.ptr, commentSizeMem.ptr
                )) {
                    release();
                    releaseVomit();
                    throw new Error("Get Pxtone Vomit Text Error.");
                }

                const titleStart = titleMem.getValue(), commentStart = commentMem.getValue();

                if(titleStart) {
                    const titleEnd = titleStart + titleSizeMem.getValue();
                    const titleBuffer = buffer.slice(titleStart, titleEnd);
                    title = await textDecoder(titleBuffer);
                }

                if(commentStart) {
                    const commentEnd = commentStart + commentSizeMem.getValue();
                    const commentBuffer = buffer.slice(commentStart, commentEnd);
                    comment = await textDecoder(commentBuffer);
                }

                release();
            }

            // info
            let outputSize;
            {
                const outputSizeMem = new Memory("i32");
                const loopStartMem = new Memory("double"), loopEndMem = new Memory("double");

                const release = () => {
                    outputSizeMem.release();
                    loopStartMem.release();
                    loopEndMem.release();
                };

                if(!getPxtoneInfo(
                    pxVomitMem.ptr, ch, sps, bps,
                    outputSizeMem.ptr, loopStartMem.ptr, loopEndMem.ptr
                )) {
                    release();
                    releaseVomit();
                    throw new Error("Get Pxtone Vomit Info Error.");
                }

                outputSize = outputSizeMem.getValue();

                const loopStart = loopStartMem.getValue(), loopEnd = loopEndMem.getValue();

                data = {
                    "loopStart":    loopStart,
                    "loopEnd":      loopEnd,
                    "title":        title,
                    "comment":      comment,
                    "byteLength":   outputSize
                }

                release();
            }

            // vomit
            if(type === "pxtone") {

                // outputSize is essentially sample_num
                outputBuffer = new ArrayBuffer(outputSize);
                const outputArray = new Uint8Array(outputBuffer);

                const tempBufferMem = new Memory(TEMP_BUFFER_SIZE);
                const tempArray = HEAPU8.subarray(tempBufferMem.ptr, tempBufferMem.ptr + TEMP_BUFFER_SIZE);

                const release = () => {
                        tempBufferMem.release();
                };


                let deadline = await waitUntilIdle();
                for(let pc = 0; pc < outputSize; pc += TEMP_BUFFER_SIZE) {
                    const size = Math.min(TEMP_BUFFER_SIZE, outputSize - pc);

                    if(!vomitPxtone(pxVomitMem.ptr, tempBufferMem.ptr, size)) {
                        release();
                        releaseVomit();
                        throw new Error("Pxtone Vomit Error.");
                    }

                    // memcpy
                    outputArray.set(size === TEMP_BUFFER_SIZE ? tempArray : HEAPU8.subarray(tempBufferMem.ptr, tempBufferMem.ptr + size), pc);

                    if(!deadline || deadline && deadline.timeRemaining() === 0) deadline = await waitUntilIdle();
                }

                // release
                release();
                releaseVomit();

            } else if(type === "stream") {

                outputStream = new Observable(observer => {

                    let cancelFlag = false;
                    (async () => {
                        const tempBufferMem = new Memory(TEMP_BUFFER_SIZE);

                        const release = () => {
                            tempBufferMem.release();
                        };

                        let deadline;
                        for(let pc = 0; pc < outputSize; pc += TEMP_BUFFER_SIZE) {
                            const size = Math.min(TEMP_BUFFER_SIZE, outputSize - pc);

                            // request idle
                            if(!deadline || deadline && deadline.timeRemaining() === 0) deadline = await waitUntilIdle();

                            // cancel
                            if(cancelFlag) break;
                         
                            if(!vomitPxtone(pxVomitMem.ptr, tempBufferMem.ptr, size)) {
                                release();
                                releaseVomit();						
                                throw new Error("Pxtone Vomit Error.");
                            }

                            // yield
                            observer.next( buffer.slice(tempBufferMem.ptr, tempBufferMem.ptr + size) );
                        }

                        if(!cancelFlag)
                            observer.complete();

                        // release
                        release();
                        releaseVomit();
                    })();

                    return () => {
                        cancelFlag = true;
                    };
                });

            }
            break;
        }

        default:
            throw new TypeError(`type is invalid (${ type })`);
    }

    return {
        "buffer":   outputBuffer,
        "stream":   outputStream,
        "data":     data
    };
}

// export
if(ENVIRONMENT === "NODE") {
	module["exports"] = decode;
} else if(ENVIRONMENT === "WEB") {
	global["pxtnDecoder"] = decode;
} else if(ENVIRONMENT === "WORKER") {
    // e is a MessageEvent. import info is in data
	global["addEventListener"]("message", async function(e) {
		const data = e["data"];
		const type = data["type"];

        if(type !== "noise" && type !== "pxtone" && type !== "stream" && type !== "cancel")
            throw new TypeError(`type is invalid (${ type })`);
        
        if(type === "cancel")
            return;

		const sessionId = data["sessionId"];
		const { buffer, stream, data: retData } = await decode(type, data["buffer"], data["ch"], data["sps"], data["bps"]);

        // here the worker is responding to the main thread
        global["postMessage"]({
            "sessionId":	sessionId,
            "buffer":		buffer,
            "data":			retData
        }, stream ? [] : [buffer]);

        // stream
        if(stream) {

            // handle cancel event
            const cancel = (e) => {
                const data = e["data"];
                if(data["type"] === "cancel" && data["sessionId"] === sessionId) {
                    subscription["unsubscribe"]();
                    global["removeEventListener"]("message", cancel);
                }
            };
            global["addEventListener"]("message", cancel);

            // subscribe to the stream Observable
            // when the next thing is filled, post a msg to the main thread forwarding the next thing
            // send a finish msg when you're done and remove the cancel
            // handler. this happens when cancel is called presumably, since the
            // flag that enables complete to be called only can be set from
            // outside.
            const subscription = stream.subscribe({
                next(streamBuffer) {
                    global["postMessage"]({
                        "sessionId":    sessionId,
                        "streamBuffer": streamBuffer,
                        "done":         false
                    });
                },
                complete() {
                    global["postMessage"]({
                        "sessionId":    sessionId,
                        "streamBuffer": null,
                        "done":         true
                    });
                    global["removeEventListener"]("message", cancel);                    
                }
            });

        }
        
	});
}
