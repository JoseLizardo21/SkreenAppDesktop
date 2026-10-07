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

# Packaging and the driver

The package bundles the virtual monitor kernel driver from `../skreen_drive`
(override with `SKREEN_DRIVER_DIR=/path ./packaging/build-*.sh`). Its sources
are installed to `/usr/src/skreen-driver-<version>/`, built by DKMS for every
installed kernel and loaded on boot (`/usr/lib/modules-load.d/skreen-driver.conf`).

To ship a new driver, bump `skreen_drive/VERSION` and rebuild the packages.
On upgrade the new module is installed right away, but if the virtual
monitor is in use the old one keeps running until the next reboot. The app
compares `/sys/module/skreen_driver/version` with the version it was built
with and asks the user to reboot when they differ.

With Secure Boot enabled, DKMS signs the module with a local key that must be
enrolled once (`sudo mokutil --import /var/lib/dkms/mok.pub` and reboot).

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
