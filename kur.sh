#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# NoKext NTFS — TEK KOMUTLA kurulum:  sudo ./kur.sh
#   1) macFUSE yoksa resmi sürümünü indirip doğrulayarak kurar (scripts/macfuse-kur.sh)
#   2) dist/ yoksa ./build.sh'yi sizin hesabınızla (root olmadan) çalıştırır
#   3) aşağıdaki parçaları kurar ve servisleri yükler
#
# Kurulanlar (hepsi root:wheel):
#   /Library/NTFSOtomatik/{bin,betikler,durum,surumler/<sürüm>,current,onceki}
#   /Library/LaunchDaemons/local.ntfsotomatik.plist
#   /Library/LaunchAgents/local.ntfsotomatik.bildirim.plist
#   /Library/Logs/NTFSOtomatik/
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
DIST="$HERE/dist"
. "$HERE/scripts/ortak.sh"
root_gerek

MAJOR=$(sw_vers -productVersion | cut -d. -f1)
[ "$MAJOR" -ge 26 ] || { echo "macOS 26 veya üstü gerekli (FSKit). Bu Mac: $(sw_vers -productVersion)"; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "Xcode Command Line Tools gerekli. Önce şunu çalıştırıp kurun: xcode-select --install"; exit 1; }
bagliysa_dur   # erken uyarı (uzun adımlardan önce); asıl denetim 3. adımda servis durdurulduktan sonra

echo "==> 1/3 macFUSE"
/bin/sh "$HERE/scripts/macfuse-kur.sh"

if [ ! -f "$DIST/SURUM" ]; then
  echo "==> 2/3 derleme (root olmadan)"
  [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ] || { echo "Lütfen normal hesabınızdan 'sudo ./kur.sh' ile çalıştırın."; exit 1; }
  sudo -u "$SUDO_USER" "$HERE/build.sh"
else
  echo "==> 2/3 dist/ hazır ($(cat "$DIST/SURUM")); derleme atlandı"
fi
echo "==> 3/3 kurulum"
# Eski servis durdurulur (macFUSE kurulumu / derleme sırasında takılan diski artık bağlayamaz), sonra
# bağlı / bağlanmakta olan ntfs-3g yeniden denetlenir. Kurulum yarıda kalırsa eski servis geri açılır.
bakim_baslat

echo "==> dosyalar doğrulanıyor"
(cd "$DIST" && shasum -a 256 -c SHA256SUMS >/dev/null) || { echo "dist/ içindeki dosyalar değişmiş; yeniden derleyin."; exit 1; }
VER=$(cat "$DIST/SURUM")
echo "$VER" | grep -Eq '^[0-9A-Za-z.-]{1,40}$' || { echo "Geçersiz sürüm adı"; exit 1; }

umask 022
mkdir -p "$N/bin" "$N/betikler" "$N/durum" "$N/surumler" /Library/Logs/NTFSOtomatik
chown root:wheel "$N" "$N/bin" "$N/betikler" "$N/durum" "$N/surumler" /Library/Logs/NTFSOtomatik
chmod 755 "$N" "$N/bin" "$N/betikler" "$N/durum" "$N/surumler" /Library/Logs/NTFSOtomatik

echo "==> ntfs-3g sürümü: $VER"
D="$N/surumler/$VER"
if [ -e "$D" ]; then echo "  $D zaten var; olduğu gibi bırakıldı"; else
  cp -R "$DIST/surumler/$VER" "$D"
  chown -R root:wheel "$D"; find "$D" -type d -exec chmod 755 {} +; find "$D" -type f -exec chmod go-w {} +
fi
"$D/bin/ntfs-3g" --version >/dev/null 2>&1 || { echo "ntfs-3g çalışmıyor; kurulum durdu"; exit 1; }
C=$(readlink "$N/current" 2>/dev/null || true)
if [ "$C" != "surumler/$VER" ]; then
  surum_degistir "surumler/$VER" "$C" || exit 1
fi
chown -h root:wheel "$N/current"; [ -L "$N/onceki" ] && chown -h root:wheel "$N/onceki"

echo "==> servis, bildirim yardımcısı ve yönetim betikleri"
U=$(konsol_uid)
launchctl bootout gui/$U/local.ntfsotomatik.bildirim 2>/dev/null || true
for b in ntfsotomatikd ntfsotomatik-bildirim; do
  [ -f "$N/bin/$b" ] && ! cmp -s "$N/bin/$b" "$DIST/bin/$b" && cp -p "$N/bin/$b" "$N/bin/$b.onceki"
  install -o root -g wheel -m 755 "$DIST/bin/$b" "$N/bin/$b"
done
install -o root -g wheel -m 755 "$HERE"/scripts/durum.sh "$HERE"/scripts/servis.sh "$HERE"/scripts/guncelle.sh \
        "$HERE"/scripts/geri-al.sh "$HERE"/scripts/kaldir.sh "$N/betikler/"
install -o root -g wheel -m 644 "$HERE/scripts/ortak.sh" "$N/betikler/ortak.sh"
touch "$N/durum/bildirim"; chown root:wheel "$N/durum/bildirim"; chmod 644 "$N/durum/bildirim"
install -o root -g wheel -m 644 "$HERE/launchd/local.ntfsotomatik.plist" "$DAEMON_PLIST"
install -o root -g wheel -m 644 "$HERE/launchd/local.ntfsotomatik.bildirim.plist" "$AGENT_PLIST"
launchctl bootstrap system "$DAEMON_PLIST"
launchctl bootstrap gui/$U "$AGENT_PLIST" 2>/dev/null || true

cat <<'EOF'

Kurulum tamam. Son iki adım SİZDE (Sistem Ayarları):
 1) Gizlilik ve Güvenlik > Tam Disk Erişimi > "+" > Cmd+Shift+G >
    /Library/NTFSOtomatik/bin/ntfsotomatikd  > Aç  (anahtar açık olsun)
 2) Genel > Oturum Açma Öğeleri ve Genişletmeler > Kategoriye Göre >
    Dosya Sistemi Genişletmeleri (ⓘ) > "macFUSE" ve "macFUSE (local)" açık olsun
    (Listede yoksa bir NTFS disk takın; macFUSE ilk kullanımda kendini kaydeder.)
Sonra NTFS diski takın: "yazılabilir olarak bağlandı" bildirimi gelmeli.
Durum:  /Library/NTFSOtomatik/betikler/durum.sh
EOF
