// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Sahte (mock) CoreFoundation / DiskArbitration / SystemConfiguration / libdispatch ve sistem çağrıları.
 * Amaç: src/ntfsotomatikd.c'yi DEĞİŞTİRMEDEN derleyip DiskArbitration olay sıralarını (disk çekme,
 * gecikmiş ayırma sonucu, klon UUID ...) gerçek disk ve root yetkisi olmadan sınamak.
 * Hiçbir gerçek aygıt açılmaz, süreç başlatılmaz, bağlama yapılmaz; dosyalar geçici klasöre gider.
 *
 * Bu başlık ntfsotomatikd.c'den ÖNCE yüklenir: önce kaynağın kullandığı tüm sistem başlıklarını açar
 * (sonradan tekrar açılmasınlar), sonra sistem çağrılarını sahte_* işlevlerine yönlendirir.
 */
#ifndef SAHTE_H
#define SAHTE_H

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
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>
#ifndef PATH_MAX
#include <limits.h>
#endif

/* ---------- bloklar (Linux'ta küçük bir çalışma zamanı sahte.c içinde) ---------- */
void *_Block_copy(const void *);
void _Block_release(const void *);

/* ---------- CoreFoundation ---------- */
typedef struct sahte_nesne sahte_nesne;
typedef const void *CFTypeRef;
typedef const sahte_nesne *CFStringRef, *CFDictionaryRef, *CFBooleanRef, *CFNumberRef, *CFUUIDRef, *CFArrayRef;
typedef const void *CFAllocatorRef;
typedef void *CFRunLoopRef;
typedef long CFIndex;
typedef unsigned long CFTypeID;
typedef unsigned int CFStringEncoding;
typedef int CFNumberType;
#define kCFAllocatorDefault ((CFAllocatorRef)0)
#define kCFStringEncodingUTF8 0x08000100u
#define kCFNumberLongLongType 11
#define kCFRunLoopDefaultMode ((CFStringRef)0)
enum { SAHTE_STR = 1, SAHTE_SOZLUK, SAHTE_BOOL, SAHTE_SAYI, SAHTE_UUID, SAHTE_DISK, SAHTE_RET, SAHTE_OTURUM };

CFTypeID CFGetTypeID(CFTypeRef);
CFTypeID CFStringGetTypeID(void);
CFTypeID CFUUIDGetTypeID(void);
int CFStringGetCString(CFStringRef, char *, CFIndex, CFStringEncoding);
CFStringRef CFUUIDCreateString(CFAllocatorRef, CFUUIDRef);
void CFRelease(CFTypeRef);
CFTypeRef CFRetain(CFTypeRef);
const void *CFDictionaryGetValue(CFDictionaryRef, const void *);
int CFBooleanGetValue(CFBooleanRef);
int CFNumberGetValue(CFNumberRef, CFNumberType, void *);
CFStringRef sahte_cfstr(const char *);
#define CFSTR(s) sahte_cfstr(s)
CFRunLoopRef CFRunLoopGetMain(void);
void CFRunLoopRun(void);

/* ---------- DiskArbitration ---------- */
typedef const sahte_nesne *DADiskRef, *DADissenterRef;
typedef sahte_nesne *DASessionRef;
typedef int DAReturn;
#define kDAReturnBusy 0xF8DA0002
#define kDADiskMountOptionDefault 0
#define kDADiskUnmountOptionDefault 0
typedef void (*DADiskMountCallback)(DADiskRef, DADissenterRef, void *);
typedef DADissenterRef (*DADiskMountApprovalCallback)(DADiskRef, void *);
typedef void (*DADiskAppearedCallback)(DADiskRef, void *);
typedef void (*DADiskDescriptionChangedCallback)(DADiskRef, CFArrayRef, void *);
typedef void (*DADiskDisappearedCallback)(DADiskRef, void *);
extern CFStringRef kDADiskDescriptionDeviceInternalKey, kDADiskDescriptionMediaWholeKey, kDADiskDescriptionVolumeKindKey,
    kDADiskDescriptionVolumeNameKey, kDADiskDescriptionVolumePathKey, kDADiskDescriptionVolumeUUIDKey,
    kDADiskDescriptionMediaUUIDKey, kDADiskDescriptionDevicePathKey, kDADiskDescriptionMediaSizeKey;
#define kDADiskDescriptionMatchVolumeMountable ((CFDictionaryRef)0)
#define kDADiskDescriptionWatchVolumePath ((CFArrayRef)0)
const char *DADiskGetBSDName(DADiskRef);
CFDictionaryRef DADiskCopyDescription(DADiskRef);
DADiskRef DADiskCreateFromBSDName(CFAllocatorRef, DASessionRef, const char *);
void DADiskMount(DADiskRef, const void *, int, DADiskMountCallback, void *);
void DADiskUnmount(DADiskRef, int, DADiskMountCallback, void *);
DADissenterRef DADissenterCreate(CFAllocatorRef, DAReturn, CFStringRef);
DASessionRef DASessionCreate(CFAllocatorRef);
void DASessionScheduleWithRunLoop(DASessionRef, CFRunLoopRef, CFStringRef);
void DARegisterDiskMountApprovalCallback(DASessionRef, CFDictionaryRef, DADiskMountApprovalCallback, void *);
void DARegisterDiskAppearedCallback(DASessionRef, CFDictionaryRef, DADiskAppearedCallback, void *);
void DARegisterDiskDescriptionChangedCallback(DASessionRef, CFDictionaryRef, CFArrayRef, DADiskDescriptionChangedCallback, void *);
void DARegisterDiskDisappearedCallback(DASessionRef, CFDictionaryRef, DADiskDisappearedCallback, void *);

/* ---------- SystemConfiguration ---------- */
CFStringRef SCDynamicStoreCopyConsoleUser(void *, uid_t *, gid_t *);

/* ---------- libdispatch ---------- */
typedef void (^dispatch_block_t)(void);
typedef struct sahte_kaynak *dispatch_source_t;
typedef void *dispatch_queue_t;
typedef const void *dispatch_source_type_t;
typedef uint64_t dispatch_time_t;
#define DISPATCH_SOURCE_TYPE_READ   ((dispatch_source_type_t)1)
#define DISPATCH_SOURCE_TYPE_PROC   ((dispatch_source_type_t)2)
#define DISPATCH_SOURCE_TYPE_SIGNAL ((dispatch_source_type_t)3)
#define DISPATCH_PROC_EXIT 0x80000000
#define DISPATCH_TIME_NOW 0ull
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif
#ifndef QOS_CLASS_UTILITY
#define QOS_CLASS_UTILITY 0x11
#endif
dispatch_queue_t dispatch_get_main_queue_(void);
#define dispatch_get_main_queue() dispatch_get_main_queue_()
dispatch_queue_t dispatch_get_global_queue(long, unsigned long);
void dispatch_async(dispatch_queue_t, dispatch_block_t);
dispatch_time_t dispatch_time(dispatch_time_t, int64_t);
void dispatch_after(dispatch_time_t, dispatch_queue_t, dispatch_block_t);
dispatch_source_t dispatch_source_create(dispatch_source_type_t, uintptr_t, unsigned long, dispatch_queue_t);
void dispatch_source_set_event_handler(dispatch_source_t, dispatch_block_t);
void dispatch_source_set_cancel_handler(dispatch_source_t, dispatch_block_t);
void dispatch_resume(dispatch_source_t);
void dispatch_source_cancel(dispatch_source_t);
void dispatch_release(dispatch_source_t);

/* ---------- sistem çağrıları ---------- */
#ifndef __APPLE__
struct statfs { char f_mntonname[1024]; };
#endif
int sahte_open(const char *, int, ...);
int sahte_stat(const char *, struct stat *);
int sahte_lstat(const char *, struct stat *);
int sahte_statfs(const char *, struct statfs *);
int sahte_mkdir(const char *, mode_t);
int sahte_rmdir(const char *);
int sahte_chown(const char *, uid_t, gid_t);
int sahte_access(const char *, int);
int sahte_rename(const char *, const char *);
pid_t sahte_fork(void);
pid_t sahte_waitpid(pid_t, int *, int);
int sahte_kill(pid_t, int);
int sahte_unmount(const char *, int);
int sahte_fcntl(int, int, ...);
struct passwd *sahte_getpwnam(const char *);
int sahte_pipe(int *);
#define open(...)      sahte_open(__VA_ARGS__)
#define stat(p, s)     sahte_stat(p, s)
#define lstat(p, s)    sahte_lstat(p, s)
#define statfs(p, s)   sahte_statfs(p, s)
#define mkdir(p, m)    sahte_mkdir(p, m)
#define rmdir(p)       sahte_rmdir(p)
#define chown(p, u, g) sahte_chown(p, u, g)
#define access(p, m)   sahte_access(p, m)
#define rename(a, b)   sahte_rename(a, b)
#define fork()         sahte_fork()
#define waitpid(p, s, o) sahte_waitpid(p, s, o)
#define kill(p, s)     sahte_kill(p, s)
#define unmount(p, f)  sahte_unmount(p, f)
#define fcntl(...)     sahte_fcntl(__VA_ARGS__)
#define getpwnam(n)    sahte_getpwnam(n)
#define pipe(p)        sahte_pipe(p)
#define main           ntfsotomatikd_main

/* ---------- testin dünyayı yönetmesi için ---------- */
typedef struct {
    int var;              /* takılı mı */
    char bsd[32];
    char uuid[40];        /* birim UUID ("" = yok) */
    char ad[64];
    int dahili;
    char tur[16];         /* "ntfs" ... */
    int macos_bagli;      /* macOS bağlamış (VolumePath var) */
    int acilamaz;         /* /dev/<bsd> açılırken EPERM */
} sahte_disk;

sahte_disk *sahte_disk_tak(const char *bsd, const char *uuid, const char *ad, int macos_bagli);
void sahte_disk_cek(const char *bsd);              /* fiziksel olarak çekildi (aygıt kaybolur) */
DADiskRef sahte_disk_ref(const char *bsd);         /* o anki bölüm için DADiskRef */
DADiskRef sahte_disk_ref_eski(const char *bsd);    /* çekilmeden önce alınmış ref gibi (aygıt yoksa da) */

/* bekleyen DADiskMount / DADiskUnmount işlemleri */
typedef struct { int tur; /* 1 bağla, 2 ayır */ char bsd[32]; DADiskMountCallback cb; void *ctx; int bitti; } sahte_da_islem;
sahte_da_islem *sahte_da_bekleyen(int tur, const char *bsd);
void sahte_da_bitir(sahte_da_islem *, int reddet);   /* sonucu teslim et */

/* ntfs-3g süreçleri (fork edilmiş gibi yapılır) */
int sahte_ntfs3g_sayisi(void);
pid_t sahte_ntfs3g_pid(const char *bsd);           /* bu aygıt için çalışan sahte ntfs-3g; yoksa -1 */
void sahte_ntfs3g_baglandi(pid_t);                 /* bağlama noktası artık bağlı (statfs) */
void sahte_ntfs3g_bitti(pid_t, int kod);           /* süreç bitti: PROC kaynağı tetiklenir */
pid_t sahte_ntfs3g_onceki_servis(const char *bsd); /* önceki servisin başlattığı, aygıtı kilitlemiş bağlı ntfs-3g
                                                      (bu servisin çocuğu değil: waitpid ECHILD döner) */
int sahte_waitpid_yabanci(void);                   /* çocuk olmayan sürece kaç kez waitpid çağrıldı */
int sahte_gunluk_var(const char *parca);           /* servis günlüğünde bu metin geçiyor mu */
void sahte_ntfs3g_yaz(pid_t, const char *metin);   /* ntfs-3g stderr'ine yaz (okuma olayı tetiklenmez) */
void sahte_okuma_tetikle(void);                    /* boru okuma kaynaklarının olayını çalıştır */
int sahte_dosya_var(const char *ad, const char *parca); /* servis klasöründeki dosyada metin var mı */
int sahte_son_sinyal(pid_t);                       /* bu sürece gönderilen son sinyal (0 = yok) */
int sahte_unmount_sayisi(const char *yol);         /* bu yol için unmount kaç kez çağrıldı */
void sahte_konsol_yok(int yok);                    /* giriş yapmış kullanıcı yokmuş gibi */
void sahte_dosya_yaz(const char *ad, const char *metin); /* servis klasörüne dosya (ör. baglar) */

void sahte_kuyrugu_calistir(void);                 /* dispatch_async kuyruğunu boşalt */
void sahte_saniye_ilerlet(int sn);                 /* dispatch_after zamanlayıcılarını çalıştır */
void sahte_sifirla(const char *klasor);            /* yeni senaryo; günlük/bildirim bu klasöre */
int sahte_bildirim_var(const char *parca);         /* bildirim dosyasında bu metin geçiyor mu */

#endif
