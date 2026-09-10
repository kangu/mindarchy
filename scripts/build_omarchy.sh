#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/.." && pwd)
version=${VERSION:-0.1.0}
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'VERSION must be major.minor.patch' >&2; exit 1; }
if [[ $(uname -s) != Linux ]]; then
    : "${OMARCHY_HOST:?Set OMARCHY_HOST to the SSH build machine}"
    : "${OMARCHY_SOURCE:?Set OMARCHY_SOURCE to its current qt-prototype checkout}"
    printf -v command 'cd %q && VERSION=%q bash scripts/build_omarchy.sh' "$OMARCHY_SOURCE" "$version"
    exec ssh "$OMARCHY_HOST" "$command"
fi
for tool in makepkg qmake6 make tar; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; install base-devel, qt6-base, qt6-declarative and qt6-wayland." >&2; exit 1; }
done
[[ $EUID != 0 ]] || { echo 'Run makepkg as a normal user, not root.' >&2; exit 1; }
output="$root/artifacts/omarchy"
mkdir -p "$output"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
# Stage only build inputs; never include old builds, user maps, or artifacts.
tar -C "$root" -cf "$work/source.tar" src qml assets packaging prototype.pro resources.qrc
mkdir "$work/source"
tar -C "$work/source" -xf "$work/source.tar"
cat > "$work/PKGBUILD" <<PKG
pkgname=mindarchy
pkgver=$version
pkgrel=1
pkgdesc='A desktop mind map editor'
arch=('x86_64' 'aarch64')
depends=('qt6-base' 'qt6-declarative' 'qt6-wayland')
makedepends=('gcc' 'make')
options=('!debug')
build() {
    mkdir -p "\$srcdir/build"
    cd "\$srcdir/build"
    qmake6 "\$startdir/source/prototype.pro" PREFIX=/usr
    make -j"\$(nproc)"
}
package() {
    cd "\$srcdir/build"
    make INSTALL_ROOT="\$pkgdir" install
}
PKG
(cd "$work" && PKGDEST="$output" makepkg --force --cleanbuild)
find "$output" -maxdepth 1 -name "mindarchy-$version-*.pkg.tar.*" -print
printf 'Install on Omarchy with: sudo pacman -U <package path>\n'
