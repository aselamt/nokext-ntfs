#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# NoKext NTFS durumu (yetki gerekmez, hiçbir şeyi değiştirmez)
. "$(dirname "$0")/ortak.sh"
echo "== Servis"
launchctl print system/local.ntfsotomatik 2>/dev/null | grep -E '^\s*(state|pid) =' | head -2 || echo "  yüklü değil"
echo "== Bildirim yardımcısı"
launchctl print gui/$(id -u)/local.ntfsotomatik.bildirim 2>/dev/null | grep -E '^\s*(state|pid) =' | head -2 || echo "  yüklü değil"
echo "== Sürümler"
echo "  current -> $(readlink $N/current)"; echo "  onceki  -> $(readlink $N/onceki 2>/dev/null || echo yok)"
"$N/current/bin/ntfs-3g" --version 2>&1 | head -1 | sed 's/^/  /'
echo "== ntfs-3g ile bağlı diskler"
mount | grep '(macfuse' | sed 's/^/  /' || true
ntfs_bagli_mi || echo "  yok"
echo "== Günlüğün son 15 satırı ($LOG)"
tail -15 "$LOG" 2>/dev/null | sed 's/^/  /'
