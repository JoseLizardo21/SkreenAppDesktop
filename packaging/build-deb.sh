#!/bin/bash
set -e

echo "==> Installing build dependencies..."
sudo apt install -y \
    cmake \
    g++ \
    libgtk-3-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libdbus-1-dev \
    git

VERSION="0.1.0"
NAME="skreenapp"
ARCH=$(dpkg --print-architecture)
PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PKG_DIR=$(mktemp -d)

trap "rm -rf $PKG_DIR" EXIT

echo "==> Building project..."
BUILD_DIR=$(mktemp -d)
trap "rm -rf $BUILD_DIR $PKG_DIR" EXIT

cmake -S "$PROJ_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" -- -j"$(nproc)"

echo "==> Assembling package tree..."
install -Dm755 "$BUILD_DIR/skreen_desktop" \
    "$PKG_DIR/usr/bin/skreen_desktop"

install -Dm644 "$PROJ_DIR/src/assets/logo-removebg-preview.png" \
    "$PKG_DIR/usr/share/pixmaps/skreenapp.png"

install -Dm644 "$PROJ_DIR/packaging/skreenapp.desktop" \
    "$PKG_DIR/usr/share/applications/skreenapp.desktop"

install -Dm644 "$PROJ_DIR/polkit/com.skreenapp.firewall.policy" \
    "$PKG_DIR/usr/share/polkit-1/actions/com.skreenapp.firewall.policy"

install -Dm755 "$PROJ_DIR/scripts/skreenapp-firewall-helper" \
    "$PKG_DIR/usr/local/lib/skreenapp/skreenapp-firewall-helper"

echo "==> Creating DEBIAN metadata..."
mkdir -p "$PKG_DIR/DEBIAN"

cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: $NAME
Version: $VERSION
Architecture: $ARCH
Maintainer: Lizardo <jose.212002lizardo@gmail.com>
Depends: libgtk-3-0, libgstreamer1.0-0, libgstreamer-plugins-base1.0-0, firewalld, polkitd, adb
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

echo "==> Building .deb..."
OUT_DIR="$PROJ_DIR/packaging"
DEB_FILE="$OUT_DIR/${NAME}_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$PKG_DIR" "$DEB_FILE"

echo ""
echo "Done! Package located at:"
echo "  $DEB_FILE"
