#Cmake build
cmake -B build -S .
cmake --build build


# Requirements

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

# Run

**Desktop app**

```bash
mkdir build && cd build
cmake ..
make
./skreen_desktop
```

To enable the monitor on/off switch (disabled by default):

```bash
SKREEN_ACTIVE_MODULE_DRIVER=1 ./skreen_desktop
```

# Package as RPM

Make sure you have `rpmbuild` installed:

```bash
sudo dnf install -y rpm-build rsync
```

Then run:

```bash
./packaging/build-rpm.sh
```

The `.rpm` package will be placed in `~/rpmbuild/RPMS/`. To install it:

```bash
sudo dnf install ~/rpmbuild/RPMS/x86_64/skreenapp-0.1.0-1.*.rpm
```

To uninstall it:
```bash
sudo dnf remove skreenapp
```

# Package as DEB

Run:

```bash
./packaging/build-deb.sh
```

The `.deb` package will be placed in `packaging/`. To install it:

```bash
sudo apt install ./packaging/skreenapp_0.1.0_amd64.deb
```

To uninstall it:

```bash
sudo apt remove skreenapp
```

# USB connection

Connect the phone via USB with USB debugging enabled and run:

```bash
adb reverse tcp:9002 tcp:9002
```

Then open the app on the phone and tap **Connect**.

# Uninstalling

rpm -qa | grep skreen

sudo dnf remove skreenapp
