Switch audio delivery from HTTP polling to WebSocket push in sim-web. Read `src/sim-web/sim-web.py` and `src/sim-web/index.html` before making changes. Work in `/home/johmagnu/learning/simple-stm32/sim`.

## Problem

Audio uses `GET /audio` HTTP polling every ~50ms. Display, UART, and trace already use WebSocket push — audio should too.

## Changes to sim-web.py

Add `GET /ws-audio` WebSocket endpoint. Update `audio_reader` thread to push to WebSocket clients:

```python
elif req.startswith('GET /ws-audio'):
    if self._ws_upgrade(conn, data):
        self._ws_audio_clients.append(conn)
        continue

# In audio_reader thread:
frame = self._ws_encode(data)
for c in self._ws_audio_clients:
    try: c.sendall(frame)
    except: dead.append(c)
```

Keep `GET /audio` HTTP endpoint as fallback for test scripts.

## Changes to index.html

Replace `pollAudio` fetch loop with WebSocket:

```javascript
const ws = new WebSocket(`ws://${location.host}/ws-audio`);
ws.binaryType = 'arraybuffer';
ws.onmessage = (evt) => {
    const samples = new Int16Array(evt.data);
    const buf = audioCtx.createBuffer(1, samples.length, 22050);
    const channel = buf.getChannelData(0);
    for (let i = 0; i < samples.length; i++)
        channel[i] = samples[i] / 32768.0;
    const source = audioCtx.createBufferSource();
    source.buffer = buf;
    source.connect(audioCtx.destination);
    const startTime = Math.max(audioNextTime, audioCtx.currentTime);
    source.start(startTime);
    audioNextTime = startTime + buf.duration;
};
```

## Testing

Verify audio plays. Check DevTools Network — should show `ws-audio` WebSocket, no periodic `GET /audio`.
