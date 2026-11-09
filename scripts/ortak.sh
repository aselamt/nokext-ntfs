# SPDX-License-Identifier: GPL-2.0-or-later
# NTFSOtomatik ortak yardımcılar (diğer betikler bunu yükler)
N=/Library/NTFSOtomatik
LOG=/Library/Logs/NTFSOtomatik/ntfsotomatik.log
DAEMON_PLIST=/Library/LaunchDaemons/local.ntfsotomatik.plist
AGENT_PLIST=/Library/LaunchAgents/local.ntfsotomatik.bildirim.plist
PATH=/usr/bin:/bin:/usr/sbin:/sbin; export PATH

root_gerek() { [ "$(id -u)" = 0 ] || { echo "Bu komut yönetici yetkisi ister: başına 'sudo ' ekleyip tekrar çalıştır."; exit 1; }; }

# ntfs-3g ile bağlı bir NTFS disk varsa 0 döner
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
