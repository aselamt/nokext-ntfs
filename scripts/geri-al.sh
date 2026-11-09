#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Bir önceki ntfs-3g sürümüne döner (current <-> onceki). Kullanım: sudo geri-al.sh
. "$(dirname "$0")/ortak.sh"
root_gerek
bagliysa_dur
C=$(readlink "$N/current"); O=$(readlink "$N/onceki" 2>/dev/null || true)
[ -n "$O" ] && [ -x "$N/$O/bin/ntfs-3g" ] || { echo "Geri dönülecek sürüm yok."; exit 1; }
"$N/$O/bin/ntfs-3g" --version >/dev/null 2>&1 || { echo "Önceki sürüm çalışmıyor; dokunulmadı."; exit 1; }
ln -sfn "$O" "$N/current.yeni" && mv -fh "$N/current.yeni" "$N/current"
ln -sfn "$C" "$N/onceki.yeni" && mv -fh "$N/onceki.yeni" "$N/onceki"
chown -h root:wheel "$N/current" "$N/onceki"
echo "Geri alındı: current -> $(readlink $N/current), onceki -> $(readlink $N/onceki)"
echo "$(date '+%F %T') geri-al: current $C -> $O" >> "$LOG"
