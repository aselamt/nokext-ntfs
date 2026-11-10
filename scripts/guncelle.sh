#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Yeni bir ntfs-3g sürüm klasörünü etkinleştirir.
# Kullanım: sudo guncelle.sh <hazırlanmış-klasör> <sürüm-adı>
#   <hazırlanmış-klasör>: içinde bin/ntfs-3g ve lib/libfuse.2.dylib olan klasör
#   <sürüm-adı>: ör. 2026.9.28-5 (yalnızca harf, rakam, nokta, tire)
set -eu
. "$(dirname "$0")/ortak.sh"
root_gerek
bagliysa_dur   # erken uyarı; asıl denetim bakim_baslat'ta
SRC="${1:-}"; V="${2:-}"
gecerli_surum "$V" || { echo "Geçersiz sürüm adı."; exit 1; }
[ -x "$SRC/bin/ntfs-3g" ] && [ -f "$SRC/lib/libfuse.2.dylib" ] || { echo "Klasörde bin/ntfs-3g ve lib/libfuse.2.dylib olmalı."; exit 1; }
D="$N/surumler/$V"
[ -e "$D" ] && { echo "$D zaten var."; exit 1; }
# ntfs-3g yalnızca bu sürüm klasöründeki libfuse'u kullanmalı
otool -L "$SRC/bin/ntfs-3g" | grep -qF "$D/lib/libfuse.2.dylib" || { echo "ntfs-3g, $D/lib/libfuse.2.dylib yolunu kullanacak şekilde derlenmemiş."; exit 1; }
otool -L "$SRC/bin/ntfs-3g" | grep -E '/opt/homebrew|/usr/local' && { echo "Homebrew/usr/local bağımlılığı var; reddedildi."; exit 1; }
# Kopyalama, sahiplik ya da deneme adımlarından biri olmazsa (set -e) yarım sürüm klasörü silinir;
# current'a dokunulmaz ve aynı sürüm adıyla yeniden denenebilir.
mkdir "$D"
trap 'echo "Güncelleme yarıda kaldı; $D silindi, current değiştirilmedi."; rm -rf "$D"' EXIT
cp -R "$SRC"/. "$D"/
chown -R root:wheel "$D"; find "$D" -type d -exec chmod 755 {} +; find "$D" -type f -exec chmod go-w {} +
"$D/bin/ntfs-3g" --version >/dev/null 2>&1 || { echo "Yeni ntfs-3g çalışmıyor."; exit 1; }
trap - EXIT
C=$(readlink "$N/current" 2>/dev/null || true)
bakim_baslat   # servis durur, bağlı disk yoksa devam; çıkışta servis yeniden başlar
surum_degistir "surumler/$V" "$C" || exit 1
echo "Güncellendi: current -> surumler/$V, onceki -> $C"
echo "$(date '+%F %T') guncelle: $C -> surumler/$V" >> "$LOG"
