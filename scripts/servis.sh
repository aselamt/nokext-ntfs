#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Kullanım: sudo servis.sh ac | kapat
. "$(dirname "$0")/ortak.sh"
root_gerek
U=$(konsol_uid)
case "${1:-}" in
  ac)
    launchctl print system/local.ntfsotomatik >/dev/null 2>&1 || launchctl bootstrap system "$DAEMON_PLIST"
    launchctl kickstart system/local.ntfsotomatik
    launchctl print gui/$U/local.ntfsotomatik.bildirim >/dev/null 2>&1 || launchctl bootstrap gui/$U "$AGENT_PLIST" 2>/dev/null || true
    launchctl kickstart gui/$U/local.ntfsotomatik.bildirim 2>/dev/null || true
    echo "Servis açıldı. Takılan NTFS diskler yazılabilir bağlanacak." ;;
  kapat)
    bakim_baslat       # önce servis durur, sonra denetlenir; bağlı disk varsa servis geri açılır
    SERVIS_ACIKTI=0    # başarıyla kapatıldı: çıkışta yeniden başlatılmasın
    launchctl bootout gui/$U/local.ntfsotomatik.bildirim 2>/dev/null || true
    echo "Servis kapatıldı. NTFS diskler macOS'un kendi salt-okunur desteğiyle açılır."
    echo "(Mac yeniden başlarsa servis, otomatik başlatma açıksa yeniden başlar; kalıcı kapatmak için kaldir.sh.)" ;;
  *) echo "Kullanım: sudo $0 ac | kapat"; exit 1 ;;
esac
