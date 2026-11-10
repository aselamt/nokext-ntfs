#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Bir önceki ntfs-3g sürümüne döner (current <-> onceki). Kullanım: sudo geri-al.sh
set -eu
. "$(dirname "$0")/ortak.sh"
root_gerek
C=$(readlink "$N/current" 2>/dev/null || true); O=$(readlink "$N/onceki" 2>/dev/null || true)
[ -n "$C" ] && [ -n "$O" ] && [ -x "$N/$O/bin/ntfs-3g" ] || { echo "Geri dönülecek sürüm yok."; exit 1; }
"$N/$O/bin/ntfs-3g" --version >/dev/null 2>&1 || { echo "Önceki sürüm çalışmıyor; dokunulmadı."; exit 1; }
bakim_baslat   # servis durur, bağlı disk yoksa devam; çıkışta servis yeniden başlar
surum_degistir "$O" "$C" || exit 1
echo "Geri alındı: current -> $(readlink "$N/current"), onceki -> $(readlink "$N/onceki")"
echo "$(date '+%F %T') geri-al: current $C -> $O" >> "$LOG"
