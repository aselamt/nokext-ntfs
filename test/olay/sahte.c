// SPDX-License-Identifier: GPL-2.0-or-later
/* sahte.h'deki sahte çerçevelerin gerçeklemesi. Bu dosya sistem çağrılarını GERÇEK adlarıyla kullanır. */
#include "sahte.h"
#undef open
#undef stat
#undef lstat
#undef statfs
#undef mkdir
#undef rmdir
#undef chown
#undef access
#undef rename
#undef fork
#undef waitpid
#undef kill
#undef unmount
#undef fcntl
#undef getpwnam
#undef main

/* ---------- blok çalışma zamanı (yalnızca Apple dışı; macOS'ta libSystem'de var) ---------- */
#ifndef __APPLE__
void *_NSConcreteStackBlock[32], *_NSConcreteGlobalBlock[32], *_NSConcreteMallocBlock[32];
struct blok_tanim { unsigned long ayrilmis, boyut; void (*kopya)(void *, const void *); void (*at)(const void *); };
struct blok { void *isa; int bayrak; int ayrilmis; void (*cagir)(void *); struct blok_tanim *tanim; };
#define B_KOPYA_AT  (1 << 25)
#define B_GLOBAL    (1 << 28)
#define B_YIGIN     (1 << 24)
void *_Block_copy(const void *p) {
    const struct blok *b = p;
    if (!b) return NULL;
    if (b->bayrak & (B_GLOBAL | B_YIGIN)) return (void *)p;   /* sızıntı kabul: testler kısa */
    struct blok *y = malloc(b->tanim->boyut);
    memcpy(y, b, b->tanim->boyut);
    y->isa = _NSConcreteMallocBlock; y->bayrak |= B_YIGIN;
    if (b->bayrak & B_KOPYA_AT) b->tanim->kopya(y, b);
    return y;
}
void _Block_release(const void *p) { (void)p; }
void _Block_object_assign(void *hedef, const void *kaynak, int bayrak) {
    if ((bayrak & 7) == 7) *(void **)hedef = _Block_copy(kaynak);
    else *(const void **)hedef = kaynak;
}
void _Block_object_dispose(const void *p, int bayrak) { (void)p; (void)bayrak; }
#endif

/* ---------- CF nesneleri ---------- */
struct sahte_nesne {
    int tip;
    char s[160];
    long long n;
    int b;
    int adet; const void *k[16]; const void *v[16];
    int disk;             /* SAHTE_DISK: kayıt sırası */
};
static sahte_nesne *yeni(int tip) { sahte_nesne *o = calloc(1, sizeof *o); o->tip = tip; return o; }
static sahte_nesne *metin(const char *s) { sahte_nesne *o = yeni(SAHTE_STR); snprintf(o->s, sizeof o->s, "%s", s); return o; }
CFStringRef sahte_cfstr(const char *s) { return metin(s); }

CFTypeID CFGetTypeID(CFTypeRef o) { return o ? (CFTypeID)((const sahte_nesne *)o)->tip : 0; }
CFTypeID CFStringGetTypeID(void) { return SAHTE_STR; }
CFTypeID CFUUIDGetTypeID(void) { return SAHTE_UUID; }
int CFStringGetCString(CFStringRef s, char *out, CFIndex len, CFStringEncoding e) {
    (void)e; if (!s || (size_t)len <= strlen(s->s)) return 0;
    strcpy(out, s->s); return 1;
}
CFStringRef CFUUIDCreateString(CFAllocatorRef a, CFUUIDRef u) { (void)a; return metin(u->s); }
void CFRelease(CFTypeRef o) { (void)o; }
CFTypeRef CFRetain(CFTypeRef o) { return o; }
const void *CFDictionaryGetValue(CFDictionaryRef d, const void *k) {
    for (int i = 0; i < d->adet; i++) if (d->k[i] == k) return d->v[i];
    return NULL;
}
int CFBooleanGetValue(CFBooleanRef b) { return b->b; }
int CFNumberGetValue(CFNumberRef n, CFNumberType t, void *out) { (void)t; *(long long *)out = n->n; return 1; }
CFRunLoopRef CFRunLoopGetMain(void) { return NULL; }
void CFRunLoopRun(void) {}

/* ---------- DiskArbitration ---------- */
CFStringRef kDADiskDescriptionDeviceInternalKey, kDADiskDescriptionMediaWholeKey, kDADiskDescriptionVolumeKindKey,
    kDADiskDescriptionVolumeNameKey, kDADiskDescriptionVolumePathKey, kDADiskDescriptionVolumeUUIDKey,
    kDADiskDescriptionMediaUUIDKey, kDADiskDescriptionDevicePathKey, kDADiskDescriptionMediaSizeKey;

#define MAKS 64
static sahte_disk g_dunya[MAKS];   /* her takma yeni kayıt; hiç yeniden kullanılmaz */
static int g_dunya_n;

sahte_disk *sahte_disk_tak(const char *bsd, const char *uuid, const char *ad, int macos_bagli) {
    sahte_disk *d = &g_dunya[g_dunya_n++];
    memset(d, 0, sizeof *d);
    d->var = 1; snprintf(d->bsd, sizeof d->bsd, "%s", bsd); snprintf(d->uuid, sizeof d->uuid, "%s", uuid);
    snprintf(d->ad, sizeof d->ad, "%s", ad); snprintf(d->tur, sizeof d->tur, "ntfs"); d->macos_bagli = macos_bagli;
    return d;
}
static sahte_disk *takili(const char *bsd) {
    for (int i = g_dunya_n - 1; i >= 0; i--) if (g_dunya[i].var && !strcmp(g_dunya[i].bsd, bsd)) return &g_dunya[i];
    return NULL;
}
void sahte_disk_cek(const char *bsd) { sahte_disk *d = takili(bsd); if (d) d->var = 0; }
static DADiskRef ref_of(int i) { sahte_nesne *o = yeni(SAHTE_DISK); o->disk = i; snprintf(o->s, sizeof o->s, "%s", g_dunya[i].bsd); return o; }
DADiskRef sahte_disk_ref(const char *bsd) { sahte_disk *d = takili(bsd); return d ? ref_of((int)(d - g_dunya)) : NULL; }
DADiskRef sahte_disk_ref_eski(const char *bsd) {
    for (int i = g_dunya_n - 1; i >= 0; i--) if (!strcmp(g_dunya[i].bsd, bsd)) return ref_of(i);
    return NULL;
}
const char *DADiskGetBSDName(DADiskRef d) { return d ? d->s : NULL; }
static void ekle(sahte_nesne *s, CFStringRef k, const void *v) { if (v) { s->k[s->adet] = k; s->v[s->adet++] = v; } }
CFDictionaryRef DADiskCopyDescription(DADiskRef ref) {
    sahte_disk *d = &g_dunya[ref->disk];   /* çekilmiş diskin de son açıklaması döner (DA önbelleği gibi) */
    sahte_nesne *s = yeni(SAHTE_SOZLUK);
    sahte_nesne *ic = yeni(SAHTE_BOOL); ic->b = d->dahili;
    sahte_nesne *tum = yeni(SAHTE_BOOL); tum->b = 0;
    ekle(s, kDADiskDescriptionDeviceInternalKey, ic);
    ekle(s, kDADiskDescriptionMediaWholeKey, tum);
    ekle(s, kDADiskDescriptionVolumeKindKey, metin(d->tur));
    ekle(s, kDADiskDescriptionVolumeNameKey, metin(d->ad));
    if (d->macos_bagli) ekle(s, kDADiskDescriptionVolumePathKey, metin("file:///Volumes/x"));
    if (d->uuid[0]) { sahte_nesne *u = yeni(SAHTE_UUID); snprintf(u->s, sizeof u->s, "%s", d->uuid); ekle(s, kDADiskDescriptionVolumeUUIDKey, u); }
    return s;
}
DADiskRef DADiskCreateFromBSDName(CFAllocatorRef a, DASessionRef s, const char *bsd) { (void)a; (void)s; return sahte_disk_ref(bsd); }

static sahte_da_islem g_islem[MAKS]; static int g_islem_n;
static int g_islem_disk[MAKS];   /* işlemin hangi takma kaydına ait olduğu */
static void islem_ekle(int tur, DADiskRef d, DADiskMountCallback cb, void *ctx) {
    sahte_da_islem *x = &g_islem[g_islem_n++];
    x->tur = tur; snprintf(x->bsd, sizeof x->bsd, "%s", d->s); x->cb = cb; x->ctx = ctx; x->bitti = 0;
    g_islem_disk[g_islem_n - 1] = d->disk;
}
void DADiskMount(DADiskRef d, const void *p, int o, DADiskMountCallback cb, void *ctx) { (void)p; (void)o; islem_ekle(1, d, cb, ctx); }
void DADiskUnmount(DADiskRef d, int o, DADiskMountCallback cb, void *ctx) { (void)o; islem_ekle(2, d, cb, ctx); }
sahte_da_islem *sahte_da_bekleyen(int tur, const char *bsd) {
    for (int i = 0; i < g_islem_n; i++) if (!g_islem[i].bitti && g_islem[i].tur == tur && !strcmp(g_islem[i].bsd, bsd)) return &g_islem[i];
    return NULL;
}
void sahte_da_bitir(sahte_da_islem *x, int reddet) {
    int i = (int)(x - g_islem);
    sahte_disk *d = &g_dunya[g_islem_disk[i]];
    x->bitti = 1;
    if (!reddet && d->var) d->macos_bagli = (x->tur == 1);
    x->cb(ref_of(g_islem_disk[i]), reddet ? (DADissenterRef)yeni(SAHTE_RET) : NULL, x->ctx);
}
DADissenterRef DADissenterCreate(CFAllocatorRef a, DAReturn r, CFStringRef s) { (void)a; (void)r; (void)s; return yeni(SAHTE_RET); }
DASessionRef DASessionCreate(CFAllocatorRef a) { (void)a; return yeni(SAHTE_OTURUM); }
void DASessionScheduleWithRunLoop(DASessionRef s, CFRunLoopRef r, CFStringRef m) { (void)s; (void)r; (void)m; }
void DARegisterDiskMountApprovalCallback(DASessionRef s, CFDictionaryRef m, DADiskMountApprovalCallback c, void *x) { (void)s; (void)m; (void)c; (void)x; }
void DARegisterDiskAppearedCallback(DASessionRef s, CFDictionaryRef m, DADiskAppearedCallback c, void *x) { (void)s; (void)m; (void)c; (void)x; }
void DARegisterDiskDescriptionChangedCallback(DASessionRef s, CFDictionaryRef m, CFArrayRef w, DADiskDescriptionChangedCallback c, void *x) { (void)s; (void)m; (void)w; (void)c; (void)x; }
void DARegisterDiskDisappearedCallback(DASessionRef s, CFDictionaryRef m, DADiskDisappearedCallback c, void *x) { (void)s; (void)m; (void)c; (void)x; }

CFStringRef SCDynamicStoreCopyConsoleUser(void *s, uid_t *u, gid_t *g) { (void)s; *u = 501; *g = 20; return metin("deneme"); }

/* ---------- libdispatch ---------- */
struct sahte_kaynak { dispatch_source_type_t tur; uintptr_t tutamak; dispatch_block_t olay, iptal; int iptal_edildi; };
static int g_ana, g_genel;
dispatch_queue_t dispatch_get_main_queue_(void) { return &g_ana; }
dispatch_queue_t dispatch_get_global_queue(long a, unsigned long b) { (void)a; (void)b; return &g_genel; }
static dispatch_block_t g_kuyruk[4096]; static int g_kuyruk_bas, g_kuyruk_son;
void dispatch_async(dispatch_queue_t q, dispatch_block_t b) { (void)q; g_kuyruk[g_kuyruk_son++] = _Block_copy(b); }
void sahte_kuyrugu_calistir(void) { while (g_kuyruk_bas < g_kuyruk_son) g_kuyruk[g_kuyruk_bas++](); }
static uint64_t g_simdi;
static struct { uint64_t t; dispatch_block_t b; int bitti; } g_zaman[8192]; static int g_zaman_n;
dispatch_time_t dispatch_time(dispatch_time_t w, int64_t d) { (void)w; return (dispatch_time_t)d; }
void dispatch_after(dispatch_time_t w, dispatch_queue_t q, dispatch_block_t b) {
    (void)q; g_zaman[g_zaman_n].t = g_simdi + w; g_zaman[g_zaman_n].b = _Block_copy(b); g_zaman[g_zaman_n++].bitti = 0;
}
void sahte_saniye_ilerlet(int sn) {
    for (int s = 0; s < sn; s++) {
        g_simdi += NSEC_PER_SEC;
        for (int i = 0; i < g_zaman_n; i++) if (!g_zaman[i].bitti && g_zaman[i].t <= g_simdi) { g_zaman[i].bitti = 1; g_zaman[i].b(); }
        sahte_kuyrugu_calistir();
    }
}
static struct sahte_kaynak *g_kaynak[256]; static int g_kaynak_n;
dispatch_source_t dispatch_source_create(dispatch_source_type_t t, uintptr_t h, unsigned long m, dispatch_queue_t q) {
    (void)m; (void)q; struct sahte_kaynak *k = calloc(1, sizeof *k); k->tur = t; k->tutamak = h; g_kaynak[g_kaynak_n++] = k; return k;
}
void dispatch_source_set_event_handler(dispatch_source_t k, dispatch_block_t b) { k->olay = _Block_copy(b); }
void dispatch_source_set_cancel_handler(dispatch_source_t k, dispatch_block_t b) { k->iptal = _Block_copy(b); }
void dispatch_resume(dispatch_source_t k) { (void)k; }
void dispatch_source_cancel(dispatch_source_t k) {
    if (k->iptal_edildi) return;
    k->iptal_edildi = 1;
    if (k->iptal) dispatch_async(&g_ana, k->iptal);
}

/* ---------- sistem çağrıları ---------- */
static char g_klasor[1024];
static char g_dizinler[MAKS][1024]; static int g_dizin_n;
static struct { pid_t pid; char bsd[32]; char mnt[1024]; int bagli, bitti, kod; } g_ntfs3g[MAKS]; static int g_ntfs3g_n;
static char g_son_aygit[64], g_son_dizin[1024];

static const char *esle(const char *p, char *buf, size_t len) {
    if (!strncmp(p, "/Library/", 9)) { const char *b = strrchr(p, '/'); snprintf(buf, len, "%s%s", g_klasor, b); return buf; }
    return p;
}
static sahte_disk *aygit(const char *p) { return strncmp(p, "/dev/", 5) ? NULL : takili(p + 5); }

int sahte_open(const char *p, int f, ...) {
    va_list ap; va_start(ap, f); int m = va_arg(ap, int); va_end(ap);
    if (!strncmp(p, "/dev/disk", 9)) {
        sahte_disk *d = aygit(p);
        if (!d) { errno = ENOENT; return -1; }
        if (d->acilamaz) { errno = EPERM; return -1; }
        snprintf(g_son_aygit, sizeof g_son_aygit, "%s", d->bsd);
        return open("/dev/null", O_RDWR | O_CLOEXEC);
    }
    char b[1100]; return open(esle(p, b, sizeof b), f, m);
}
int sahte_stat(const char *p, struct stat *s) { char b[1100]; return stat(esle(p, b, sizeof b), s); }
int sahte_rename(const char *a, const char *c) { char b1[1100], b2[1100]; return rename(esle(a, b1, sizeof b1), esle(c, b2, sizeof b2)); }
static int dizin_bul(const char *p) { for (int i = 0; i < g_dizin_n; i++) if (!strcmp(g_dizinler[i], p)) return i; return -1; }
int sahte_lstat(const char *p, struct stat *s) {
    memset(s, 0, sizeof *s);
    if (aygit(p)) { s->st_mode = S_IFBLK | 0640; return 0; }
    if (dizin_bul(p) >= 0) { s->st_mode = S_IFDIR | 0755; return 0; }
    errno = ENOENT; return -1;
}
int sahte_statfs(const char *p, struct statfs *s) {
    memset(s, 0, sizeof *s);
    snprintf(s->f_mntonname, sizeof s->f_mntonname, "/");
    for (int i = 0; i < g_ntfs3g_n; i++)
        if (g_ntfs3g[i].bagli && !g_ntfs3g[i].bitti && !strcmp(g_ntfs3g[i].mnt, p)) snprintf(s->f_mntonname, sizeof s->f_mntonname, "%s", p);
    return 0;
}
int sahte_mkdir(const char *p, mode_t m) {
    (void)m;
    if (!strncmp(p, "/Library/", 9)) return 0;
    if (dizin_bul(p) >= 0) { errno = EEXIST; return -1; }
    snprintf(g_dizinler[g_dizin_n++], sizeof g_dizinler[0], "%s", p);
    snprintf(g_son_dizin, sizeof g_son_dizin, "%s", p);
    return 0;
}
int sahte_rmdir(const char *p) {
    int i = dizin_bul(p); if (i < 0) { errno = ENOENT; return -1; }
    g_dizinler[i][0] = 0; return 0;
}
int sahte_chown(const char *p, uid_t u, gid_t g) { (void)p; (void)u; (void)g; return 0; }
int sahte_access(const char *p, int m) { (void)m; if (aygit(p)) return 0; errno = ENOENT; return -1; }
pid_t sahte_fork(void) {
    pid_t pid = 5000 + g_ntfs3g_n;
    memset(&g_ntfs3g[g_ntfs3g_n], 0, sizeof g_ntfs3g[0]);
    g_ntfs3g[g_ntfs3g_n].pid = pid;
    snprintf(g_ntfs3g[g_ntfs3g_n].bsd, sizeof g_ntfs3g[0].bsd, "%s", g_son_aygit);
    snprintf(g_ntfs3g[g_ntfs3g_n].mnt, sizeof g_ntfs3g[0].mnt, "%s", g_son_dizin);
    g_ntfs3g_n++;
    return pid;
}
static int ntfs3g_bul(pid_t pid) { for (int i = 0; i < g_ntfs3g_n; i++) if (g_ntfs3g[i].pid == pid) return i; return -1; }
pid_t sahte_waitpid(pid_t pid, int *st, int o) {
    (void)o; int i = ntfs3g_bul(pid); if (i < 0) { errno = ECHILD; return -1; }
    if (st) *st = (g_ntfs3g[i].kod & 0xff) << 8;
    return pid;
}
int sahte_kill(pid_t pid, int s) { (void)pid; (void)s; return 0; }
int sahte_unmount(const char *p, int f) { (void)p; (void)f; return 0; }
int sahte_fcntl(int fd, int cmd, ...) {
    va_list ap; va_start(ap, cmd); void *a = va_arg(ap, void *); va_end(ap);
    if (cmd == F_GETLK) { ((struct flock *)a)->l_type = F_UNLCK; return 0; }
    return fcntl(fd, cmd, a);
}
struct passwd *sahte_getpwnam(const char *n) {
    static struct passwd pw; static char ad[64];
    snprintf(ad, sizeof ad, "%s", n);
    pw.pw_name = ad; pw.pw_uid = 501; pw.pw_gid = 20; pw.pw_dir = "/Users/deneme";
    return strcmp(n, "deneme") ? NULL : &pw;
}

int sahte_ntfs3g_sayisi(void) { int n = 0; for (int i = 0; i < g_ntfs3g_n; i++) if (!g_ntfs3g[i].bitti) n++; return n; }
pid_t sahte_ntfs3g_pid(const char *bsd) {
    for (int i = g_ntfs3g_n - 1; i >= 0; i--) if (!g_ntfs3g[i].bitti && !strcmp(g_ntfs3g[i].bsd, bsd)) return g_ntfs3g[i].pid;
    return -1;
}
void sahte_ntfs3g_baglandi(pid_t pid) { int i = ntfs3g_bul(pid); if (i >= 0) g_ntfs3g[i].bagli = 1; }
void sahte_ntfs3g_bitti(pid_t pid, int kod) {
    int i = ntfs3g_bul(pid); if (i < 0) return;
    g_ntfs3g[i].bitti = 1; g_ntfs3g[i].bagli = 0; g_ntfs3g[i].kod = kod;
    for (int k = 0; k < g_kaynak_n; k++)
        if (g_kaynak[k]->tur == DISPATCH_SOURCE_TYPE_PROC && g_kaynak[k]->tutamak == (uintptr_t)pid && !g_kaynak[k]->iptal_edildi && g_kaynak[k]->olay)
            dispatch_async(&g_ana, g_kaynak[k]->olay);
    sahte_kuyrugu_calistir();
}

int sahte_bildirim_var(const char *parca) {
    char p[1100]; snprintf(p, sizeof p, "%s/bildirim", g_klasor);
    FILE *f = fopen(p, "r"); if (!f) return 0;
    char satir[2048]; int var = 0;
    while (fgets(satir, sizeof satir, f)) if (strstr(satir, parca)) var = 1;
    fclose(f); return var;
}

void sahte_sifirla(const char *klasor) {
    snprintf(g_klasor, sizeof g_klasor, "%s", klasor);
    char p[1100];
    snprintf(p, sizeof p, "%s/bildirim", klasor); unlink(p);
    snprintf(p, sizeof p, "%s/ntfsotomatik.log", klasor); unlink(p);
    sahte_kuyrugu_calistir();
    g_dunya_n = 0; g_islem_n = 0; g_dizin_n = 0; g_ntfs3g_n = 0; g_zaman_n = 0; g_kaynak_n = 0;
    g_kuyruk_bas = g_kuyruk_son = 0; g_simdi = 0;
    if (!kDADiskDescriptionVolumeKindKey) {
        kDADiskDescriptionDeviceInternalKey = metin("DAMediaInternal"); kDADiskDescriptionMediaWholeKey = metin("DAMediaWhole");
        kDADiskDescriptionVolumeKindKey = metin("DAVolumeKind"); kDADiskDescriptionVolumeNameKey = metin("DAVolumeName");
        kDADiskDescriptionVolumePathKey = metin("DAVolumePath"); kDADiskDescriptionVolumeUUIDKey = metin("DAVolumeUUID");
        kDADiskDescriptionMediaUUIDKey = metin("DAMediaUUID"); kDADiskDescriptionDevicePathKey = metin("DADevicePath");
        kDADiskDescriptionMediaSizeKey = metin("DAMediaSize");
    }
}
