// Web Audio playback of the streamed PCM (raw little-endian int16 mono @ 22050 Hz).
// Each binary WS frame is N*256 bytes; we schedule it as one AudioBuffer back to
// back with the previous chunk so playback is gap-free. AudioBuffers carry their
// own 22050 Hz sample rate, so the context resamples regardless of its own rate.

const SAMPLE_RATE = 22050
const LOOKAHEAD = 0.04   // s; small safety so scheduling never falls behind currentTime

export class PcmPlayer {
  private ctx: AudioContext | null = null
  private nextTime = 0
  enabled = false

  // Must be called from a user gesture (autoplay policy).
  start() {
    if (!this.ctx) this.ctx = new AudioContext()
    void this.ctx.resume()
    this.nextTime = this.ctx.currentTime + LOOKAHEAD
    this.enabled = true
  }

  stop() {
    this.enabled = false
    if (this.ctx) void this.ctx.suspend()
  }

  toggle() {
    if (this.enabled) this.stop()
    else this.start()
    return this.enabled
  }

  push(buf: ArrayBuffer) {
    if (!this.enabled || !this.ctx) return
    if (buf.byteLength < 2) return
    const i16 = new Int16Array(buf)
    const f32 = new Float32Array(i16.length)
    for (let i = 0; i < i16.length; i++) f32[i] = i16[i] / 32768
    const ab = this.ctx.createBuffer(1, f32.length, SAMPLE_RATE)
    ab.copyToChannel(f32, 0)
    const src = this.ctx.createBufferSource()
    src.buffer = ab
    src.connect(this.ctx.destination)
    const now = this.ctx.currentTime
    // Resync on underrun (tab was backgrounded, etc.) instead of piling up latency.
    if (this.nextTime < now + LOOKAHEAD) this.nextTime = now + LOOKAHEAD
    src.start(this.nextTime)
    this.nextTime += ab.duration
  }
}
