#!/usr/bin/env bash
# Load a packaged addon in headless Godot and check that its classes register.
#
#   scripts/release/smoke_test.sh dist/cesium_godot-linux-x86_64.tar.gz
#
# The engine is the checksum-pinned build havoc-canvas uses; override it with
# GODOT_VERSION and GODOT_SHA256. Godot's settings and caches go to a scratch
# directory, not the user's.
#
# The probe runs without the editor. Registering the extension through an editor
# import would also work, but Godot 4.6 crashes on shutdown when `--editor
# --quit` hot-loads this extension into an otherwise empty project. Canvas's own
# project imports cleanly, and Canvas's CI checks that import.
set -euo pipefail

[ $# = 1 ] || { echo "usage: $0 <addon archive>" >&2; exit 2; }
ARCHIVE=$(realpath "$1")
GODOT_VERSION=${GODOT_VERSION:-4.6-stable}
GODOT_SHA256=${GODOT_SHA256:-6bcc59dfd1d670e918c77eae06e82b9dc5699de13d353dc3a4b3b6b307b6dc06}
WORK=$(mktemp -d)
export XDG_CONFIG_HOME="$WORK/config" XDG_CACHE_HOME="$WORK/cache" XDG_DATA_HOME="$WORK/data"

if [ "$(id -u)" = 0 ] && command -v apt-get > /dev/null; then
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends ca-certificates curl unzip
fi

engine="Godot_v${GODOT_VERSION}_linux.x86_64"
curl -fsSL -o "$WORK/godot.zip" \
  "https://github.com/godotengine/godot/releases/download/$GODOT_VERSION/$engine.zip"
echo "$GODOT_SHA256  $WORK/godot.zip" | sha256sum -c -
unzip -q "$WORK/godot.zip" -d "$WORK"
GODOT="$WORK/$engine"

PROJECT="$WORK/project"
mkdir -p "$PROJECT/addons" "$PROJECT/.godot"
tar -xzf "$ARCHIVE" -C "$PROJECT/addons"
cat > "$PROJECT/project.godot" << 'EOF'
config_version=5

[application]

config/name="cesium_godot smoke test"
EOF
# What an editor import would write: Godot loads the listed extensions at startup.
echo "res://addons/cesium_godot/Godot3DTiles.gdextension" > "$PROJECT/.godot/extension_list.cfg"
cat > "$PROJECT/probe.gd" << 'EOF'
extends SceneTree

func _init() -> void:
	var missing := []
	for name in ["CesiumGeoreference", "Cesium3DTileset", "Cesium3DTile", "CesiumIonRasterOverlay"]:
		if not ClassDB.class_exists(name):
			missing.append(name)
	if missing.is_empty():
		print("cesium_godot classes registered")
		quit(0)
	else:
		printerr("cesium_godot classes missing: ", missing)
		quit(1)
EOF

"$GODOT" --headless --path "$PROJECT" --script res://probe.gd 2>&1 | tee "$WORK/probe.log"
if grep -E "ERROR:|SCRIPT ERROR:" "$WORK/probe.log"; then
  echo "error: Godot reported errors, see above" >&2
  exit 1
fi
grep -q "cesium_godot classes registered" "$WORK/probe.log"
