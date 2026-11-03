#!/usr/bin/env bash
# Build the Linux x86_64 GDExtension and package the addon for a release.
#
# Run it on Debian bookworm, the base of havoc-canvas's runtime image. The
# library then needs at most glibc 2.36 and GCC 12's libstdc++, so it loads
# there and on any newer distribution. A build on a newer host does not: the
# host's glibc and libstdc++ symbol versions end up in the library.
#
# CI runs it in the release workflow's container. To run it locally:
#
#   docker run --rm -v "$PWD:/src:ro" -v "$PWD/dist:/dist" \
#     -v cesium-godot-ezvcpkg:/root/.ezvcpkg \
#     public.ecr.aws/docker/library/python:3.11-slim-bookworm@sha256:0a310eeecf4e1f5a0743f9a6520c90c88d089c903ca5fd283f501e3a805f5f89 \
#     /src/scripts/release/build_linux.sh
#
# It builds from a copy of the files git tracks in $SRC, so nothing from a host
# build (cesium_godot/native, godot-cpp, object files) gets in and $SRC is left
# untouched. It writes these to $OUT:
#
#   cesium_godot-linux-x86_64.tar.gz        the addon, with the stripped library
#   cesium_godot-linux-x86_64-debug.tar.gz  the library's debug info
#   SHA256SUMS
set -euo pipefail

SRC=${SRC:-/src}
OUT=${OUT:-/dist}
WORK=${WORK:-$(mktemp -d)}
JOBS=${JOBS:-$(nproc)}

# The commit whose headers are under cesium_godot/third_party/litehtml/include.
# The Linux archives committed beside them were built against glibc 2.38 (they
# call __isoc23_strtol), so they are rebuilt here.
LITEHTML_COMMIT=35ecd69d05e72b0148204a576db62c2148084193

# The newest symbol versions bookworm's glibc and libstdc++ provide.
MAX_GLIBC=2.36
MAX_GLIBCXX=3.4.30

# Libraries every glibc system has. Anything else would have to be installed
# in the image that loads the addon.
ALLOWED_NEEDED="libc.so.6 libm.so.6 libdl.so.2 libpthread.so.0 libgcc_s.so.1 libstdc++.so.6 ld-linux-x86-64.so.2"

LIB_NAME=libGodot3DTiles.linux.template_debug.x86_64.so
ASSET=cesium_godot-linux-x86_64

fail() {
  echo "error: $*" >&2
  exit 1
}

grep -q '^12\.' /etc/debian_version 2>/dev/null \
  || fail "run this on Debian 12 (bookworm); see the comment at the top of $0"
[ "$(id -u)" = 0 ] || fail "run this as root, it installs build packages"

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  autoconf autoconf-archive automake binutils build-essential ca-certificates \
  curl git libtool linux-libc-dev nasm ninja-build perl pkg-config tar unzip zip
python3 -m pip install --no-cache-dir --root-user-action=ignore \
  cmake==3.31.6 scons==4.8.1

git config --global --add safe.directory "$SRC"
SOURCE_DATE_EPOCH=$(git -C "$SRC" log -1 --format=%ct)
mkdir -p "$WORK/src" "$OUT"
git -C "$SRC" ls-files -z | tar -C "$SRC" --null -T - -cf - | tar -C "$WORK/src" -xf -

echo "Building litehtml $LITEHTML_COMMIT"
git init --quiet "$WORK/litehtml"
git -C "$WORK/litehtml" fetch --quiet --depth 1 https://github.com/litehtml/litehtml.git "$LITEHTML_COMMIT"
git -C "$WORK/litehtml" checkout --quiet FETCH_HEAD
cmake -S "$WORK/litehtml" -B "$WORK/litehtml/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLITEHTML_BUILD_TESTING=OFF
cmake --build "$WORK/litehtml/build" --parallel "$JOBS"
for archive in liblitehtml.a libgumbo.a; do
  cp "$(find "$WORK/litehtml/build" -name "$archive" -print -quit)" "$WORK/src/cesium_godot/third_party/litehtml/"
done

echo "Building the extension"
cd "$WORK/src"
# cesium-native builds with a bare `cmake --build`, which is serial without this.
export CMAKE_BUILD_PARALLEL_LEVEL=$JOBS
# cesium-native compiles itself with -Werror, and its CI only builds with GCC 13
# and newer. GCC 12 reports a false use-after-free in ReferenceCounted, so that
# check stays a warning. CMake reads CXXFLAGS when it configures cesium-native;
# scons does not read it.
export CXXFLAGS=-Wno-error=use-after-free
scons -j"$JOBS" platform=linux arch=x86_64 compileTarget=extension \
  target=template_debug buildCesium=true debug_symbols=yes

ADDON="$WORK/src/godot3dtiles/addons/cesium_godot"
LIB="$ADDON/lib/$LIB_NAME"
[ -f "$LIB" ] || fail "the build did not produce $LIB"

echo "Checking what the library needs at load time"
newest_version() {
  objdump -T "$LIB" | grep -o "$1_[0-9][0-9.]*" | sed "s/^$1_//" | sort -uV | tail -1
}
for pair in "GLIBC $MAX_GLIBC" "GLIBCXX $MAX_GLIBCXX"; do
  set -- $pair
  found=$(newest_version "$1")
  [ "$(printf '%s\n%s\n' "$found" "$2" | sort -V | tail -1)" = "$2" ] \
    || fail "the library needs $1_$found, newer than bookworm's $1_$2"
  echo "  newest $1 version: $found (bookworm has $2)"
done
for needed in $(readelf -d "$LIB" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
  case " $ALLOWED_NEEDED " in
    *" $needed "*) ;;
    *) fail "the library links $needed, which the Canvas image may not have" ;;
  esac
done
undefined=$(ldd -r "$LIB" 2>&1 | grep 'undefined symbol' || true)
[ -z "$undefined" ] || fail "the library has unresolved symbols:"$'\n'"$undefined"

echo "Splitting off debug info"
(
  cd "$ADDON/lib"
  objcopy --only-keep-debug "$LIB_NAME" "$LIB_NAME.debug"
  objcopy --strip-debug --add-gnu-debuglink="$LIB_NAME.debug" "$LIB_NAME"
)

echo "Packaging"
mkdir -p "$WORK/stage" "$WORK/stage-debug/cesium_godot/lib"
cp -a "$ADDON" "$WORK/stage/"
mv "$WORK/stage/cesium_godot/lib/$LIB_NAME.debug" "$WORK/stage-debug/cesium_godot/lib/"
pack() {
  tar --sort=name --owner=0 --group=0 --numeric-owner --mtime="@$SOURCE_DATE_EPOCH" \
    -C "$1" -cf - cesium_godot | gzip -9n > "$2"
}
pack "$WORK/stage" "$OUT/$ASSET.tar.gz"
pack "$WORK/stage-debug" "$OUT/$ASSET-debug.tar.gz"
(cd "$OUT" && sha256sum "$ASSET.tar.gz" "$ASSET-debug.tar.gz" > SHA256SUMS)
# Hand the files to whoever owns $OUT (the host user when run under Docker).
chown "$(stat -c %u:%g "$OUT")" "$OUT/$ASSET.tar.gz" "$OUT/$ASSET-debug.tar.gz" "$OUT/SHA256SUMS"

tar -tzvf "$OUT/$ASSET.tar.gz" | grep '/lib/'
cat "$OUT/SHA256SUMS"
