// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ntfsotomatikd DiskArbitration olay sırası regresyon testleri (gerçek disk / root gerekmez).
 * src/ntfsotomatikd.c değiştirilmeden, sahte.h'deki sahte çerçevelerle derlenir.
 * Çalıştırmak için: sh test/olay/calistir.sh
 */
#include "sahte.h"
#include "../../src/ntfsotomatikd.c"
#undef main

static int g_hata, g_toplam;
static char g_klasor[1024];
#define BEKLE(kosul, ...) do { g_toplam++; if (!(kosul)) { g_hata++; printf("  HATA: "); printf(__VA_ARGS__); printf("  [%s:%d]\n", __FILE__, __LINE__); } } while (0)

/* önceki senaryo hata verdiyse servis günlüğünü göster */
static int g_onceki_hata;
static void gunluk_goster(void) {
    if (g_hata == g_onceki_hata) return;
    g_onceki_hata = g_hata;
    char p[1100]; snprintf(p, sizeof p, "%s/ntfsotomatik.log", g_klasor);
    FILE *f = fopen(p, "r"); if (!f) return;
    char satir[2048]; printf("  -- servis günlüğü --\n");
    while (fgets(satir, sizeof satir, f)) printf("  | %s", satir + 20);
    fclose(f);
}
static void basla(const char *ad) {
    gunluk_goster();
    printf("== %s\n", ad);
    sahte_sifirla(g_klasor);
    memset(g_disks, 0, sizeof g_disks);
    for (int i = 0; i < MAX_DISKS; i++) { g_disks[i].pid = -1; g_disks[i].out_fd = -1; }
}
/* macOS'un bağlama isteği bu disk için reddediliyor mu? (reddedilmezse macOS kendi sürücüsüyle bağlar) */
static int macos_reddedildi(const char *bsd) {
    DADissenterRef r = mount_approval(sahte_disk_ref(bsd), NULL);
    sahte_kuyrugu_calistir();
    return r != NULL;
}
/* ntfs-3g bağlandı ve servis bunu gördü */
static void ntfs3g_baglansin(const char *bsd) {
    sahte_ntfs3g_baglandi(sahte_ntfs3g_pid(bsd));
    sahte_saniye_ilerlet(1);
}

/* Bulgu 2: macOS bağlantısı ayrılırken disk çekilir, yeni disk yuvayı alır, eski ayırma sonucu sonra gelir. */
static void gecikmis_ayirma(const char *yeni_bsd, int reddet) {
    char ad[160];
    snprintf(ad, sizeof ad, "gecikmiş ayırma sonucu (%s, yeni disk %s)", reddet ? "ret" : "başarı", yeni_bsd);
    basla(ad);
    sahte_disk_tak("disk4s1", "11111111-1111-1111-1111-111111111111", "ESKI", 1);
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    sahte_da_islem *ayir = sahte_da_bekleyen(2, "disk4s1");
    BEKLE(ayir != NULL, "macOS bağlantısı için ayırma istenmedi");
    if (!ayir) return;

    sahte_disk_cek("disk4s1");
    disk_disappeared(sahte_disk_ref_eski("disk4s1"), NULL);

    sahte_disk_tak(yeni_bsd, "22222222-2222-2222-2222-222222222222", "YENI", 0);
    disk_appeared(sahte_disk_ref(yeni_bsd), NULL);
    BEKLE(sahte_ntfs3g_pid(yeni_bsd) > 0, "yeni disk için ntfs-3g başlamadı");

    sahte_da_bitir(ayir, reddet);   /* eski diskin gecikmiş sonucu */
    sahte_kuyrugu_calistir();

    BEKLE(macos_reddedildi(yeni_bsd), "ntfs-3g çalışırken macOS'un yeni diski bağlamasına izin verildi (çift bağlama)");
    BEKLE(sahte_ntfs3g_sayisi() == 1, "aynı disk için %d ntfs-3g çalışıyor (1 olmalı)", sahte_ntfs3g_sayisi());
    BEKLE(!sahte_bildirim_var("“YENI” macOS tarafından salt-okunur"), "eski diskin sonucu yeni disk için bildirildi");

    ntfs3g_baglansin(yeni_bsd);
    BEKLE(sahte_bildirim_var("“YENI” yazılabilir olarak bağlandı"), "yeni disk yazılabilir bağlanmadı");
}

/* Bulgu 2: macOS salt-okunur yedek bağlama sonucu, disk çekilip başka disk takıldıktan sonra gelir. */
static void gecikmis_yedek_baglama(void) {
    basla("gecikmiş salt-okunur yedek bağlama sonucu");
    sahte_disk *x = sahte_disk_tak("disk4s1", "11111111-1111-1111-1111-111111111111", "ESKI", 0);
    x->acilamaz = 1;   /* Tam Disk Erişimi yok gibi: yedek yol denenir */
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    sahte_da_islem *bagla = sahte_da_bekleyen(1, "disk4s1");
    BEKLE(bagla != NULL, "salt-okunur yedek bağlama istenmedi");
    if (!bagla) return;

    sahte_disk_cek("disk4s1");
    disk_disappeared(sahte_disk_ref_eski("disk4s1"), NULL);
    sahte_disk_tak("disk4s1", "22222222-2222-2222-2222-222222222222", "YENI", 0);   /* aynı aygıt adı */
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    BEKLE(sahte_ntfs3g_pid("disk4s1") > 0, "yeni disk için ntfs-3g başlamadı");

    sahte_da_bitir(bagla, 0);
    sahte_kuyrugu_calistir();
    BEKLE(macos_reddedildi("disk4s1"), "ntfs-3g çalışırken macOS'un yeni diski bağlamasına izin verildi (çift bağlama)");
    BEKLE(!sahte_bildirim_var("“YENI” yazılabilir bağlanamadı"), "eski diskin sonucu yeni disk için bildirildi");
}

/* Bulgu 3: aynı birim UUID'sini taşıyan iki klon disk. */
static void klon_diskler(int once_onay) {
    basla(once_onay ? "klon diskler (ikincisi önce macOS bağlama isteğiyle gelir)" : "klon diskler (ikincisi 'göründü' olayıyla gelir)");
    const char *u = "33333333-3333-3333-3333-333333333333";
    sahte_disk_tak("disk4s1", u, "KLON", 0);
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    ntfs3g_baglansin("disk4s1");
    BEKLE(sahte_bildirim_var("“KLON” yazılabilir olarak bağlandı"), "ilk klon bağlanmadı");

    sahte_disk_tak("disk5s1", u, "KLON", 0);
    if (once_onay) BEKLE(macos_reddedildi("disk5s1"), "ikinci klon için macOS isteği reddedilmedi");
    disk_appeared(sahte_disk_ref("disk5s1"), NULL);
    BEKLE(sahte_ntfs3g_pid("disk5s1") > 0, "ikinci klon için ntfs-3g hiç başlamadı");
    BEKLE(sahte_ntfs3g_sayisi() == 2, "%d ntfs-3g çalışıyor (2 olmalı)", sahte_ntfs3g_sayisi());

    /* ikinci klon Finder'dan çıkarılınca birincisi etkilenmemeli */
    sahte_ntfs3g_bitti(sahte_ntfs3g_pid("disk5s1"), 0);
    BEKLE(macos_reddedildi("disk4s1"), "ikinci klon çıkarılınca birincisi için macOS bağlamasına izin verildi");
    BEKLE(sahte_ntfs3g_pid("disk4s1") > 0, "birinci klonun ntfs-3g'si durdu");
}

/* Var olan davranış: Finder'dan çıkarılan disk çıkar-tak olana kadar yeniden bağlanmaz. */
static void finder_cikarma(void) {
    basla("Finder'dan çıkarma, sonra fiziksel çıkar-tak");
    sahte_disk_tak("disk4s1", "44444444-4444-4444-4444-444444444444", "VERI", 0);
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    disk_changed(sahte_disk_ref("disk4s1"), NULL, NULL);
    BEKLE(macos_reddedildi("disk4s1"), "bağlanırken macOS isteği reddedilmedi");
    BEKLE(sahte_ntfs3g_sayisi() == 1, "tekrarlanan olaylarda %d ntfs-3g başladı (1 olmalı)", sahte_ntfs3g_sayisi());
    ntfs3g_baglansin("disk4s1");

    sahte_ntfs3g_bitti(sahte_ntfs3g_pid("disk4s1"), 0);   /* Finder ⏏ */
    disk_changed(sahte_disk_ref("disk4s1"), NULL, NULL);
    BEKLE(macos_reddedildi("disk4s1"), "çıkarılan disk macOS'a bırakıldı");
    BEKLE(sahte_ntfs3g_sayisi() == 0, "çıkarılan disk yeniden bağlandı");

    sahte_disk_cek("disk4s1");
    disk_disappeared(sahte_disk_ref_eski("disk4s1"), NULL);
    sahte_disk_tak("disk4s1", "44444444-4444-4444-4444-444444444444", "VERI", 0);
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    BEKLE(sahte_ntfs3g_sayisi() == 1, "çıkar-tak sonrası bağlanmadı");
}

/* Var olan davranış: macOS bağlantısı başarıyla ayrılınca ntfs-3g başlar. */
static void macos_baglantisi_devralma(void) {
    basla("macOS'un salt-okunur bağlantısını devralma");
    sahte_disk_tak("disk4s1", "55555555-5555-5555-5555-555555555555", "ARSIV", 1);
    disk_appeared(sahte_disk_ref("disk4s1"), NULL);
    sahte_da_islem *ayir = sahte_da_bekleyen(2, "disk4s1");
    BEKLE(ayir != NULL, "ayırma istenmedi");
    if (!ayir) return;
    BEKLE(macos_reddedildi("disk4s1"), "ayırma sürerken macOS isteği reddedilmedi");
    sahte_da_bitir(ayir, 0);
    sahte_kuyrugu_calistir();
    BEKLE(sahte_ntfs3g_sayisi() == 1, "ayrıldıktan sonra ntfs-3g başlamadı");
}

int main(int argc, char **argv) {
    snprintf(g_klasor, sizeof g_klasor, "%s", argc > 1 ? argv[1] : ".");
    finder_cikarma();
    macos_baglantisi_devralma();
    gecikmis_ayirma("disk5s1", 1);
    gecikmis_ayirma("disk5s1", 0);
    gecikmis_ayirma("disk4s1", 1);
    gecikmis_ayirma("disk4s1", 0);
    gecikmis_yedek_baglama();
    klon_diskler(0);
    klon_diskler(1);
    gunluk_goster();
    printf("\n%d/%d denetim geçti\n", g_toplam - g_hata, g_toplam);
    return g_hata ? 1 : 0;
}
