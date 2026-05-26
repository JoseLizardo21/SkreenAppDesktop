#!/bin/bash
# Instala la acción polkit y el helper para gestión de firewall.
# Ejecutar una sola vez después de compilar la app.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HELPER_SRC="$SCRIPT_DIR/skreenapp-firewall-helper"
POLICY_SRC="$SCRIPT_DIR/../polkit/com.skreenapp.firewall.policy"

HELPER_DEST="/usr/local/lib/skreenapp/skreenapp-firewall-helper"
POLICY_DEST="/usr/share/polkit-1/actions/com.skreenapp.firewall.policy"

echo "Installing SkreenApp polkit action..."
mkdir -p /usr/local/lib/skreenapp
install -m 0755 "$HELPER_SRC" "$HELPER_DEST"
install -m 0644 "$POLICY_SRC" "$POLICY_DEST"
echo "Installed successfully."
echo "  Helper: $HELPER_DEST"
echo "  Policy: $POLICY_DEST"
