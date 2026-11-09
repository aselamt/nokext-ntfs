# NTFSOtomatik

Automatic, **write-enabled** mounting of external NTFS (Windows) drives on macOS 26/27 —
using the open-source **ntfs-3g** driver and the **macFUSE FSKit backend** (no kernel extension,
no Recovery Mode, no reduced security).

Plug in an NTFS drive → it is mounted read-write automatically → a notification appears.
Eject it in Finder when done.

> 🇹🇷 Türkçe açıklama: [README.tr.md](README.tr.md) · Kullanım kılavuzu: [KULLANIM.md](KULLANIM.md)

> **No warranty.** "Don't lose my data" is a design goal, not a guarantee. NTFS is a closed
> Microsoft format written here by a third-party driver. Keep backups of anything important.

## Why another one?

Existing tools either need the macFUSE kernel extension (Recovery Mode, "Reduced Security"),
or run the driver as root, or delete Windows hibernation data / reset the NTFS journal by default.
On macOS 27 several assumptions of older tools also stop working (see [docs/TASARIM.md](docs/TASARIM.md)):

- Raw disk access requires **Full Disk Access**, even for root.
- macFUSE FSKit modules are enabled **per user**, so a FUSE file system started as root is refused
  ("File system extension not enabled").
- macOS keeps re-mounting the same partition read-only with its own NTFS driver while ntfs-3g has it
  open — two drivers on one partition. This corrupted directory indexes in testing until it was blocked.

## How it works

```
 disk plugged in ──► DiskArbitration ──► ntfsotomatikd (root, LaunchDaemon, has Full Disk Access)
                                         │  • accepts only ^disk[0-9]+s[0-9]+$, external, VolumeKind=ntfs
                                         │  • refuses macOS's own (read-only) mount of that partition
                                         │  • opens /dev/diskXsY, drops privileges to the console user
                                         ▼
                         ntfs-3g /dev/fd/3 /Volumes/<name> -o norecover,backend=fskit,local,
                                                               streams_interface=openxattr,…
                         (runs as the logged-in user, hardened runtime, never as root)
 notifications: root appends lines to /Library/NTFSOtomatik/durum/bildirim (root-owned);
                a per-user LaunchAgent only reads it and shows macOS notifications (one-way).
```

Safety rules built in:

- ntfs-3g always runs with `norecover`; `recover`, `remove_hiberfile`, `force` are never used;
  ntfsprogs (incl. `ntfsfix`) are not even built.
- A Windows volume that was not shut down cleanly (Fast Startup / hibernation) is mounted **read-only**
  by ntfs-3g and the user is told to shut Windows down fully.
- If a read-write mount fails, macOS's own read-only NTFS support is used; if that fails too, the disk
  is left unmounted. Notifications state the real cause.
- Never forces an unmount. A disk ejected in Finder is not re-mounted until it is physically re-attached.
- Mount point names are sanitized (no `/ : , = \ "`, control characters, `.`/`..`).
- Everything that runs as root, and all parent folders, is owned by `root:wheel`; nothing comes from Homebrew.
- Versioned install (`/Library/NTFSOtomatik/surumler/<ver>`, `current`/`onceki` links) with one-command rollback.

## Requirements

- macOS 26 or 27 (developed and tested on macOS 27.2, Apple Silicon M4)
- Xcode Command Line Tools (`xcode-select --install`)
- [macFUSE](https://macfuse.github.io) **5.5.0** (Core is enough; installed separately, not bundled)

## Build & install

```sh
./build.sh                 # downloads ntfs-3g 2026.9.28, verifies SHA-256, applies patches, builds into dist/
sudo ./kur.sh              # installs into /Library/NTFSOtomatik and loads the launchd jobs
```

Then, in System Settings:

1. **Privacy & Security → Full Disk Access → +** → `/Library/NTFSOtomatik/bin/ntfsotomatikd`
2. **General → Login Items & Extensions → File System Extensions (ⓘ)** → enable **macFUSE** and **macFUSE (local)**
   (they appear after the first NTFS disk is plugged in).

Note: the binaries are ad-hoc signed, so Full Disk Access must be granted again whenever
`ntfsotomatikd` is rebuilt.

## Management (Turkish command names)

| Command | What it does |
|---|---|
| `/Library/NTFSOtomatik/betikler/durum.sh` | status, versions, mounted disks, last log lines |
| `sudo …/servis.sh kapat` / `ac` | stop / start the service |
| `sudo …/geri-al.sh` | roll back to the previous ntfs-3g version |
| `sudo …/guncelle.sh <dir> <ver>` | activate a new prepared ntfs-3g version |
| `sudo …/kaldir.sh [--macfuse]` | uninstall (optionally macFUSE too, via its own uninstaller) |

All of them refuse to run while an NTFS disk is mounted by ntfs-3g. Log: `/Library/Logs/NTFSOtomatik/ntfsotomatik.log` (1 MB × 3).

## Known limitations

- Very long file names with many Turkish/accented letters can exceed macOS's 255-byte name limit once
  decomposed (NFD) and become inaccessible on the Mac.
- The FSKit backend is slow (≈4–5 MB/s writes, ≈12 MB/s reads on a USB stick in testing).
- Not tested: two disks with the same volume name, a hibernated Windows volume, behaviour with
  exFAT/APFS disks attached at the same time.

## ntfs-3g patches (macOS only)

- `01-macos-unprivileged-blkdev.patch` — removes the Linux-specific "unprivileged user can not mount NTFS
  block devices" check on macOS; access control is done by the root launcher that opens the device.
- `02-macos-devfd-dup.patch` — a `/dev/fd/N` device path is duplicated instead of re-opened
  (macOS re-checks device permissions on re-open).

## License

GPL-2.0-or-later (see [LICENSE](LICENSE)); the patches modify ntfs-3g, which is GPL.
ntfs-3g © Tuxera and contributors. macFUSE © Benjamin Fleischer — not included; it has its own license.
