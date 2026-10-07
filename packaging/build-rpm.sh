#!/bin/bash
set -e

VERSION="0.1.0"
NAME="skreenapp"
PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DRIVER_DIR="${SKREEN_DRIVER_DIR:-$PROJ_DIR/../skreen_drive}"
DRIVER_VERSION="$(cat "$DRIVER_DIR/VERSION")"
TARBALL_NAME="${NAME}-${VERSION}"

echo "==> Creating source tarball..."
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

mkdir -p "$TMPDIR/$TARBALL_NAME"
rsync -a --exclude='build' --exclude='_build' --exclude='.git' \
    "$PROJ_DIR/" "$TMPDIR/$TARBALL_NAME/"

echo "==> Adding driver sources ($DRIVER_VERSION)..."
mkdir -p "$TMPDIR/$TARBALL_NAME/driver"
cp "$DRIVER_DIR"/*.c "$DRIVER_DIR"/*.h "$DRIVER_DIR/Makefile" \
    "$DRIVER_DIR/VERSION" "$DRIVER_DIR/dkms.conf" "$TMPDIR/$TARBALL_NAME/driver/"

mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

tar -czf ~/rpmbuild/SOURCES/${TARBALL_NAME}.tar.gz \
    -C "$TMPDIR" "$TARBALL_NAME"

echo "==> Copying spec..."
cp "$PROJ_DIR/packaging/skreenapp.spec" ~/rpmbuild/SPECS/

echo "==> Building RPM..."
rpmbuild -ba --define "driver_version $DRIVER_VERSION" ~/rpmbuild/SPECS/skreenapp.spec

echo ""
echo "Done! RPM located at:"
find ~/rpmbuild/RPMS -name "${NAME}-${VERSION}-*.rpm" -o -name "skreen-driver-dkms-${DRIVER_VERSION}-*.rpm"
