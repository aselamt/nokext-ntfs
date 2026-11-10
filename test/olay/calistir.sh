#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# ntfsotomatikd olay sırası testlerini derleyip çalıştırır (macOS ya da Linux, clang; yetki gerekmez).
# Kullanım: sh test/olay/calistir.sh
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
W=$(mktemp -d "${TMPDIR:-/tmp}/olay_testi.XXXXXX")
trap 'rm -rf "$W"' EXIT
clang -std=gnu11 -fblocks -O0 -g -U_FORTIFY_SOURCE -Wall -Wno-unused-function -Wno-unused-parameter \
      -I "$HERE/sahte" -I "$HERE" -o "$W/olay_testi" "$HERE/olay_testi.c" "$HERE/sahte.c"
"$W/olay_testi" "$W"
