# NTFSOtomatik — Kullanım Kılavuzu

## 1. Bu ne yapar?

Mac'e bir NTFS disk (Windows diski) takıldığında onu kendiliğinden **yazılabilir** olarak bağlar.
Şifre sormaz. Sağ üstte bir bildirim gösterir: *“Disk adı” yazılabilir olarak bağlandı.*

> **Garanti değildir.** "Verilerim kaybolmasın" bu sistemin tasarım hedefidir, garantisi değildir.
> NTFS, Microsoft'un kapalı bir dosya sistemidir; ntfs-3g onu dışarıdan yazar.
> Önemli dosyalarının **mutlaka** başka bir yerde (başka bir diskte veya bulutta) yedeği olsun.

## 2. Günlük kullanım

- Diski tak → bildirim gelir → Finder'da görünür → normal kullan.
- İşin bitince **her zaman Finder'dan çıkar (⏏)**; disk Finder'dan kaybolunca kabloyu çek.
  - **Neden?** Mac yazdıklarını bir süre bellekte tutar; ⏏ hepsinin diske yazılmasını ve NTFS'in
    düzgün kapanmasını sağlar. ⏏ yapmadan çekersen son yazılanlar kaybolabilir ve Windows
    "diski onar" isteyebilir.
- Finder'dan çıkardığın disk, kablosunu çekip yeniden takana kadar tekrar bağlanmaz (bilerek böyle).

## 3. Bildirimler ne demek?

| Bildirim | Anlamı / yapılacak |
|---|---|
| *yazılabilir olarak bağlandı* | Her şey yolunda. |
| *salt-okunur bağlandı: Windows'ta tam kapatılmamış ya da hazırda bekletmede…* | Disk Windows'ta düzgün kapatılmamış (Hızlı Başlatma / hazırda bekletme). Veriyi korumak için yalnızca okunur açıldı. Diski Windows'a tak, Windows'u **tamamen** kapat (Hızlı Başlatma kapalı), sonra tekrar Mac'e tak. |
| *yazılabilir bağlanamadı (sebep). macOS'un salt-okunur desteğiyle açıldı.* | Okuyabilirsin ama yazamazsın. Sebep bildirimde yazar; ayrıntı günlükte. Sık sebep: Tam Disk Erişimi (bölüm 8). |
| *bağlanamadı (sebep). Disk bağlı değil.* | Hiç açılamadı; günlüğe bak. |
| *yazma/okuma hatası algılandı* | Açık dosyaları kaydet, diski Finder'dan çıkar, Windows'ta `chkdsk X:` (salt-okuma) ile kontrol et. |
| *Finder'dan çıkarılmadan çekildi* | Diski Windows'a takıp `chkdsk X:` ile kontrol et. |

## 4. Bilinen sınırlar

- Çok uzun ve çok Türkçe/aksanlı harf içeren dosya adları (yaklaşık 85+ böyle harf) Mac'te açılamayabilir
  (macOS'un 255 bayt ad sınırı, adlar ayrışık/NFD biçimde verildiği için).
- Hız: macFUSE'un FSKit yöntemi yavaştır (testte USB bellekte yazma ~4,5 MB/sn).
- Aynı adlı iki disk takılırsa ikincisi "Ad 2" olarak açılır (henüz denenmedi).
- Disk bağlıyken oturum kapatılırsa disk açık kalır; önce Finder'dan çıkar.
- Belirli bir diski hep salt-okunur tutmak istersen, takmadan önce servisi kapat (bölüm 7).

## 5. Sistemin parçaları (yer — sahibi)

Hepsi `root:wheel`'e aittir; kullanıcı hesabı değiştiremez.

| Yer | Görev |
|---|---|
| `/Library/NTFSOtomatik/bin/ntfsotomatikd` | root servis: diski açar, ntfs-3g'yi kullanıcı olarak başlatır |
| `/Library/NTFSOtomatik/bin/ntfsotomatik-bildirim` | bildirim yardımcısı (yalnızca okur) |
| `/Library/NTFSOtomatik/current` → `surumler/<sürüm>` | kullanılan ntfs-3g sürümü |
| `/Library/NTFSOtomatik/onceki` → `surumler/<sürüm>` | geri dönülebilecek önceki sürüm |
| `/Library/NTFSOtomatik/surumler/<sürüm>/` | ntfs-3g + libfuse kopyası + yamalar |
| `/Library/NTFSOtomatik/betikler/` | yönetim komutları (bölüm 7) |
| `/Library/NTFSOtomatik/durum/bildirim` | servisten bildirim yardımcısına tek yönlü mesajlar |
| `/Library/LaunchDaemons/local.ntfsotomatik.plist` | servisin başlatma ayarı |
| `/Library/LaunchAgents/local.ntfsotomatik.bildirim.plist` | bildirim yardımcısının başlatma ayarı |
| `/Library/Logs/NTFSOtomatik/` | günlük (en fazla 1 MB + 3 eski dosya) |

macFUSE (ayrıca kurulur): `/Library/Filesystems/macfuse.fs`, `/Library/Frameworks/macFUSE.framework`,
`/usr/local/lib/libfuse*`, `/usr/local/include/fuse*`, `/Library/LaunchDaemons/io.macfuse.app.launchservice.daemon.plist`,
`/Library/PrivilegedHelperTools/io.macfuse.app.launchservice.daemon`.

Sistem Ayarları'nda açılan izinler:
- Gizlilik ve Güvenlik → Tam Disk Erişimi: `ntfsotomatikd`
- Genel → Oturum Açma Öğeleri ve Genişletmeler → Dosya Sistemi Genişletmeleri: macFUSE ve macFUSE (local)

## 6. Günlük (log)

```sh
tail -50 /Library/Logs/NTFSOtomatik/ntfsotomatik.log
/Library/NTFSOtomatik/betikler/durum.sh        # kısa durum özeti
```

## 7. Yönetim komutları (Terminal; şifre ister)

Hepsi, ntfs-3g ile bağlı bir disk varken çalışmayı **reddeder** ("önce Finder'dan çıkar").

```sh
sudo /Library/NTFSOtomatik/betikler/servis.sh kapat     # NTFS diskler macOS'un salt-okunur desteğiyle açılır
sudo /Library/NTFSOtomatik/betikler/servis.sh ac
sudo /Library/NTFSOtomatik/betikler/geri-al.sh          # current ile onceki yer değiştirir
sudo /Library/NTFSOtomatik/betikler/guncelle.sh <hazır-klasör> <sürüm-adı>
sudo /Library/NTFSOtomatik/betikler/kaldir.sh           # yalnızca bu kurulumun parçalarını siler
sudo /Library/NTFSOtomatik/betikler/kaldir.sh --macfuse # macFUSE'u da resmi kaldırıcısıyla siler
```

Yeni bir ntfs-3g sürümü için: depoda `./build.sh <yeni-sürüm-adı>`, sonra
`sudo /Library/NTFSOtomatik/betikler/guncelle.sh dist/surumler/<yeni-sürüm-adı> <yeni-sürüm-adı>`.

Kaldırdıktan sonra Tam Disk Erişimi listesindeki `ntfsotomatikd` satırını "−" ile sil.

## 8. Sorun çıkarsa

- **Tam Disk Erişimi** sebebiyle bağlanamıyorsa: `ntfsotomatikd` açık mı bak. Servis programı yeniden
  derlendiyse izin geçersiz olur: satırı "−" ile sil, "+" ile yeniden ekle.
- **"macFUSE (local) uzantısı kapalı"** diyorsa: Genel → Oturum Açma Öğeleri ve Genişletmeler →
  Kategoriye Göre → Dosya Sistemi Genişletmeleri → ⓘ → macFUSE (local) açık olsun.
- Kullanımdaki bir diski asla zorla ayırma. Finder çıkarmıyorsa açık pencereleri/programları kapat, tekrar dene.
- Disk onarımını (`chkdsk /f`) yalnızca Windows'ta ve gerekirse yap; Mac'te onarım aracı kurulu değildir (bilerek).

## 9. Güvenlik notları

- Servis dışarıdan komut kabul etmez; yalnızca disk takma/çıkarma olaylarına tepki verir.
- ntfs-3g root olarak çalışmaz; oturum açmış kullanıcı olarak çalışır. Diski yalnızca root servis açar.
- ntfs-3g her zaman `norecover` ile çalışır; Windows günlüğünü sıfırlama, hazırda bekletme verisini silme,
  zorla bağlama yapılmaz.
- Kalan risk: disk bağlıyken kullanıcı hesabında çalışan kötü amaçlı bir program o diske yazabilir
  (her disk için geçerli). Servis programı Tam Disk Erişimi'ne sahiptir; dosyası root'a aittir.
