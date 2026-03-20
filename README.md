
# Requisitos

```bash
sudo dnf install -y \
    gstreamer1-plugins-base \
    gstreamer1-plugins-good \
    gstreamer1-plugins-bad-free \
    gstreamer1-plugins-bad-free-extras \
    libnice \
    libnice-gstreamer1
```

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