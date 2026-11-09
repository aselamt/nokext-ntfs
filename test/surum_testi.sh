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
  cp "$HERE/scripts/geri-al.sh" "$HERE/scripts/ortak.sh" "$W/b/"
  cat >> "$W/b/ortak.sh" <<EOF

# ---- test ortamı ----
N="$W/N"; LOG=/dev/null
root_gerek() { :; }
bakim_baslat() { :; }
chown() { :; }
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

exit $HATA
