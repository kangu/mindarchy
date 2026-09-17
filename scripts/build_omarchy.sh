#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/.." && pwd)
version=${VERSION:-}
release=0
for arg in "$@"; do
    case $arg in
        --release) release=1 ;;
        *) { echo "Unknown option: $arg" >&2; exit 1; } ;;
    esac
done

# On macOS this delegates to an Omarchy build machine; the tooling is Linux-native.
if [[ "$(uname -s)" != Linux ]]; then
    : "${OMARCHY_HOST:?Set OMARCHY_HOST to the SSH build machine}"
    : "${OMARCHY_SOURCE:?Set OMARCHY_SOURCE to its current qt-prototype checkout}"
    printf -v command 'cd %q && VERSION=%q bash scripts/build_omarchy.sh %s' "$OMARCHY_SOURCE" "$version" "$*"
    exec ssh "$OMARCHY_HOST" "$command"
fi

for tool in makepkg qmake6 make tar; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; install base-devel, qt6-base, qt6-declarative and qt6-wayland." >&2; exit 1; }
done
[[ $EUID != 0 ]] || { echo 'Run makepkg as a normal user, not root.' >&2; exit 1; }
[[ -f "$root/packaging/PKGBUILD" ]] || { echo "Missing $root/packaging/PKGBUILD" >&2; exit 1; }

output="$root/artifacts/omarchy"
mkdir -p "$output"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

if (( release )); then
    # Build the checked-in recipe exactly as the official Omarchy repository
    # would: the tag v$pkgver must already be published on GitHub.
    echo "Building release $version from packaging/PKGBUILD (tag v$version must exist on GitHub)."
    cp "$root/packaging/PKGBUILD" "$work/"
else
    # Package the current working tree through the same checked-in recipe:
    # stage the source archive, then substitute pkgver, source and checksum.
    [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Set VERSION to major.minor.patch (e.g. VERSION=0.1.5).' >&2; exit 1; }
    stage="$work/mindarchy-$version"
    mkdir "$stage"
    tar -C "$root" -cf - src qml assets packaging prototype.pro resources.qrc fonts.qrc | tar -xf - -C "$stage"
    tar -C "$work" -czf "$work/mindarchy-$version.tar.gz" "mindarchy-$version"
    checksum=$(cd "$work" && sha256sum "mindarchy-$version.tar.gz" | cut -d' ' -f1)
    sed -e "s/^pkgver=.*/pkgver='$version'/" \
        -e "s|^source=.*|source=(\"mindarchy-$version.tar.gz\")|" \
        -e "s/^sha256sums=.*/sha256sums=('$checksum')/" \
        "$root/packaging/PKGBUILD" > "$work/PKGBUILD"
    echo "Building working tree as $version (checksum $checksum)."
fi

cd "$work"
PKGDEST="$output" makepkg --force --cleanbuild
find "$output" -maxdepth 1 -name "mindarchy-*.pkg.tar.*" -newer "$root/packaging/PKGBUILD" -print
printf 'Install with: sudo pacman -U <package path>\n'
