
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

# Empaquetar como RPM

Asegúrate de tener `rpmbuild` instalado:

```bash
sudo dnf install -y rpm-build rsync
```

Luego ejecuta:

```bash
./packaging/build-rpm.sh
```

El paquete `.rpm` quedará en `~/rpmbuild/RPMS/`. Para instalarlo:

```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/skreenapp-0.1.0-1.*.rpm
```

# Conexión USB

Conectar el celular por USB con USB debugging activado y ejecutar:

```bash
adb reverse tcp:9002 tcp:9002
```

Luego abrir la app en el celular y pulsar **Conectar**.

> El servidor Node.js ya no es necesario.
