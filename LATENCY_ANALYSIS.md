# Análisis de Latencia — SkreenApp

## Pipeline completo

```
Pantalla → PipeWire → GStreamer (10 elementos) → H.264 encode →
h264parse → appsink → TCP raw (prefijo 4 bytes) → decode → pantalla
```

Cada etapa suma latencia. A 30fps ya tienes 33ms por frame de base.

---

## Fuentes de latencia (de mayor a menor impacto)


### 2. h264parse + TCP raw con prefijo de longitud

El MPEG-TS muxer fue eliminado. El pipeline ahora usa `h264parse → appsink → TCP raw`.

En `GStreamerManager.cpp` (~línea 126):

```cpp
g_object_set(G_OBJECT(h264parse_), "config-interval", -1, NULL);
```

`config-interval=-1` inserta SPS/PPS inline antes de cada IDR (formato Annex-B), lo que
permite al receptor decodificar desde cualquier IDR sin estado previo. Agrega ~0.5–1ms
solo en frames IDR (cada ~167ms con `key-int-max=5`).

El transporte TCP envía cada frame H.264 con un prefijo de 4 bytes big-endian:

```cpp
uint8_t len_prefix[4] = { (size>>24)&0xFF, (size>>16)&0xFF,
                           (size>>8)&0xFF,   size&0xFF };
send(fd, len_prefix, 4, MSG_NOSIGNAL);
send(fd, data, size, MSG_NOSIGNAL);
```

`TCP_NODELAY` activo elimina el algoritmo de Nagle. Sin buffering de múltiples paquetes
(vs. `alignment=7` del muxer anterior). **Latencia estimada: ~0.5–2ms** (antes: 3–8ms).

---

### 3. Pipeline GStreamer — 7 elementos en serie

```
pipewiresrc → videorate → capsfilter → queue → videoscale →
capsfilter → videoconvert → encoder → h264parse → appsink
```

Cada elemento procesa en su propio hilo y pasa buffers entre sí. Aunque `queue_main` tiene
`max-size-buffers=1` y `leaky=2` (descarta frames viejos), la **cadena de procesamiento**
agrega ~5–15ms.

---

### 4. H.264 encoding

Incluso con hardware acceleration (`vah264enc`, `target-usage=7`) y `key-int-max=5`:

- Hardware encode: ~10–20ms
- El `cpb-size=500` limita bursts pero no elimina el tiempo de codificación

---

### 5. Protocolo HTTP sobre TCP

Aunque TCP sobre cable USB (adb reverse) tiene latencia de red ~0.5ms, el protocolo HTTP
agrega overhead de handshake inicial. El `Content-Type: video/mp2t` es correcto pero MPV
tarda en establecer el stream.

---

## Resumen de latencias estimadas

| Etapa | Latencia estimada |
|-------|-------------------|
| Captura PipeWire → GStreamer | 8–16ms |
| Pipeline GStreamer (7 elementos) | 5–15ms |
| H.264 encoding (hardware) | 10–20ms |
| h264parse (SPS/PPS inline) + appsink | 0.5–2ms |
| TCP raw (prefijo 4B) / USB cable | <1ms |
| H.264 decode | 5–15ms |
| Display (frame timing) | 0–33ms |
| **Total estimado** | **~50–200ms** |

---

## Por qué es estructuralmente difícil de resolver

El stack actual usa tecnología diseñada para **media streaming** (MPEG-TS, MPV, HTTP),
no para **screen mirroring de baja latencia**. Los protocolos como WebRTC o RTP están
optimizados para esto. MPEG-TS + MPV tiene buffers en múltiples capas que son difíciles
de eliminar completamente sin cambiar el enfoque.
