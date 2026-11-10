#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# geri-al.sh / guncelle.sh sürüm bağı değişikliklerinin regresyon testi (yetki gerekmez, sisteme dokunmaz).
# Gerçek betikler geçici bir klasöre kopyalanır; /Library yerine o klasör kullanılır, launchctl/chown
# devre dışı bırakılır ve sembolik bağ değişikliklerinden biri isteğe bağlı olarak başarısız yapılır.
# Kullanım: sh test/surum_testi.sh
set -u
HERE="$(cd "$(dirname "$0")/.." && pwd)"
W=$(mktemp -d "${TMPDIR:-/tmp}/surum_testi.XXXXXX")
trap 'rm -rf "$W"' EXIT
HATA=0

kur() {   # temiz kurulum: surumler/{a,b}, current -> b, onceki -> a
  rm -rf "$W/N" "$W/b"; mkdir -p "$W/N" "$W/b"
  for v in a b; do
    mkdir -p "$W/N/surumler/$v/bin"
    printf '#!/bin/sh\nexit 0\n' > "$W/N/surumler/$v/bin/ntfs-3g"; chmod +x "$W/N/surumler/$v/bin/ntfs-3g"
  done
  ln -s surumler/b "$W/N/current"; ln -s surumler/a "$W/N/onceki"
  cp "$HERE/scripts/geri-al.sh" "$HERE/scripts/guncelle.sh" "$HERE/scripts/ortak.sh" "$W/b/"
  cat >> "$W/b/ortak.sh" <<EOF

# ---- test ortamı ----
N="$W/N"; LOG=/dev/null
root_gerek() { :; }
bakim_baslat() { :; }
bagliysa_dur() { :; }
chown() { :; }
otool() { printf '\t%s (compatibility version 12.0.0)\n' "\$N/surumler/\$V/lib/libfuse.2.dylib"; }
mv() {   # mv -fh <kaynak> <hedef>; BOZ=<ad> ise <ad>.yeni için hata verir
  [ -n "\${BOZ:-}" ] && [ "\$2" = "\$N/\$BOZ.yeni" ] && { echo "mv: sahte hata" >&2; return 1; }
  if [ "\$(uname)" = Darwin ]; then command mv -fh "\$2" "\$3"; else command mv -fT "\$2" "\$3"; fi
}
EOF
}
bag() { readlink "$W/N/$1"; }
bekle() {  # bekle <açıklama> <beklenen-çıkış> <çıkış> <current> <onceki>
  if [ "$3" = "$2" ] && [ "$(bag current)" = "$4" ] && [ "$(bag onceki)" = "$5" ] && [ ! -e "$W/N/current.yeni" ] && [ ! -e "$W/N/onceki.yeni" ]; then
    echo "TAMAM  $1"
  else
    echo "HATA   $1: çıkış $3 (beklenen $2), current -> $(bag current), onceki -> $(bag onceki)"; HATA=1
  fi
}

kur; sh "$W/b/geri-al.sh" >/dev/null 2>&1; r=$?
bekle "geri-al: normal"                         0 $r surumler/a surumler/b
kur; BOZ=onceki sh "$W/b/geri-al.sh" >/dev/null 2>&1; r=$?
bekle "geri-al: onceki değişmezse current geri"  1 $r surumler/b surumler/a
kur; BOZ=current sh "$W/b/geri-al.sh" >/dev/null 2>&1; r=$?
bekle "geri-al: current değişmezse dokunulmaz"   1 $r surumler/b surumler/a

yeni_surum() {  # yeni_surum <ntfs-3g çıkış kodu>: guncelle.sh için hazırlanmış klasör
  rm -rf "$W/yeni"; mkdir -p "$W/yeni/bin" "$W/yeni/lib"
  printf '#!/bin/sh\nexit %s\n' "$1" > "$W/yeni/bin/ntfs-3g"; chmod +x "$W/yeni/bin/ntfs-3g"
  : > "$W/yeni/lib/libfuse.2.dylib"
}
kur; yeni_surum 1; sh "$W/b/guncelle.sh" "$W/yeni" c >/dev/null 2>&1; r=$?
bekle "guncelle: yeni ntfs-3g çalışmazsa dokunulmaz" 1 $r surumler/b surumler/a
if [ -e "$W/N/surumler/c" ]; then echo "HATA   guncelle: yarım sürüm klasörü kaldı"; HATA=1; else echo "TAMAM  guncelle: yarım sürüm klasörü silindi"; fi
kur; yeni_surum 0; sh "$W/b/guncelle.sh" "$W/yeni" c >/dev/null 2>&1; r=$?
bekle "guncelle: normal"                          0 $r surumler/c surumler/b

# sürüm adı denetimi (ortak.sh gecerli_surum): tüm dizgeye bakmalı, çok satırlı değeri reddetmeli
. "$HERE/scripts/ortak.sh"
yeni_satir='
'
for v in "2026.9.28-1" "a" "x.y-z"; do
  if gecerli_surum "$v"; then echo "TAMAM  sürüm adı kabul: $v"; else echo "HATA   geçerli sürüm adı reddedildi: $v"; HATA=1; fi
done
for v in "" "." ".." "a/b" "a b" "ç" "gecerli${yeni_satir}/../../baska" "$(printf '%041d' 0)"; do
  if gecerli_surum "$v"; then echo "HATA   geçersiz sürüm adı kabul edildi: $(printf %s "$v" | tr '\n' '|')"; HATA=1
  else echo "TAMAM  sürüm adı ret: $(printf %s "$v" | tr '\n' '|')"; fi
done

exit $HATA
