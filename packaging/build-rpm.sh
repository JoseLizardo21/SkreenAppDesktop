#!/bin/bash
set -e

VERSION="0.1.0"
NAME="skreenapp"
PROJ_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARBALL_NAME="${NAME}-${VERSION}"

echo "==> Creating source tarball..."
TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

mkdir -p "$TMPDIR/$TARBALL_NAME"
rsync -a --exclude='build' --exclude='_build' --exclude='.git' \
    "$PROJ_DIR/" "$TMPDIR/$TARBALL_NAME/"

mkdir -p ~/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

tar -czf ~/rpmbuild/SOURCES/${TARBALL_NAME}.tar.gz \
    -C "$TMPDIR" "$TARBALL_NAME"

echo "==> Copying spec..."
cp "$PROJ_DIR/packaging/skreenapp.spec" ~/rpmbuild/SPECS/

echo "==> Building RPM..."
rpmbuild -ba ~/rpmbuild/SPECS/skreenapp.spec

echo ""
echo "Done! RPM located at:"
find ~/rpmbuild/RPMS -name "${NAME}-*.rpm"
