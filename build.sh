#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# NoKext NTFS — derleme betiği (yönetici yetkisi GEREKMEZ; hiçbir şeyi sisteme kurmaz)
#
# Yaptıkları:
#  1) ntfs-3g kaynak arşivini indirir ve SHA-256 ile doğrular
#  2) patches/ içindeki iki macOS yamasını uygular
#  3) ntfs-3g'yi macFUSE'un libfuse'una karşı derler (ntfsprogs/ntfsfix DERLENMEZ)
#  4) servis (ntfsotomatikd) ve bildirim yardımcısını derler
#  5) her şeyi dist/ altına, kurulacağı yolları gösterecek şekilde hazırlar ve ad-hoc imzalar
#
# Gereksinimler: Xcode Command Line Tools, macFUSE 5.5.0 (Core) kurulu olmalı.
# Kullanım: ./build.sh [sürüm-adı]     (varsayılan: 2026.9.28-1)
set -eu

NTFS3G_VER=2026.9.28
NTFS3G_URL="https://tuxera.com/opensource/ntfs-3g_ntfsprogs-${NTFS3G_VER}.tgz"
NTFS3G_SHA256=350d9415c59f7c3fa74e23985ad2d56423c6a168337368f672e2e362d8f295c8
VER="${1:-${NTFS3G_VER}-1}"
case "$VER" in ''|.|..|*[!0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-]*) echo "Geçersiz sürüm adı: $VER"; exit 1 ;; esac
[ "${#VER}" -le 40 ] || { echo "Geçersiz sürüm adı: $VER"; exit 1; }

HERE="$(cd "$(dirname "$0")" && pwd)"
BUILD="$HERE/build"; DIST="$HERE/dist"
PREFIX=/Library/NTFSOtomatik/surumler/$VER     # ntfs-3g'nin libfuse'u arayacağı kesin yol

[ -f /usr/local/lib/libfuse.2.dylib ] && [ -f /usr/local/include/fuse/fuse.h ] || {
  echo "macFUSE bulunamadı. Önce: sudo scripts/macfuse-kur.sh  (ya da doğrudan: sudo ./kur.sh)"; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode Command Line Tools gerekli: xcode-select --install"; exit 1; }

rm -rf "$BUILD" "$DIST"; mkdir -p "$BUILD/dl" "$DIST"

echo "==> ntfs-3g $NTFS3G_VER indiriliyor"
curl -fsSL -o "$BUILD/dl/ntfs-3g.tgz" "$NTFS3G_URL"
echo "$NTFS3G_SHA256  $BUILD/dl/ntfs-3g.tgz" | shasum -a 256 -c -

echo "==> açılıyor ve yamalanıyor"
tar xzf "$BUILD/dl/ntfs-3g.tgz" -C "$BUILD"
SRC="$BUILD/ntfs-3g-$NTFS3G_VER"
for p in "$HERE"/patches/*.patch; do (cd "$SRC" && patch -p0 --forward < "$p"); done

echo "==> yapılandırılıyor (ntfsprogs ve onarım araçları olmadan)"
chmod +x "$HERE/tools/pkg-config"
(cd "$SRC" && PKG_CONFIG="$HERE/tools/pkg-config" ./configure \
    --prefix="$PREFIX" --disable-ntfsprogs --disable-shared --enable-static \
    --disable-ldconfig --disable-mtab --disable-crypto --disable-extras --with-fuse=external \
    > "$BUILD/configure.log" 2>&1) || { echo "configure başarısız; bkz. $BUILD/configure.log"; exit 1; }
grep -q '#define ENABLE_NFCONV 1' "$SRC/config.h" || { echo "nfconv (Unicode ad dönüşümü) açık değil"; exit 1; }

echo "==> derleniyor"
(cd "$SRC" && make -j"$(sysctl -n hw.ncpu)" > "$BUILD/make.log" 2>&1) || { echo "make başarısız; bkz. $BUILD/make.log"; exit 1; }

echo "==> sürüm klasörü hazırlanıyor: dist/surumler/$VER"
V="$DIST/surumler/$VER"; mkdir -p "$V/bin" "$V/lib" "$V/man" "$V/kaynak"
cp "$SRC/src/ntfs-3g" "$V/bin/"
cp /usr/local/lib/libfuse.2.dylib "$V/lib/"           # macFUSE'un imzalı kütüphanesi, değiştirilmeden
cp "$SRC/src/ntfs-3g.8" "$V/man/"
cp "$HERE"/patches/*.patch "$V/kaynak/"
install_name_tool -change /usr/local/lib/libfuse.2.dylib "$PREFIX/lib/libfuse.2.dylib" "$V/bin/ntfs-3g"
codesign -f -s - -o runtime --entitlements "$HERE/src/ntfs3g.entitlements" "$V/bin/ntfs-3g"
otool -L "$V/bin/ntfs-3g" | grep -q "$PREFIX/lib/libfuse.2.dylib" || { echo "libfuse yolu yanlış"; exit 1; }
if otool -L "$V/bin/ntfs-3g" | grep -Eq '/opt/homebrew|/usr/local'; then echo "Homebrew//usr/local bağımlılığı kaldı"; exit 1; fi
cat > "$V/KAYNAK.txt" <<EOF
NoKext NTFS sürüm klasörü $VER
ntfs-3g $NTFS3G_VER — $NTFS3G_URL (SHA-256 $NTFS3G_SHA256)
configure: --disable-ntfsprogs --disable-shared --enable-static --disable-crypto --disable-extras --with-fuse=external
Yamalar: kaynak/*.patch (yalnızca macOS). libfuse.2.dylib: macFUSE'tan değiştirilmeden kopyalandı.
Derleme: $(date '+%Y-%m-%d %H:%M') $(clang --version | head -1)
EOF

echo "==> servis ve bildirim yardımcısı derleniyor"
mkdir -p "$DIST/bin"
clang -O2 -Wall -Wextra -framework CoreFoundation -framework DiskArbitration -framework SystemConfiguration \
      -o "$DIST/bin/ntfsotomatikd" "$HERE/src/ntfsotomatikd.c"
clang -O2 -Wall -Wextra -o "$DIST/bin/ntfsotomatik-bildirim" "$HERE/src/ntfsotomatik-bildirim.c"
codesign -f -s - -o runtime -i local.ntfsotomatikd "$DIST/bin/ntfsotomatikd"
codesign -f -s - -o runtime -i local.ntfsotomatik-bildirim "$DIST/bin/ntfsotomatik-bildirim"

echo "$VER" > "$DIST/SURUM"
(cd "$DIST" && find . -type f ! -name SHA256SUMS | sort | xargs shasum -a 256 > SHA256SUMS)
echo
echo "Hazır: $DIST"
echo "Kurmak için: sudo ./kur.sh"
