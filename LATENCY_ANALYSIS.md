# Análisis de Latencia — SkreenApp

## Pipeline completo

```
Pantalla → PipeWire (resolución nativa @30fps) → GStreamer (6 elementos) →
H.264 encode → h264parse → appsink → TCP raw (prefijo 4 bytes) → decode → pantalla
```

Cada etapa suma latencia. A 30fps ya tienes 33ms por frame de base.

---

## Resumen de latencias estimadas

| Etapa | Latencia estimada |
|-------|-------------------|
| Captura PipeWire → GStreamer | 8–16ms |
| Pipeline GStreamer (6 elementos) | 2–8ms |
| H.264 encoding (hardware, low-latency) | 5–10ms |
| h264parse (SPS/PPS inline) + appsink | 0.5–2ms |
| TCP raw (prefijo 4B) / USB cable | <1ms |
| H.264 decode | 5–15ms |
| Display (frame timing) | 0–33ms |
| **Total estimado** | **~25–100ms** |

---

## Latencia residual

La latencia restante (~25–100ms) es inherente a la captura y compresión de video.
Reducirla por debajo de ~20ms requeriría un codec especializado (WebRTC/RTP) o
transmisión sin comprimir, que a resolución nativa no es viable por USB.
