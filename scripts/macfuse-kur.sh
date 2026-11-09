#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macFUSE'u RESMİ kaynağından indirip doğrulayarak kurar (yalnızca Core; ayar paneli kurulmaz).
# kur.sh tarafından çağrılır; tek başına da: sudo scripts/macfuse-kur.sh
#
# macFUSE © Benjamin Fleischer, kendi lisansıyla dağıtılır (https://github.com/macfuse/macfuse).
# Bu betik macFUSE'u depoya koymaz; resmi GitHub sürümünü indirir ve imzasını doğrular.
set -eu
PATH=/usr/bin:/bin:/usr/sbin:/sbin; export PATH

MACFUSE_VER=5.5.0
MACFUSE_URL="https://github.com/macfuse/macfuse/releases/download/macfuse-${MACFUSE_VER}/macfuse-${MACFUSE_VER}.dmg"
MACFUSE_DMG_SHA256=92c21ef2db4199bcf5bddc8e6efb57b23ecb9715f74ab38cba2c77ab226d3830
MACFUSE_SIGNER="Developer ID Installer: Benjamin Fleischer (3T5GSNBU6W)"

[ "$(id -u)" = 0 ] || { echo "Yönetici yetkisi gerekli: sudo $0"; exit 1; }

kurulu_surum() { pkgutil --pkg-info io.macfuse.installer.components.core 2>/dev/null | sed -n 's/^version: //p'; }
V=$(kurulu_surum)
if [ -n "$V" ] && [ -d /Library/Filesystems/macfuse.fs ]; then
  echo "macFUSE $V zaten kurulu; atlandı."
  exit 0
fi

# Tüm geçici dosyalar root'un mktemp -d ile oluşturduğu, adı tahmin edilemeyen ve önceden var
# olamayan bir klasörde durur. /private/tmp'de sabit adlı dosya KULLANILMAZ: başka bir yerel kullanıcı
# o ada önceden sembolik bağ koyarsa root'un cp'si bağın hedefinin üzerine yazar.
T=$(mktemp -d /private/tmp/ntfsotomatik-macfuse.XXXXXX)
temizle() { hdiutil detach -quiet "$T/mnt" 2>/dev/null || true; rm -rf "$T"; }
trap temizle EXIT INT TERM

echo "==> macFUSE $MACFUSE_VER indiriliyor (resmi GitHub sürümü)"
curl -fsSL -o "$T/macfuse.dmg" "$MACFUSE_URL"
echo "$MACFUSE_DMG_SHA256  $T/macfuse.dmg" | shasum -a 256 -c -
mkdir "$T/mnt"
hdiutil attach -quiet -nobrowse -readonly -mountpoint "$T/mnt" "$T/macfuse.dmg"
PKG="$T/macfuse.pkg"
cp "$T/mnt/Install macFUSE.pkg" "$PKG"
hdiutil detach -quiet "$T/mnt"

echo "==> imza doğrulanıyor"
pkgutil --check-signature "$PKG" | grep -q "$MACFUSE_SIGNER" || { echo "İmza beklenen geliştiriciye ait değil; durdu."; exit 1; }
spctl -a -t install "$PKG" >/dev/null 2>&1 || { echo "Apple noter onayı doğrulanamadı; durdu."; exit 1; }

cat > "$T/secim.xml" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><array><dict>
  <key>attributeSetting</key><integer>0</integer>
  <key>choiceAttribute</key><string>selected</string>
  <key>choiceIdentifier</key><string>io.macfuse.installer.components.preferencepane</string>
</dict></array></plist>
EOF

echo "==> kuruluyor (yalnızca macFUSE Core)"
if installer -pkg "$PKG" -applyChoiceChangesXML "$T/secim.xml" -target / ; then
  echo "macFUSE $(kurulu_surum) kuruldu."
else
  # Bazı macOS sürümlerinde komut satırından kurulum "Operation not permitted" ile durabiliyor;
  # bu durumda macOS'un kendi Yükleyici penceresi kullanılır.
  echo
  echo "Komut satırından kurulamadı; macOS Yükleyici penceresi açılıyor."
  echo "Pencerede: Devam > Kabul Et > 'Özelleştir' > 'macFUSE Preference Pane' işaretini kaldır > Yükle."
  echo "Sistem uzantısı (kext) izni isteyen bir uyarı çıkarsa İZİN VERMEYİN; FSKit yöntemi bunu gerektirmez."
  # Yükleyici kullanıcı olarak çalışır ve paketi okuyabilmeli. Paket kopyalanmaz; imzası doğrulanmış
  # dosya yerinde kalır. Klasör root'a ait ve 0755 (kullanıcı okur, içine yazamaz, adını değiştiremez:
  # /private/tmp yapışkan bitli), paket 0444.
  chown root:wheel "$T" "$PKG"; chmod 755 "$T"; chmod 444 "$PKG"
  U=$(stat -f %u /dev/console)
  launchctl asuser "$U" /usr/bin/open -a Installer "$PKG"
  printf "Kurulum bekleniyor"
  i=0
  while [ $i -lt 120 ] && [ -z "$(kurulu_surum)" ]; do sleep 5; printf "."; i=$((i+1)); done
  echo
  [ -n "$(kurulu_surum)" ] || { echo "macFUSE kurulmadı (10 dk bekendi). Kurulumu tamamlayıp betiği yeniden çalıştırın."; exit 1; }
  echo "macFUSE $(kurulu_surum) kuruldu."
fi
