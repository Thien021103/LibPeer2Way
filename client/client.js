const iceConnectionLog = document.getElementById('ice-connection-state'),
    iceGatheringLog = document.getElementById('ice-gathering-state'),
    signalingLog = document.getElementById('signaling-state'),
    dataChannelLog = document.getElementById('data-channel');

const clientId = randomId(10);
const websocket = new WebSocket('ws://192.168.66.217:8000/' + clientId);

websocket.onopen = () => {
    document.getElementById('start').disabled = false;
}

websocket.onmessage = async (evt) => {
    if (typeof evt.data !== 'string') {
        return;
    }
    const message = JSON.parse(evt.data);
    if (message.type == "offer") {
        document.getElementById('offer-sdp').textContent = message.sdp;
        await handleOffer(message)
    }
}

let pc = null;
let datachannel = null;

 function createPeerConnection() {
    const config = {
        bundlePolicy: "max-bundle",
    };
    
    if (document.getElementById('use-stun').checked) {
        config.iceServers = [{urls: ['stun:stun.l.google.com:19302']},
        // config.iceServers = [
        //     {
        //         urls: 'turn:192.168.0.170:3478',
        //         username: 'client',
        //         credential: 'client123'
        //     }
        ]

    }
    let pc = new RTCPeerConnection(config);

    // Register some listeners to help debugging
    pc.addEventListener('iceconnectionstatechange', () =>
        iceConnectionLog.textContent += ' -> ' + pc.iceConnectionState);
    iceConnectionLog.textContent = pc.iceConnectionState;

    pc.addEventListener('icegatheringstatechange', () =>
        iceGatheringLog.textContent += ' -> ' + pc.iceGatheringState);
    iceGatheringLog.textContent = pc.iceGatheringState;

    pc.addEventListener('signalingstatechange', () =>
        signalingLog.textContent += ' -> ' + pc.signalingState);
    signalingLog.textContent = pc.signalingState;


    // Receive audio/video track
    pc.ontrack = (evt) => {
        document.getElementById('media').style.display = 'block';
        const video = document.getElementById('video');
        // always overrite the last stream - you may want to do something more clever in practice
        video.srcObject = evt.streams[0]; // The stream groups audio and video tracks
        video.play();
    };

    const datachannel = pc.createDataChannel('pear')
    // // Receive data channel
    pc.ondatachannel = () => {
        console.log('ondatachannel');
    }
      
    datachannel.onclose = () => console.log('datachannel has closed');
    datachannel.onopen = () => {
        console.log('datachannel has opened');
        console.log('sending ping');
        setInterval(() => {
            console.log('sending ping');
            datachannel.send('ping');
        }, 1000);
    }
      
    datachannel.onmessage = async e => {
      
        if (e.data instanceof Blob) { 
      
            const buffer = await e.data.arrayBuffer();

            var arrayBufferView = new Uint8Array(buffer);
            var blob = new Blob( [ arrayBufferView ], { type: "image/jpeg" } );
            var urlCreator = window.URL || window.webkitURL;
            var imageUrl = urlCreator.createObjectURL( blob );
            
            var imageElement = document.getElementById('imgStream');
            imageElement.src = imageUrl;
        }  
    }
    pc.onicecandidate = event => {
        
        setInterval(() => {
    
        pc.getStats(null).then((stats) => {
            let statsOutput = "";
    
            stats.forEach((report) => {
            statsOutput +=
                `<h2>Report: ${report.type}</h2>\n<strong>ID:</strong> ${report.id}<br>\n` +
                `<strong>Timestamp:</strong> ${report.timestamp}<br>\n`;
            // Now the statistics for this report; we intentionally drop the ones we
            // sorted to the top above
    
            Object.keys(report).forEach((statName) => {
                if (
                    statName !== "id" &&
                    statName !== "timestamp" &&
                    statName !== "type"
                ) {
                    statsOutput += `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
                }
            });
            });
    
            document.getElementById("stats-box").innerHTML = statsOutput;
        });
        }, 1000);
    
    }
      
      

    return pc;
}

async function waitGatheringComplete() {
    return new Promise((resolve) => {
        if (pc.iceGatheringState === 'complete') {
            resolve();
        } else {
            pc.addEventListener('icegatheringstatechange', () => {
                if (pc.iceGatheringState === 'complete') {
                    resolve();
                }
            });
        }
    });
}

async function sendAnswer(pc) {
    await pc.setLocalDescription(await pc.createAnswer());
    await waitGatheringComplete();

    const answer = pc.localDescription;
    document.getElementById('answer-sdp').textContent = answer.sdp;

    websocket.send(JSON.stringify({
        id: "server",
        type: answer.type,
        sdp: answer.sdp,
    }));
}

async function getMedia(pc) {
    // Capture audio from the microphone and add it to the PeerConnection
    try {
        const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
        stream.getTracks().forEach((track) => pc.addTrack(track, stream));
    } catch (error) {
        console.error('Error accessing microphone:', error);
    }
 }

async function handleOffer(offer) {
    pc = createPeerConnection();
    await getMedia(pc)
    await pc.setRemoteDescription(offer);
    await sendAnswer(pc);
}

function sendRequest() {
    websocket.send(JSON.stringify({
        id: "server",
        type: "request",
    }));
}

function start() {
    document.getElementById('start').style.display = 'none';
    document.getElementById('stop').style.display = 'inline-block';
    document.getElementById('media').style.display = 'block';
    sendRequest();
}

function stop() {
    document.getElementById('stop').style.display = 'none';
    document.getElementById('media').style.display = 'none';
    document.getElementById('start').style.display = 'inline-block';

    // close data channel
    if (datachannel) {
        datachannel.close();
        datachannel = null;
    }

    // close transceivers
    if (pc.getTransceivers) {
        pc.getTransceivers().forEach((transceiver) => {
            if (transceiver.stop) {
                transceiver.stop();
            }
        });
    }

    // close local audio/video
    pc.getSenders().forEach((sender) => {
        const track = sender.track;
        if (track !== null) {
            sender.track.stop();
        }
    });

    // close peer connection
    pc.close();
    pc = null;
}

// Helper function to generate a random ID
function randomId(length) {
  const characters = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
  const pickRandom = () => characters.charAt(Math.floor(Math.random() * characters.length));
  return [...Array(length) ].map(pickRandom).join('');
}

// Helper function to generate a timestamp
let startTime = null;
function currentTimestamp() {
    if (startTime === null) {
        startTime = Date.now();
        return 0;
    } else {
        return Date.now() - startTime;
    }
}
