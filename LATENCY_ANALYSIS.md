# Análisis de Latencia — SkreenApp

## Pipeline completo

```
Pantalla → PipeWire → GStreamer (7 elementos) → H.264 encode →
MPEG-TS mux → appsink → TCP/HTTP → MPV demux → decode → pantalla
```

Cada etapa suma latencia. A 30fps ya tienes 33ms por frame de base.

---

## Fuentes de latencia (de mayor a menor impacto)

### 1. MPV en el receptor — el mayor culpable

En `SkreenAppMobile/lib/main.dart`:

```dart
// 64KB de buffer en el player
bufferSize: 64 * 1024
```

Y las propiedades MPV:

```
demuxer-lavf-probesize: 262144      # MPV espera 256KB antes de empezar a decodificar
demuxer-lavf-analyzeduration: 0.05  # 50ms de análisis
demuxer-readahead-secs: 0.1         # 100ms de readahead
stream-buffer-size: 4096            # buffer adicional
```

MPV está diseñado para **reproducción suave**, no para streaming de pantalla en tiempo real.
Aunque tiene `video-latency-hacks: yes`, su arquitectura interna introduce buffers inevitables.
El `probesize` de 256KB a 4Mbps significa que al inicio espera ~512ms solo para analizar el stream.

---

### 2. MPEG-TS muxer

En `GStreamerManager.cpp` (~línea 130):

```cpp
g_object_set(G_OBJECT(mpegtsmux_), "alignment", 7, NULL);
```

`alignment=7` agrupa **7 paquetes TS** antes de enviar. Cada paquete TS mide 188 bytes →
7×188 = 1316 bytes por envío. A 4Mbps eso es ~2.6ms de espera acumulada por buffer.
Además, MPEG-TS añade overhead de encapsulación que el receptor tiene que desmultiplexar.

---

### 3. Pipeline GStreamer — 7 elementos en serie

```
pipewiresrc → videorate → capsfilter → queue → videoscale →
capsfilter → videoconvert → encoder → h264parse → mpegtsmux → appsink
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
| MPEG-TS mux + appsink | 3–8ms |
| TCP/USB cable | <1ms |
| MPV probesize + analyzeduration | 50–500ms (inicio) / 50ms continuo |
| MPV readahead buffer | 100ms |
| H.264 decode | 5–15ms |
| Display (frame timing) | 0–33ms |
| **Total estimado** | **~200–750ms** |

---

## Por qué es estructuralmente difícil de resolver

El stack actual usa tecnología diseñada para **media streaming** (MPEG-TS, MPV, HTTP),
no para **screen mirroring de baja latencia**. Los protocolos como WebRTC o RTP están
optimizados para esto. MPEG-TS + MPV tiene buffers en múltiples capas que son difíciles
de eliminar completamente sin cambiar el enfoque.
