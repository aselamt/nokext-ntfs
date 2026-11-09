# Tasarım notları ve macOS 27 bulguları

Bu belge, sistemin neden böyle tasarlandığını ve geliştirme sırasında (Kasım 2026, macOS 27.2,
Apple M4, macFUSE 5.5.0, ntfs-3g 2026.9.28) karşılaşılan engelleri anlatır.

## Bileşen seçimi

| Seçenek | Karar | Neden |
|---|---|---|
| macFUSE **FSKit** arka ucu | ✅ | Kext yok, Kurtarma Modu / güvenlik düşürme yok. |
| macFUSE kext arka ucu | ❌ | Apple Silicon'da "Reduced Security" ister. |
| FUSE-T | ❌ | İkili dosyası yalnızca ticari olmayan kullanım için ücretsiz; ntfs-3g ile yavaş. |
| Hazır otomatik bağlayıcılar | ❌ | Bazıları varsayılan olarak `remove_hiberfile` kullanıyor ya da sürücüyü root çalıştırıyor. |

ntfs-3g `--disable-ntfsprogs` ile derlenir: `ntfsfix` gibi onarım araçları hiç oluşmaz.
Darwin'de ntfs-3g'nin `nfconv` özelliği açıktır (adlar macOS'a ayrışık/NFD biçimde sunulur).

## macOS 27'de karşılaşılan engeller

1. **Ham disk erişimi root için de Tam Disk Erişimi ister.** `/dev/diskXsY` açmak, root olarak bile
   `Operation not permitted` veriyordu. İzin, işlemi başlatan programa verilir. launchd işinin programı
   `/bin/sh` gibi bir Apple ikilisi olursa izin verilemez. Çözüm: launchd'nin doğrudan başlattığı
   kendi programımız (`ntfsotomatikd`) ve Tam Disk Erişimi ona verilir. Program ad-hoc imzalı olduğu için
   her yeniden derlemede izin yeniden verilmelidir.
2. **FSKit modülleri kullanıcı başınadır.** Sistem Ayarları'nda açılan "macFUSE (local)" yalnızca o
   kullanıcı için geçerlidir. Root olarak başlatılan ntfs-3g `File system extension not enabled` hatası
   alıyordu. Çözüm: root servis aygıtı açar, yetkiyi konsol kullanıcısına düşürür ve ntfs-3g'yi açık dosya
   tanıtıcısıyla (`/dev/fd/3`) kullanıcı olarak çalıştırır. Bu, sürücüyü root'tan çıkardığı için ayrıca
   daha güvenlidir.
3. **Root, aygıt dosyasının sahibini değiştiremiyor.** `chown /dev/diskXsY` root olarak da reddedildi.
   Bu yüzden "aygıtı kullanıcıya ver" yolu kullanılamadı.
4. **ntfs-3g'nin iki Linux varsayımı** macOS'ta yamalandı:
   - "Ayrıcalıksız kullanıcı blok aygıtı bağlayamaz" kontrolü (Linux `fuseblk` kuralı).
   - `/dev/fd/N` yeniden açılınca macOS aygıt izinlerini yeniden denetliyor; yama tanıtıcıyı çoğaltır (`dup`).
5. **Çift bağlama.** ntfs-3g bölümü kullanırken macOS aynı bölümü kendi (salt-okunur) NTFS sürücüsüyle
   kendiliğinden yeniden bağlıyordu. Testte bu, Windows chkdsk'in bulduğu dizin girdisi hatalarına ve
   Mac'te bir MFT kaydının sıfır okunmasına yol açtı. Çözüm: servis DiskArbitration'a bağlama onayı
   geri çağrısı kaydeder ve ntfs-3g bağlıyken (ve Finder'dan çıkarıldıktan sonra fiziksel çıkar-tak
   olana kadar) o bölüm için macOS'un bağlama isteğini reddeder.
6. **Ek bilgiler (xattr).** ntfs-3g'nin varsayılan `streams_interface=xattr` ayarı yalnızca `user.`
   önekli adları kabul eder; macOS'un `com.apple.*` adları `EOPNOTSUPP` alır ve Finder kopyalamayı
   "hata kodu 100102" ile iptal eder. Çözüm: `streams_interface=openxattr` (ek bilgiler NTFS adlandırılmış
   veri akışı olarak saklanır).
7. **Uzun Türkçe adlar.** nfconv adları NFD verdiği için ~85+ Türkçe harfli adlar 255 baytı aşıp
   kesiliyor ve açılamıyor (bilinen sınır).

## Güvenlik modeli

- Root servis argüman, soket ya da yapılandırma dosyası kabul etmez; yalnızca DiskArbitration olaylarına tepki verir.
- Aygıt adı `^disk[0-9]+s[0-9]+$` olmalı; işlem anında DiskArbitration'dan harici + `VolumeKind=ntfs` yeniden doğrulanır.
- Diskler kimliğe göre izlenir: birim UUID → ortam (bölüm) UUID → aygıt yolu + boyut. (Aynı `diskXsY` adı farklı disklere verilebiliyor.)
- Bağlama noktası adı ve `volname` temizlenir (`/ : , = \ "`, denetim karakterleri, `.`/`..`; virgül ntfs-3g seçeneği enjekte edemesin diye).
- Root ile kullanıcı arasında tek yönlü iletişim: root, `root:wheel 0644` bir dosyaya satır ekler; kullanıcı ajanı yalnızca okur
  ve metni osascript'e kod olarak değil `argv` olarak verir.
- ntfs-3g hardened runtime ile imzalanır (aynı kullanıcının diğer süreçleri araya giremez); macFUSE'un libfuse'u farklı imzalı
  olduğu için yalnızca `disable-library-validation` yetkisi verilir.
- Root'un çalıştırdığı her şey ve tüm üst klasörleri `root:wheel`'e aittir; Homebrew'dan hiçbir şey kullanılmaz (`otool -L` ile denetlenir).

## Test edilenler (USB bellek, macOS 27.2)

| Senaryo | Sonuç |
|---|---|
| 3,1 GB / 414 dosya yazma, çıkar-tak, SHA-256 | 414/414 aynı |
| Türkçe, emoji, boşluklu ve NFD adlar | Mac'te erişilebilir, Windows chkdsk temiz |
| Finder ile kopyalama (indirilmiş dosya, 5 xattr) | Uyarısız, içerik ve ek bilgiler korunuyor |
| Silme | Çalışıyor, alan geri geliyor, chkdsk temiz |
| Takınca otomatik bağlama / Finder ⏏ sonrası yeniden bağlamama / çıkar-tak | Çalışıyor |
| Çok bölümlü disk (EFI + NTFS) | Yalnızca NTFS bölümü işlendi |
| Tam Disk Erişimi yokken | macOS salt-okunur yedek yol + doğru sebep bildirimi |
| Finder'dan çıkarmadan çekme | Anında tek bildirim, zorlamasız temizlik |
| Disk bağlıyken güncelleme | Reddedildi |
| Gerçek güncelleme ve tek komutla geri alma | Çalışıyor |
| Yeniden başlatma sonrası otomatik başlama | Çalışıyor |

Denenemeyenler: aynı adlı iki disk, hazırda bekletmedeki Windows birimi, aynı anda takılı exFAT/APFS diskler.

Not: Geliştirme sırasında arızalı bir USB bellek, donanım düzeyinde yazma hataları verdi (Windows'ta da
bozuldu). Bu tür hatalar ntfs-3g'den bağımsızdır; kernel günlüğündeki `IOSCSI… I/O error` satırlarıyla ayırt edilebilir.
