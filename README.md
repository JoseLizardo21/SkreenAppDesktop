
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

> `gstreamer1-vaapi` habilita encoding por hardware en Intel/AMD (vaapih264enc).
> En NVIDIA instala adicionalmente `gstreamer1-plugins-bad-free` con soporte NVENC.

# Ejecutar

**Servidor**

```bash
cd server
npm install
npm start
```

**App**

```bash
mkdir build && cd build
cmake ..
make
./skreen_desktop
```