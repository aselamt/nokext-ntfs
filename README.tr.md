# NoKext NTFS

macOS 26/27'de harici NTFS (Windows) diskleri **kendiliğinden ve yazılabilir** bağlar.
Açık kaynak **ntfs-3g** sürücüsünü ve **macFUSE FSKit** yöntemini kullanır: çekirdek uzantısı (kext),
Kurtarma Modu ya da güvenlik düşürme gerekmez.

Diski tak → yazılabilir bağlanır → bildirim gelir. İşin bitince Finder'dan çıkar (⏏).

> **Garanti değildir.** "Verilerim kaybolmasın" bir tasarım hedefidir, garanti değil.
> Önemli dosyalarının mutlaka başka bir yerde yedeği olsun.

Ayrıntılı kullanım: [KULLANIM.md](KULLANIM.md) · Tasarım ve macOS 27 bulguları: [docs/TASARIM.md](docs/TASARIM.md)

## Kurulum

Gerekenler: macOS 26 veya 27, Xcode Command Line Tools (`xcode-select --install`).
**macFUSE gereklidir** (ntfs-3g macOS'a onun üzerinden bağlanır) ama ayrıca uğraşmanıza gerek yok:
`kur.sh` yoksa onu resmi GitHub sürümünden indirir, SHA-256'sını, geliştirici imzasını ve Apple noter
onayını doğrular ve yalnızca çekirdek (Core) bileşenini kurar.

```sh
git clone https://github.com/aselamt/nokext-ntfs.git && cd nokext-ntfs
sudo ./kur.sh       # macFUSE (gerekirse) → derleme (sizin hesabınızla) → kurulum
```

macOS komut satırından macFUSE kurulumuna izin vermezse Yükleyici penceresi açılır: *Özelleştir* →
*macFUSE Preference Pane* işaretini kaldır → Yükle. Çekirdek uzantısı (kext) iznini **vermeyin**.

Sonra Sistem Ayarları'nda:

1. **Gizlilik ve Güvenlik → Tam Disk Erişimi → +** → `/Library/NTFSOtomatik/bin/ntfsotomatikd`
2. **Genel → Oturum Açma Öğeleri ve Genişletmeler → Dosya Sistemi Genişletmeleri (ⓘ)** →
   **macFUSE** ve **macFUSE (local)** açık (ilk NTFS disk takılınca listede görünür).

## Nasıl çalışır?

- Root servis (`ntfsotomatikd`) yalnızca disk takma/çıkarma olaylarını dinler, dışarıdan komut almaz.
- Yalnızca harici NTFS bölümlerine dokunur; APFS, exFAT, FAT32, HFS+ ve dahili disklere dokunmaz.
- macOS'un aynı bölümü kendi sürücüsüyle bağlamasını engeller (çift bağlama veri bozabilir).
- Aygıtı kendisi açar, yetkiyi oturum açmış kullanıcıya düşürür; ntfs-3g **root olarak çalışmaz**.
- ntfs-3g hep `norecover` ile çalışır; Windows günlüğü sıfırlanmaz, hazırda bekletme verisi silinmez.
- Bildirimler tek yönlüdür: root bir dosyaya yazar, kullanıcı tarafındaki yardımcı yalnızca okur.

## Adlar

Projenin adı **NoKext NTFS**. İçeride (kurulum yolları, servis etiketleri, betikler) ilk çalışma adı
kullanılmaya devam ediyor: `/Library/NTFSOtomatik`, `local.ntfsotomatik`, `ntfsotomatikd`.

## Lisans

GPL-2.0-or-later ([LICENSE](LICENSE)). ntfs-3g © Tuxera ve katkıda bulunanlar.
macFUSE © Benjamin Fleischer — bu depoda yer almaz; kurulumda resmi sürümünden indirilir ve kendi
lisansına tabidir (ticari olmayan dağıtıma izin verir, ticari yazılımla paketlenmesine izin vermez).
