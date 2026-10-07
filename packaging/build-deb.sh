#!/bin/bash
set -e

echo "==> Installing build dependencies..."
sudo apt install -y \
    cmake \
    g++ \
    libgtk-3-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    libnice-dev \
    libdbus-1-dev \
    git

VERSION="0.1.0"
NAME="skreenapp"
ARCH=$(dpkg --print-architecture)
PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DRIVER_DIR="${SKREEN_DRIVER_DIR:-$PROJ_DIR/../skreen_drive}"
DRIVER_VERSION="$(cat "$DRIVER_DIR/VERSION")"
DKMS_NAME="skreen-driver"
PKG_DIR=$(mktemp -d)
DRV_PKG_DIR=$(mktemp -d)

trap "rm -rf $PKG_DIR $DRV_PKG_DIR" EXIT

echo "==> Building project..."
BUILD_DIR=$(mktemp -d)
trap "rm -rf $BUILD_DIR $PKG_DIR $DRV_PKG_DIR" EXIT

cmake -S "$PROJ_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSKREEN_DRIVER_DIR="$DRIVER_DIR"
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "==> Assembling package tree..."
install -Dm755 "$BUILD_DIR/skreen_desktop" \
    "$PKG_DIR/usr/bin/skreen_desktop"

install -Dm644 "$PROJ_DIR/src/assets/logo-removebg-preview.png" \
    "$PKG_DIR/usr/share/pixmaps/skreenapp.png"

install -Dm644 "$PROJ_DIR/packaging/skreenapp.desktop" \
    "$PKG_DIR/usr/share/applications/skreenapp.desktop"

echo "==> Creating DEBIAN metadata..."
mkdir -p "$PKG_DIR/DEBIAN"

cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: $NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: Lizardo <jose.212002lizardo@gmail.com>
Depends: libgtk-3-0, libgstreamer1.0-0, libgstreamer-plugins-base1.0-0, gstreamer1.0-plugins-ugly, gstreamer1.0-plugins-bad, gstreamer1.0-nice, gstreamer1.0-vaapi, adb, $DKMS_NAME-dkms (= $DRIVER_VERSION)
Description: Desktop app for screen streaming to mobile via SkreenApp
 SkreenApp Desktop allows you to stream your desktop screen to a mobile
 device using the SkreenApp mobile application.
EOF

cat > "$PKG_DIR/DEBIAN/postinst" <<'EOF'
#!/bin/sh
update-desktop-database /usr/share/applications 2>/dev/null || true
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

cat > "$PKG_DIR/DEBIAN/postrm" <<'EOF'
#!/bin/sh
update-desktop-database /usr/share/applications 2>/dev/null || true
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

echo "==> Assembling driver package ($DRIVER_VERSION)..."
DKMS_SRC="$DRV_PKG_DIR/usr/src/$DKMS_NAME-$DRIVER_VERSION"
mkdir -p "$DKMS_SRC"
for f in "$DRIVER_DIR"/*.c "$DRIVER_DIR"/*.h; do
    [ "$(basename "$f")" = monitor_control_ioctl.c ] && continue
    install -m644 "$f" "$DKMS_SRC/"
done
install -m644 "$DRIVER_DIR/Makefile" "$DRIVER_DIR/VERSION" "$DKMS_SRC/"
sed "s/#MODULE_VERSION#/$DRIVER_VERSION/" "$DRIVER_DIR/dkms.conf" > "$DKMS_SRC/dkms.conf"

install -Dm644 "$PROJ_DIR/packaging/skreen-driver.conf" \
    "$DRV_PKG_DIR/usr/lib/modules-load.d/skreen-driver.conf"

mkdir -p "$DRV_PKG_DIR/DEBIAN"

cat > "$DRV_PKG_DIR/DEBIAN/control" <<EOF
Package: $DKMS_NAME-dkms
Version: $DRIVER_VERSION
Architecture: all
Maintainer: Lizardo <jose.212002lizardo@gmail.com>
Depends: dkms
Recommends: linux-headers-generic | linux-headers-amd64
Description: Skreen virtual monitor kernel driver (DKMS)
 DRM kernel driver that exposes the virtual monitor used by SkreenApp
 Desktop. It is built with DKMS, so it is rebuilt on every kernel update.
EOF

# On upgrade dpkg runs the old prerm (removes the old version from DKMS)
# before the new postinst (adds, builds and installs the new one).
cat > "$DRV_PKG_DIR/DEBIAN/postinst" <<EOF
#!/bin/sh
set -e
if [ "\$1" = configure ]; then
    dkms add -m $DKMS_NAME -v $DRIVER_VERSION -q || true
    dkms build -m $DKMS_NAME -v $DRIVER_VERSION -q || true
    dkms install -m $DKMS_NAME -v $DRIVER_VERSION -q --force || true

    # Switch to the new module right away if nothing is using it. Otherwise
    # it is loaded on the next boot and the app asks the user to reboot.
    modprobe -r skreen_driver >/dev/null 2>&1 || true
    modprobe skreen_driver >/dev/null 2>&1 || true
fi
EOF
chmod 755 "$DRV_PKG_DIR/DEBIAN/postinst"

cat > "$DRV_PKG_DIR/DEBIAN/prerm" <<EOF
#!/bin/sh
set -e
case "\$1" in
    remove)
        modprobe -r skreen_driver >/dev/null 2>&1 || true
        dkms remove -m $DKMS_NAME -v $DRIVER_VERSION --all -q || true
        ;;
    upgrade|deconfigure|failed-upgrade)
        dkms remove -m $DKMS_NAME -v $DRIVER_VERSION --all -q || true
        ;;
esac
EOF
chmod 755 "$DRV_PKG_DIR/DEBIAN/prerm"

echo "==> Building .deb packages..."
OUT_DIR="$PROJ_DIR/packaging"
DEB_FILE="$OUT_DIR/${NAME}_${VERSION}_${ARCH}.deb"
DRV_DEB_FILE="$OUT_DIR/${DKMS_NAME}-dkms_${DRIVER_VERSION}_all.deb"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$DEB_FILE"
dpkg-deb --build --root-owner-group "$DRV_PKG_DIR" "$DRV_DEB_FILE"

echo ""
echo "Done! Packages located at:"
echo "  $DEB_FILE"
echo "  $DRV_DEB_FILE"
