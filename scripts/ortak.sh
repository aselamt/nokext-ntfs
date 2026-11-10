# SPDX-License-Identifier: GPL-2.0-or-later
# NoKext NTFS ortak yardımcılar (diğer betikler bunu yükler)
N=/Library/NTFSOtomatik
LOG=/Library/Logs/NTFSOtomatik/ntfsotomatik.log
DAEMON_PLIST=/Library/LaunchDaemons/local.ntfsotomatik.plist
AGENT_PLIST=/Library/LaunchAgents/local.ntfsotomatik.bildirim.plist
SERVIS=system/local.ntfsotomatik
PATH=/usr/bin:/bin:/usr/sbin:/sbin; export PATH

root_gerek() { [ "$(id -u)" = 0 ] || { echo "Bu komut yönetici yetkisi ister: başına 'sudo ' ekleyip tekrar çalıştır."; exit 1; }; }

# ntfs-3g ile bağlı (ya da bağlanmakta olan) bir NTFS disk varsa 0 döner
ntfs_bagli_mi() {
  pgrep -x ntfs-3g >/dev/null 2>&1 && return 0
  mount | grep -q '(macfuse' && return 0
  return 1
}
bagliysa_dur() {
  if ntfs_bagli_mi; then
    echo "DURDU: ntfs-3g ile bağlı bir NTFS disk var:"
    mount | grep '(macfuse' | sed 's/^/  /'
    echo "Önce bu diskleri Finder'dan çıkar (⏏), sonra tekrar dene. Zorla ayırma yapılmaz."
    exit 2
  fi
}
konsol_uid() { stat -f %u /dev/console; }

# Sürüm adı: yalnızca harf, rakam, nokta, tire; 1-40 karakter; tek satır; "." ve ".." değil.
# (grep satır satır eşleştirdiği için çok satırlı bir değeri kabul ederdi; case tüm dizgeye bakar.)
gecerli_surum() {
  case "$1" in
    ''|.|..|*[!0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-]*) return 1 ;;
  esac
  [ "${#1}" -le 40 ]
}

# ---- bakım kilidi ----
# bagliysa_dur tek başına yalnızca o anı denetler: servis çalışmaya devam ederse kontrolden hemen sonra
# takılan bir disk bağlanabilir. Bakım yapan betikler bu yüzden önce servisi durdurur (yeni bağlama
# başlayamaz), SONRA bağlı / başlamakta olan ntfs-3g var mı diye bakar. Betik nasıl biterse bitsin
# (hata, Ctrl-C) servis önceden açıksa yeniden başlatılır; istenmiyorsa SERVIS_ACIKTI=0 yapılır.
# Servis dururken takılan NTFS diskleri macOS salt-okunur bağlar; servis açılınca onları devralır.
SERVIS_ACIKTI=0
bakim_bitir() {
  if [ "$SERVIS_ACIKTI" = 1 ] && [ -f "$DAEMON_PLIST" ] && ! launchctl print "$SERVIS" >/dev/null 2>&1; then
    launchctl bootstrap system "$DAEMON_PLIST" 2>/dev/null \
      || echo "UYARI: servis yeniden başlatılamadı. Elle: sudo $N/betikler/servis.sh ac"
  fi
}
bakim_baslat() {
  # Önce, servis daha ÇALIŞIRKEN bakılır: bağlı disk varken servis hiç durdurulmaz. Servis dururken
  # macOS'un kendi bağlama isteklerini reddeden kimse olmaz (aynı bölüme iki sürücü). Servis durduktan
  # sonraki ikinci denetim, bu arada başlamış bir bağlamayı yakalar.
  bagliysa_dur
  launchctl print "$SERVIS" >/dev/null 2>&1 && SERVIS_ACIKTI=1
  trap bakim_bitir EXIT
  trap 'exit 130' HUP INT TERM
  launchctl bootout "$SERVIS" 2>/dev/null || true
  i=0
  while pgrep -x ntfsotomatikd >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 1; i=$((i+1)); done
  if pgrep -x ntfsotomatikd >/dev/null 2>&1; then echo "DURDU: servis (ntfsotomatikd) durdurulamadı; hiçbir şey değiştirilmedi."; exit 1; fi
  bagliysa_dur
}

# ---- sürüm bağları (current / onceki) ----
# $N/<ad> sembolik bağını atomik olarak <hedef>e çevirir. Olmazsa bağ olduğu gibi kalır ve 1 döner.
bag_degistir() {
  ln -sfn "$2" "$N/$1.yeni" && chown -h root:wheel "$N/$1.yeni" && mv -fh "$N/$1.yeni" "$N/$1" \
    || { rm -f "$N/$1.yeni"; return 1; }
}
# current'ı <yeni-current>, onceki'yi <yeni-onceki> yapar (boşsa onceki'ye dokunmaz).
# İkinci değişiklik olmazsa birincisi geri çevrilir: ya ikisi birden değişir ya hiçbiri.
surum_degistir() {
  eski_c=$(readlink "$N/current" 2>/dev/null || true)
  bag_degistir current "$1" || { echo "HATA: current değiştirilemedi; sürümler değişmedi."; return 1; }
  [ -n "$2" ] || return 0
  bag_degistir onceki "$2" && return 0
  if [ -n "$eski_c" ] && bag_degistir current "$eski_c"; then
    echo "HATA: onceki değiştirilemedi; current eski haline ($eski_c) döndürüldü. Sürümler değişmedi."
  else
    echo "HATA: onceki değiştirilemedi ve current geri çevrilemedi."
    echo "  Şu an: current -> $(readlink "$N/current" 2>/dev/null), onceki -> $(readlink "$N/onceki" 2>/dev/null)"
    echo "  Olması gereken: current -> $eski_c. Elle düzeltin."
  fi
  return 1
}
