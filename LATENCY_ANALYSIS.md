# Análisis de Latencia — SkreenApp

## Pipeline completo

```
Pantalla → PipeWire → GStreamer (10 elementos) → H.264 encode →
h264parse → appsink → TCP raw (prefijo 4 bytes) → decode → pantalla
```

Cada etapa suma latencia. A 30fps ya tienes 33ms por frame de base.

---

## Fuentes de latencia (de mayor a menor impacto)


### 1. Pipeline GStreamer — 10 elementos en serie

```
pipewiresrc → videorate → capsfilter → queue → videoscale →
capsfilter → videoconvert → encoder → h264parse → appsink
```

Cada elemento procesa en su propio hilo y pasa buffers entre sí. Aunque `queue_main` tiene
`max-size-buffers=1` y `leaky=2` (descarta frames viejos), la **cadena de procesamiento**
agrega ~5–15ms.

---

### 2. H.264 encoding

Incluso con hardware acceleration (`vah264enc`, `target-usage=7`) y `key-int-max=5`:

- Hardware encode: ~10–20ms
- El `cpb-size=500` limita bursts pero no elimina el tiempo de codificación

---

## Resumen de latencias estimadas

| Etapa | Latencia estimada |
|-------|-------------------|
| Captura PipeWire → GStreamer | 8–16ms |
| Pipeline GStreamer (10 elementos) | 5–15ms |
| H.264 encoding (hardware) | 10–20ms |
| h264parse (SPS/PPS inline) + appsink | 0.5–2ms |
| TCP raw (prefijo 4B) / USB cable | <1ms |
| H.264 decode | 5–15ms |
| Display (frame timing) | 0–33ms |
| **Total estimado** | **~50–200ms** |

---

## Por qué es estructuralmente difícil de resolver

Las fuentes de latencia restantes (pipeline GStreamer y encoding H.264) son inherentes al
stack de captura y compresión de video. Reducir el encoding a <5ms requeriría un codec
diseñado para screen mirroring (como el modo de baja latencia de VP8/VP9 en WebRTC) o
transmisión de frames sin comprimir, lo cual no es viable a 4Mbps por USB.
