
# Requisitos

```bash
sudo dnf install -y \
    gstreamer1-devel \
    gstreamer1-plugins-base \
    gstreamer1-plugins-base-devel \
    gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free \
    gstreamer1-plugins-bad-free-extras \
    gstreamer1-plugins-bad-free-devel \
    gstreamer1-vaapi \
    libnice \
    libnice-devel \
    libnice-gstreamer1
```

# Ejecutar

**App desktop**

```bash
mkdir build && cd build
cmake ..
make
./skreen_desktop
```

# Conexión USB

Conectar el celular por USB con USB debugging activado y ejecutar:

```bash
adb reverse tcp:9002 tcp:9002
```

Luego abrir la app en el celular y pulsar **Conectar**.

> El servidor Node.js ya no es necesario.
