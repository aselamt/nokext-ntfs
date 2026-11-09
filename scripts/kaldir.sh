#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# NTFSOtomatik'i kaldırır: yalnızca bu kurulumun eklediği parçalar silinir.
# Kullanım: sudo kaldir.sh            (servis + NTFSOtomatik dosyaları)
#           sudo kaldir.sh --macfuse  (ek olarak macFUSE'u kendi resmi kaldırıcısıyla kaldırır)
. "$(dirname "$0")/ortak.sh"
root_gerek
bagliysa_dur
U=$(konsol_uid)
launchctl bootout system/local.ntfsotomatik 2>/dev/null || true
launchctl bootout gui/$U/local.ntfsotomatik.bildirim 2>/dev/null || true
rm -f "$DAEMON_PLIST" "$AGENT_PLIST"
rm -rf /Library/Logs/NTFSOtomatik
if [ "${1:-}" = "--macfuse" ]; then
  K=/Library/Filesystems/macfuse.fs/Contents/Resources/uninstall_macfuse.app/Contents/Resources/Scripts/uninstall_macfuse.sh
  [ -x "$K" ] && /bin/bash "$K" && echo "macFUSE kaldırıldı."
fi
rm -rf "$N"
echo "NTFSOtomatik kaldırıldı."
echo "Elle yapman gerekenler (Sistem Ayarları):"
echo "  - Gizlilik ve Güvenlik > Tam Disk Erişimi: ntfsotomatikd, ntfs-baslatici, ntfs-3g satırlarını '−' ile sil"
echo "  - macFUSE'u da kaldırdıysan: Genel > Oturum Açma Öğeleri ve Genişletmeler > Dosya Sistemi Genişletmeleri'nde macFUSE görünmez"
