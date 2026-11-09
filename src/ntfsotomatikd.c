// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ntfsotomatikd — NoKext NTFS root servisi
 *
 * Görev: Harici NTFS bölümleri takıldığında onları ntfs-3g ile yazılabilir bağlamak.
 *
 * Güvenlik kuralları:
 *  - Dışarıdan komut / serbest metin kabul etmez: soket, argüman, yapılandırma dosyası yok.
 *    Yalnızca DiskArbitration olaylarına tepki verir (yoklama döngüsü yok).
 *  - Aygıt adı ^disk[0-9]+s[0-9]+$ olmalı; işlem öncesi DiskArbitration'dan yeniden doğrulanır:
 *    harici + NTFS değilse hiçbir şey yapılmaz (APFS, exFAT, FAT32, HFS+ ve dahili disklere dokunulmaz).
 *  - Diskler aygıt adına göre değil, kimliğe (birim UUID / ortam UUID / aygıt yolu+boyut) göre izlenir.
 *  - ntfs-3g her zaman -o norecover ile; recover, remove_hiberfile, force KULLANILMAZ. ntfsfix yok.
 *  - Zorla ayırma yok: macOS'un salt-okunur bağlantısı yalnızca zorlamasız ayrılır, olmazsa dokunulmaz.
 *  - ntfs-3g root olarak çalışmaz: servis aygıtı açar, alt süreçte yetki konsol kullanıcısına düşürülür,
 *    ntfs-3g /dev/fd/3 ile başlatılır.
 *  - Bağlama noktası adı birim adından türetilir ve temizlenir (/ : , = denetim karakterleri, . ve ..).
 *  - Yazılabilir bağlama olmazsa macOS'un kendi salt-okunur desteği denenir; o da olmazsa disk bağlı gösterilmez.
 *  - Finder'dan çıkarılan disk, fiziksel olarak çıkarılıp takılana kadar yeniden bağlanmaz.
 *  - Bildirim tek yönlüdür: root yalnızca /Library/NTFSOtomatik/durum/bildirim dosyasına satır ekler.
 *  - Günlük boyutu sınırlıdır (1 MB, 3 eski dosya).
 */
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>

#define BASE        "/Library/NTFSOtomatik"
#define NTFS3G      BASE "/current/bin/ntfs-3g"
#define NOTIFY_DIR  BASE "/durum"
#define NOTIFY_FILE NOTIFY_DIR "/bildirim"
#define LOG_DIR     "/Library/Logs/NTFSOtomatik"
#define LOG_FILE    LOG_DIR "/ntfsotomatik.log"
#define LOG_MAX     (1024 * 1024)
#define LOG_KEEP    3
#define NOTIFY_MAX  (64 * 1024)
#define MAX_DISKS   32
#define MOUNT_WAIT  45 /* saniye */

extern char **environ;

/* ---------- günlük ---------- */

static void rotate_if_needed(const char *path, off_t max, int keep) {
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size < max) return;
    char a[PATH_MAX], b[PATH_MAX];
    for (int i = keep - 1; i >= 1; i--) {
        snprintf(a, sizeof a, "%s.%d", path, i);
        snprintf(b, sizeof b, "%s.%d", path, i + 1);
        rename(a, b);
    }
    snprintf(a, sizeof a, "%s.1", path);
    rename(path, a);
}

static void logf_(const char *fmt, ...) {
    rotate_if_needed(LOG_FILE, LOG_MAX, LOG_KEEP);
    int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) return;
    char ts[32]; time_t t = time(NULL); struct tm tm; localtime_r(&t, &tm);
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    char msg[2048];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    dprintf(fd, "%s %s\n", ts, msg);
    close(fd);
}

/* Bildirim: tek satır, denetim karakterleri temizlenir. Dosya root:wheel 0644. */
static void notify_user(const char *fmt, ...) {
    char msg[1024];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    for (char *p = msg; *p; p++) if ((unsigned char)*p < 0x20 || *p == 0x7f) *p = ' ';
    struct stat st;
    int fd = open(NOTIFY_FILE, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0644);
    if (fd < 0) { logf_("bildirim dosyası açılamadı: %s", strerror(errno)); return; }
    if (fstat(fd, &st) == 0 && st.st_size > NOTIFY_MAX) ftruncate(fd, 0);
    dprintf(fd, "%ld\t%s\n", (long)time(NULL), msg);
    close(fd);
    logf_("BİLDİRİM: %s", msg);
}

/* ---------- yardımcılar ---------- */

static int valid_bsd(const char *s) {
    if (!s || strncmp(s, "disk", 4) != 0) return 0;
    const char *p = s + 4; int n = 0;
    while (*p >= '0' && *p <= '9') { p++; n++; }
    if (n == 0 || n > 4 || *p != 's') return 0;
    p++; n = 0;
    while (*p >= '0' && *p <= '9') { p++; n++; }
    return n > 0 && n <= 4 && *p == '\0';
}

static int cfstr(CFTypeRef v, char *out, size_t len) {
    out[0] = 0;
    if (!v || CFGetTypeID(v) != CFStringGetTypeID()) return 0;
    return CFStringGetCString(v, out, (CFIndex)len, kCFStringEncodingUTF8);
}

static int uuid_str(CFTypeRef v, char *out, size_t len) {
    out[0] = 0;
    if (!v || CFGetTypeID(v) != CFUUIDGetTypeID()) return 0;
    CFStringRef s = CFUUIDCreateString(kCFAllocatorDefault, v);
    int ok = cfstr(s, out, len); CFRelease(s); return ok;
}

/* Bağlama noktası / volname için güvenli ad: tek yol bileşeni, ntfs-3g seçenek ayırıcısı yok. */
static void sanitize_name(const char *in, char *out, size_t len) {
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && o + 1 < len; p++) {
        unsigned char c = *p;
        if (c < 0x20 || c == 0x7f || c == '/' || c == ':' || c == ',' || c == '=' || c == '\\' || c == '"')
            c = '_';
        out[o++] = (char)c;
    }
    out[o] = 0;
    /* UTF-8 dizisini yarıda kesmemek için 100 bayt sınırında geri çekil */
    if (o > 100) { o = 100; while (o > 0 && ((unsigned char)out[o] & 0xC0) == 0x80) o--; out[o] = 0; }
    while (o > 0 && out[o - 1] == ' ') out[--o] = 0;
    size_t s = 0; while (out[s] == '.' || out[s] == ' ') s++;
    if (s) memmove(out, out + s, strlen(out + s) + 1);
    if (!out[0] || !strcmp(out, ".") || !strcmp(out, "..")) snprintf(out, len, "NTFS");
}

static int is_mountpoint(const char *path) {
    struct statfs sf;
    if (statfs(path, &sf) != 0) return 0;
    return strcmp(sf.f_mntonname, path) == 0;
}

/* Konsol kullanıcısı (giriş yapmış kişi). Yoksa ya da root/loginwindow ise 0 döner. */
static int console_user(uid_t *uid, gid_t *gid, char *name, size_t len) {
    CFStringRef u = SCDynamicStoreCopyConsoleUser(NULL, uid, gid);
    if (!u) return 0;
    int ok = cfstr(u, name, len); CFRelease(u);
    if (!ok || *uid == 0 || !strcmp(name, "loginwindow")) return 0;
    return 1;
}

/* ---------- disk durumu ---------- */

enum dstate { ST_FREE = 0, ST_MOUNTING, ST_MOUNTED, ST_EJECTED, ST_NATIVE, ST_FAILED };

typedef struct {
    enum dstate st;
    char key[160];        /* kimlik: birim UUID / ortam UUID / yol+boyut */
    char bsd[32];         /* yalnızca o anki işlem için */
    char name[128];       /* görünen ad (temizlenmiş) */
    char mnt[PATH_MAX];
    pid_t pid;
    int  out_fd;          /* ntfs-3g stderr okuma ucu */
    int  saw_fallback_ro; /* ntfs-3g "Falling back to read-only" */
    int  saw_mounted;
    int  io_errors;
    char last_err[256];
    dispatch_source_t rd, proc;
    time_t started;
    int gone;             /* disk fiziksel olarak çıkarıldı */
    int pull_notified;    /* 'çıkarılmadan çekildi' bildirimi verildi */
    char obuf[4096];      /* ntfs-3g çıktı satır tamponu */
    size_t oused;
} disk_t;

static disk_t g_disks[MAX_DISKS];
static DASessionRef g_session;

static disk_t *find_by_key(const char *key) {
    for (int i = 0; i < MAX_DISKS; i++) if (g_disks[i].st != ST_FREE && !strcmp(g_disks[i].key, key)) return &g_disks[i];
    return NULL;
}
static disk_t *alloc_disk(const char *key) {
    for (int i = 0; i < MAX_DISKS; i++) if (g_disks[i].st == ST_FREE) {
        memset(&g_disks[i], 0, sizeof(disk_t)); g_disks[i].out_fd = -1; g_disks[i].pid = -1;
        snprintf(g_disks[i].key, sizeof g_disks[i].key, "%s", key);
        return &g_disks[i];
    }
    return NULL;
}

typedef struct {
    int ok;          /* harici NTFS bölümü mü */
    int mounted;
    char bsd[32], key[160], name[128], kind[32];
} info_t;

static void disk_info(DADiskRef disk, info_t *in) {
    memset(in, 0, sizeof *in);
    const char *b = DADiskGetBSDName(disk);
    if (!valid_bsd(b)) return;
    snprintf(in->bsd, sizeof in->bsd, "%s", b);
    CFDictionaryRef d = DADiskCopyDescription(disk);
    if (!d) return;
    CFBooleanRef internal = CFDictionaryGetValue(d, kDADiskDescriptionDeviceInternalKey);
    CFBooleanRef whole = CFDictionaryGetValue(d, kDADiskDescriptionMediaWholeKey);
    cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeKindKey), in->kind, sizeof in->kind);
    char vname[256] = "";
    cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeNameKey), vname, sizeof vname);
    sanitize_name(vname, in->name, sizeof in->name);
    in->mounted = CFDictionaryGetValue(d, kDADiskDescriptionVolumePathKey) != NULL;
    char u[64] = "";
    if (uuid_str(CFDictionaryGetValue(d, kDADiskDescriptionVolumeUUIDKey), u, sizeof u))
        snprintf(in->key, sizeof in->key, "vol:%s", u);
    else if (uuid_str(CFDictionaryGetValue(d, kDADiskDescriptionMediaUUIDKey), u, sizeof u))
        snprintf(in->key, sizeof in->key, "media:%s", u);
    else {
        char path[128] = ""; long long size = 0;
        cfstr(CFDictionaryGetValue(d, kDADiskDescriptionDevicePathKey), path, sizeof path);
        CFNumberRef n = CFDictionaryGetValue(d, kDADiskDescriptionMediaSizeKey);
        if (n) CFNumberGetValue(n, kCFNumberLongLongType, &size);
        snprintf(in->key, sizeof in->key, "yol:%s|%lld|%s", path, size, in->bsd);
    }
    in->ok = internal && !CFBooleanGetValue(internal) && (!whole || !CFBooleanGetValue(whole))
             && !strcmp(in->kind, "ntfs");
    CFRelease(d);
}

/* ---------- ntfs-3g çıktısının yorumlanması ---------- */

static const char *explain(const char *line) {
    if (strstr(line, "Operation not permitted")) return "macOS izin vermedi (Tam Disk Erişimi izni kapalı olabilir)";
    if (strstr(line, "File system extension not enabled")) return "macFUSE (local) uzantısı Sistem Ayarları'nda kapalı";
    if (strstr(line, "not found") && strstr(line, "extension")) return "macFUSE dosya sistemi uzantısı bulunamadı";
    if (strstr(line, "Device not configured")) return "disk bağlantısı koptu";
    if (strstr(line, "Input/output error")) return "diske okuma/yazma hatası";
    if (strstr(line, "hibernat") || strstr(line, "Hibernat")) return "Windows hazırda bekletme verisi var";
    return NULL;
}

static void handle_line(disk_t *dk, const char *line) {
    logf_("[%s] ntfs-3g: %s", dk->name, line);
    if (strstr(line, "Falling back to read-only mount")) dk->saw_fallback_ro = 1;
    if (!strncmp(line, "Mounted ", 8)) dk->saw_mounted = 1;
    int is_io = strstr(line, "Input/output error") || strstr(line, "Error writing") ||
                strstr(line, "Failed to sync") || strstr(line, "inconsistent state");
    if (is_io && dk->st == ST_MOUNTED && !dk->gone && !dk->pull_notified && !strstr(line, "Device not configured")) {
        if (dk->io_errors++ == 0)
            notify_user("“%s”: yazma/okuma hatası algılandı. Açık dosyaları kaydedip diski Finder'dan çıkar; Windows'ta chkdsk ile kontrol et. Ayrıntı: " LOG_FILE, dk->name);
    }
    const char *e = explain(line);
    if (e) snprintf(dk->last_err, sizeof dk->last_err, "%s", e);
    else if ((strstr(line, "rror") || strstr(line, "ailed")) && !dk->last_err[0])
        snprintf(dk->last_err, sizeof dk->last_err, "%.200s", line);
}

/* ---------- macOS'un kendi salt-okunur bağlaması (yedek yol) ---------- */

static void native_mount_cb(DADiskRef disk, DADissenterRef dis, void *ctx) {
    disk_t *dk = ctx;
    if (dis && disk) {
        /* macOS kendi isteğiyle aynı anda bağlamış olabilir: gerçekten bağlı mı, ona bak */
        CFDictionaryRef d = DADiskCopyDescription(disk);
        if (d) {
            if (CFDictionaryGetValue(d, kDADiskDescriptionVolumePathKey)) dis = NULL;
            CFRelease(d);
        }
    }
    if (!dis) {
        dk->st = ST_NATIVE;
        notify_user("“%s” yazılabilir bağlanamadı (%s). macOS'un salt-okunur desteğiyle açıldı. Ayrıntı: " LOG_FILE,
                    dk->name, dk->last_err[0] ? dk->last_err : "bilinmeyen sebep");
    } else {
        dk->st = ST_FAILED;
        notify_user("“%s” bağlanamadı (%s). Disk bağlı değil. Ayrıntı: " LOG_FILE,
                    dk->name, dk->last_err[0] ? dk->last_err : "bilinmeyen sebep");
    }
}

static void fallback_native(disk_t *dk) {
    DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, g_session, dk->bsd);
    if (!disk) { dk->st = ST_FAILED; notify_user("“%s” bağlanamadı ve disk artık görünmüyor.", dk->name); return; }
    info_t in; disk_info(disk, &in);
    if (!in.ok || strcmp(in.key, dk->key)) { CFRelease(disk); dk->st = ST_FAILED; logf_("[%s] yedek bağlama iptal: disk değişmiş", dk->name); return; }
    dk->st = ST_NATIVE; /* onay geri çağrısı bu duruma izin verir */
    logf_("[%s] macOS salt-okunur bağlama deneniyor (%s)", dk->name, dk->bsd);
    DADiskMount(disk, NULL, kDADiskMountOptionDefault, native_mount_cb, dk);
    CFRelease(disk);
}

/* ---------- ntfs-3g bitince ---------- */

static void child_exited(disk_t *dk) {
    int status = 0;
    waitpid(dk->pid, &status, 0);
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    logf_("[%s] ntfs-3g bitti, çıkış kodu %d (durum %d)", dk->name, code, (int)dk->st);
    if (dk->rd) { dispatch_source_cancel(dk->rd); dk->rd = NULL; }
    if (dk->proc) { dispatch_source_cancel(dk->proc); dk->proc = NULL; }
    dk->pid = -1;
    if (dk->mnt[0] && !is_mountpoint(dk->mnt)) rmdir(dk->mnt);

    if (dk->st == ST_MOUNTING) {
        /* bağlanamadı -> macOS salt-okunur denemesi */
        fallback_native(dk);
    } else if (dk->st == ST_MOUNTED) {
        char devp[48]; snprintf(devp, sizeof devp, "/dev/%s", dk->bsd);
        int pulled = dk->gone || access(devp, F_OK) != 0;
        if (pulled) {
            dk->st = ST_EJECTED;
            if (!dk->pull_notified)
                notify_user("“%s” Finder'dan çıkarılmadan çekildi. Açık dosyalar kaydedilmemiş olabilir; diski yeniden takmadan önce Windows'ta chkdsk ile kontrol et.", dk->name);
            logf_("[%s] disk çekildiği için ntfs-3g kapandı", dk->name);
        } else if (code == 0 && dk->io_errors == 0) {
            dk->st = ST_EJECTED; /* çıkarıldı: fiziksel çıkar-tak olana kadar yeniden bağlanmaz */
            logf_("[%s] düzgün çıkarıldı", dk->name);
        } else {
            dk->st = ST_EJECTED;
            notify_user("“%s” beklenmedik şekilde ayrıldı (%s). Finder'dan çıkarılmadan çekilmiş olabilir; Windows'ta chkdsk ile kontrol et. Ayrıntı: " LOG_FILE,
                        dk->name, dk->last_err[0] ? dk->last_err : "bilinmeyen sebep");
        }
    }
    if (dk->gone) { memset(dk, 0, sizeof *dk); dk->st = ST_FREE; dk->pid = -1; dk->out_fd = -1; }
}

/* ---------- yazılabilir bağlama ---------- */

static void consider(DADiskRef disk);

static void unmount_cb(DADiskRef disk, DADissenterRef dis, void *ctx) {
    disk_t *dk = ctx;
    if (dis) {
        dk->st = ST_NATIVE;
        notify_user("“%s” macOS tarafından salt-okunur açık ve zorlamadan ayrılamadı (kullanımda olabilir). Dokunulmadı; salt-okunur kaldı.", dk->name);
        return;
    }
    logf_("[%s] macOS bağlantısı ayrıldı", dk->name);
    dk->st = ST_FREE; dk->key[0] = 0;
    CFRetain(disk);
    dispatch_async(dispatch_get_main_queue(), ^{ consider(disk); CFRelease(disk); });
}

static void start_mount(DADiskRef disk, disk_t *dk, const info_t *in) {
    uid_t uid; gid_t gid; char uname[256];
    if (!console_user(&uid, &gid, uname, sizeof uname)) {
        logf_("[%s] giriş yapmış kullanıcı yok; yazılabilir bağlama yapılmıyor", dk->name);
        snprintf(dk->last_err, sizeof dk->last_err, "giriş yapmış kullanıcı yok");
        dk->st = ST_MOUNTING; fallback_native(dk); return;
    }
    struct passwd *pw = getpwnam(uname);
    if (!pw || pw->pw_uid != uid || pw->pw_uid == 0) { logf_("[%s] kullanıcı doğrulanamadı", dk->name); dk->st = ST_FAILED; return; }

    /* macOS bağlamışsa zorlamadan ayır (senkron değil; ayrılınca yeniden çağrılır) */
    if (in->mounted) {
        logf_("[%s] macOS bağlantısı var, zorlamadan ayırma isteniyor", dk->name);
        dk->st = ST_MOUNTING; /* ayrılırken gelen olaylar yok sayılsın */
        DADiskUnmount(disk, kDADiskUnmountOptionDefault, unmount_cb, dk);
        return;
    }

    /* bağlama noktası: /Volumes/<ad>[ n] */
    char base[PATH_MAX];
    snprintf(base, sizeof base, "/Volumes/%s", dk->name);
    snprintf(dk->mnt, sizeof dk->mnt, "%s", base);
    for (int n = 2; n < 100; n++) {
        struct stat st;
        if (lstat(dk->mnt, &st) != 0) break;
        snprintf(dk->mnt, sizeof dk->mnt, "%s %d", base, n);
    }
    if (mkdir(dk->mnt, 0755) != 0) { logf_("[%s] bağlama noktası oluşturulamadı: %s", dk->name, strerror(errno)); dk->st = ST_FAILED; return; }
    if (chown(dk->mnt, uid, gid) != 0) { rmdir(dk->mnt); dk->st = ST_FAILED; return; }

    char dev[48]; snprintf(dev, sizeof dev, "/dev/%s", dk->bsd);
    struct stat st;
    if (lstat(dev, &st) != 0 || !S_ISBLK(st.st_mode)) { rmdir(dk->mnt); dk->st = ST_FAILED; return; }
    int fd = open(dev, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        snprintf(dk->last_err, sizeof dk->last_err, "%s", errno == EPERM ? "macOS izin vermedi (Tam Disk Erişimi izni kapalı olabilir)" : strerror(errno));
        logf_("[%s] %s açılamadı: %s", dk->name, dev, strerror(errno));
        rmdir(dk->mnt); dk->st = ST_MOUNTING; fallback_native(dk); return;
    }
    struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET };
    if (fcntl(fd, F_GETLK, &fl) == 0 && fl.l_type != F_UNLCK) {
        logf_("[%s] aygıt zaten başka bir ntfs-3g tarafından kullanılıyor (pid %d); dokunulmadı", dk->name, (int)fl.l_pid);
        close(fd); rmdir(dk->mnt); dk->st = ST_MOUNTED; dk->pid = -1; return;
    }
    int pfd[2];
    if (pipe(pfd) != 0) { close(fd); rmdir(dk->mnt); dk->st = ST_FAILED; return; }

    /* seçenekler: volname temizlenmiş addan (virgül/eşittir yok) */
    char opts[512];
    snprintf(opts, sizeof opts,
             "no_detach,norecover,backend=fskit,local,volname=%s,umask=022,streams_interface=openxattr", dk->name);

    pid_t pid = fork();
    if (pid < 0) { close(fd); close(pfd[0]); close(pfd[1]); rmdir(dk->mnt); dk->st = ST_FAILED; return; }
    if (pid == 0) {
        /* alt süreç: fd 3 = aygıt, 1/2 = boru; yetki kullanıcıya düşer */
        int devnull = open("/dev/null", O_RDONLY);
        if (devnull >= 0) dup2(devnull, 0);
        dup2(pfd[1], 1); dup2(pfd[1], 2);
        if (fd != 3) { dup2(fd, 3); }
        fcntl(3, F_SETFD, 0);
        for (int i = 4; i < 1024; i++) close(i);
        if (initgroups(pw->pw_name, (int)gid) != 0 || setgid(gid) != 0 || setuid(uid) != 0) _exit(71);
        if (setuid(0) == 0 || geteuid() == 0) _exit(72);
        char home[PATH_MAX + 8], user[300], logname[300];
        snprintf(home, sizeof home, "HOME=%s", pw->pw_dir);
        snprintf(user, sizeof user, "USER=%s", pw->pw_name);
        snprintf(logname, sizeof logname, "LOGNAME=%s", pw->pw_name);
        char *env[] = { "PATH=/usr/bin:/bin:/usr/sbin:/sbin", home, user, logname, NULL };
        char *args[] = { NTFS3G, "/dev/fd/3", dk->mnt, "-o", opts, NULL };
        execve(NTFS3G, args, env);
        _exit(73);
    }
    close(fd); close(pfd[1]);
    dk->pid = pid; dk->out_fd = pfd[0]; dk->st = ST_MOUNTING; dk->started = time(NULL);
    logf_("[%s] ntfs-3g başlatıldı: %s -> %s, kullanıcı %s (pid %d)", dk->name, dev, dk->mnt, uname, (int)pid);

    /* stderr satırlarını oku */
    dk->rd = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)dk->out_fd, 0, dispatch_get_main_queue());
    dk->oused = 0;
    dispatch_source_set_event_handler(dk->rd, ^{
        ssize_t n = read(dk->out_fd, dk->obuf + dk->oused, sizeof dk->obuf - 1 - dk->oused);
        if (n <= 0) { dispatch_source_cancel(dk->rd); return; }
        dk->oused += (size_t)n; dk->obuf[dk->oused] = 0;
        char *start = dk->obuf, *nl;
        while ((nl = strchr(start, '\n'))) { *nl = 0; if (*start) handle_line(dk, start); start = nl + 1; }
        dk->oused = strlen(start); memmove(dk->obuf, start, dk->oused + 1);
        if (dk->oused >= sizeof dk->obuf - 1) { handle_line(dk, dk->obuf); dk->oused = 0; }
    });
    int rfd = dk->out_fd;
    dispatch_source_set_cancel_handler(dk->rd, ^{ close(rfd); });
    dispatch_resume(dk->rd);

    dk->proc = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
    dispatch_source_set_event_handler(dk->proc, ^{ child_exited(dk); });
    dispatch_resume(dk->proc);

    /* bağlandı mı? sınırlı süre bekle (yalnızca bu bağlama için, yoklama döngüsü değil) */
    for (int i = 1; i <= MOUNT_WAIT; i++) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)i * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            if (dk->st != ST_MOUNTING || dk->pid != pid) return;
            if (is_mountpoint(dk->mnt)) {
                dk->st = ST_MOUNTED;
                if (dk->saw_fallback_ro)
                    notify_user("“%s” salt-okunur bağlandı: Windows'ta tam kapatılmamış ya da hazırda bekletmede. Diski Windows'ta tamamen kapatıp (Hızlı Başlatma kapalı) tekrar tak.", dk->name);
                else
                    notify_user("“%s” yazılabilir olarak bağlandı. İşin bitince Finder'dan çıkar.", dk->name);
            } else if (i == MOUNT_WAIT) {
                logf_("[%s] %d sn içinde bağlanmadı; ntfs-3g durduruluyor", dk->name, MOUNT_WAIT);
                snprintf(dk->last_err, sizeof dk->last_err, "zaman aşımı");
                kill(pid, SIGTERM); /* ntfs-3g henüz bağlamadı; normal sonlandırma */
            }
        });
    }
}

/* ---------- DiskArbitration geri çağrıları ---------- */

static void consider(DADiskRef disk) {
    info_t in; disk_info(disk, &in);
    if (!in.ok) return;
    disk_t *dk = find_by_key(in.key);
    if (dk) {
        /* tekrarlanan olaylar: yalnızca durum serbestse işlem yap */
        if (dk->st == ST_EJECTED || dk->st == ST_MOUNTING || dk->st == ST_MOUNTED || dk->st == ST_NATIVE || dk->st == ST_FAILED) return;
    } else {
        dk = alloc_disk(in.key);
        if (!dk) { logf_("çok fazla disk; %s yok sayıldı", in.bsd); return; }
    }
    snprintf(dk->bsd, sizeof dk->bsd, "%s", in.bsd);
    snprintf(dk->name, sizeof dk->name, "%s", in.name);
    logf_("[%s] harici NTFS bölümü: %s (kimlik %s)%s", dk->name, in.bsd, in.key, in.mounted ? ", macOS bağlamış" : "");
    start_mount(disk, dk, &in);
}

static DADissenterRef mount_approval(DADiskRef disk, void *ctx) {
    (void)ctx;
    info_t in; disk_info(disk, &in);
    if (!in.ok) return NULL; /* NTFS değil / dahili: macOS normal davranır */
    disk_t *dk = find_by_key(in.key);
    if (dk && dk->st == ST_NATIVE) return NULL; /* yedek salt-okunur bağlamaya izin */
    if (dk && dk->st == ST_FAILED) return NULL;
    logf_("[%s] macOS'un bağlama isteği reddedildi (%s)", in.name, in.bsd);
    if (!dk || dk->st == ST_FREE) {
        /* ilk kez görülüyor: biz bağlayacağız */
        CFRetain(disk);
        dispatch_async(dispatch_get_main_queue(), ^{ consider(disk); CFRelease(disk); });
    }
    return DADissenterCreate(kCFAllocatorDefault, kDAReturnBusy, CFSTR("NoKext NTFS"));
}

static void disk_appeared(DADiskRef disk, void *ctx) { (void)ctx; consider(disk); }

static void disk_changed(DADiskRef disk, CFArrayRef keys, void *ctx) {
    (void)keys; (void)ctx;
    consider(disk); /* macOS bağlantısı zorlamadan ayrıldığında burada devam edilir */
}

static void disk_disappeared(DADiskRef disk, void *ctx) {
    (void)ctx;
    const char *b = DADiskGetBSDName(disk);
    if (!b) return;
    for (int i = 0; i < MAX_DISKS; i++) {
        disk_t *dk = &g_disks[i];
        if (dk->st == ST_FREE || strcmp(dk->bsd, b)) continue;
        logf_("[%s] disk çıkarıldı (%s)", dk->name, b);
        if (dk->pid > 0) {
            if (dk->st == ST_MOUNTED && !dk->pull_notified) {
                dk->pull_notified = 1;
                notify_user("“%s” Finder'dan çıkarılmadan çekildi. Açık dosyalar kaydedilmemiş olabilir; diski yeniden takmadan önce Windows'ta chkdsk ile kontrol et.", dk->name);
            }
            if (dk->mnt[0]) {
                /* aygıt yok; Finder'da kalan bağlantıyı ZORLAMADAN kaldır (unmount bayrakları 0) */
                char *mnt = strdup(dk->mnt), *nm = strdup(dk->name);
                if (mnt && nm) dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                    int r = unmount(mnt, 0);
                    int e = errno;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        if (r == 0) logf_("[%s] çekilen diskin bağlantısı zorlamadan kaldırıldı", nm);
                        else {
                            logf_("[%s] çekilen diskin bağlantısı kaldırılamadı: %s", nm, strerror(e));
                            notify_user("“%s” Finder'da görünmeye devam ediyor (kullanımda). Açık pencereleri kapatıp Finder'dan ⏏ ile kaldır.", nm);
                        }
                        free(mnt); free(nm);
                    });
                });
                else { free(mnt); free(nm); }
            }
            dk->gone = 1;            /* ntfs-3g bitince yuva boşalır */
            dk->key[0] = 0;          /* yeniden takılan disk bu yuvayla eşleşmesin */
            if (!dk->last_err[0]) snprintf(dk->last_err, sizeof dk->last_err, "disk çıkarıldı");
        } else {
            memset(dk, 0, sizeof *dk); dk->st = ST_FREE; dk->pid = -1; dk->out_fd = -1;
        }
    }
}

static void prepare_dirs(void) {
    umask(022);
    mkdir(LOG_DIR, 0755); chown(LOG_DIR, 0, 0); chmod(LOG_DIR, 0755);
    mkdir(NOTIFY_DIR, 0755); chown(NOTIFY_DIR, 0, 0); chmod(NOTIFY_DIR, 0755);
    int fd = open(NOTIFY_FILE, O_WRONLY | O_CREAT | O_NOFOLLOW, 0644);
    if (fd >= 0) { fchown(fd, 0, 0); fchmod(fd, 0644); close(fd); }
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc != 1) { fprintf(stderr, "ntfsotomatikd argüman kabul etmez\n"); return 2; }
    if (getuid() != 0) { fprintf(stderr, "root olarak çalışmalı\n"); return 2; }
    signal(SIGPIPE, SIG_IGN);
    prepare_dirs();
    logf_("ntfsotomatikd başladı (pid %d)", (int)getpid());

    g_session = DASessionCreate(kCFAllocatorDefault);
    if (!g_session) { logf_("DASession oluşturulamadı"); return 3; }
    DASessionScheduleWithRunLoop(g_session, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    DARegisterDiskMountApprovalCallback(g_session, NULL, mount_approval, NULL);
    DARegisterDiskAppearedCallback(g_session, kDADiskDescriptionMatchVolumeMountable, disk_appeared, NULL);
    DARegisterDiskDescriptionChangedCallback(g_session, kDADiskDescriptionMatchVolumeMountable,
                                             kDADiskDescriptionWatchVolumePath, disk_changed, NULL);
    DARegisterDiskDisappearedCallback(g_session, NULL, disk_disappeared, NULL);

    dispatch_source_t term = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
    signal(SIGTERM, SIG_IGN);
    dispatch_source_set_event_handler(term, ^{
        logf_("ntfsotomatikd durduruluyor; bağlı diskler ntfs-3g tarafından açık kalır (zorla ayırma yok)");
        exit(0);
    });
    dispatch_resume(term);

    CFRunLoopRun();
    return 0;
}
