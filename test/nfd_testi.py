# Kısa NFD/NFC ad testi: aynı adlar iki klasöre, biri ayrışık (NFD) biri birleşik (NFC) biçimde yazılır.
import os, sys, unicodedata, hashlib
root = sys.argv[1]
adlar = ['Çanakkale.txt', 'Şubat raporu.txt', 'Üsküdar.txt', 'Ödev ödül.txt', 'İzmir İl.txt',
         'ağaç dağ.txt', 'Selçuk İlahiyat Dergisi.pdf', 'Sempozyum Afişi.pdf', 'AYASOFYA TÜM GRUP-SON.xlsx',
         'aaa.txt', 'zzz.txt', 'çç.txt', 'şş.txt', 'öö.txt', 'üü.txt', 'Ğ büyük.txt', 'ğ küçük.txt']
alt = 'Ayrışık Klasör'
out = []
for bicim in ('NFD', 'NFC'):
    d = os.path.join(root, f'{bicim.lower()}_testi')
    os.makedirs(os.path.join(d, unicodedata.normalize(bicim, alt)), exist_ok=False)
    for i, a in enumerate(adlar):
        veri = (f'{bicim} {a} {i}\n' * 200).encode()
        p = os.path.join(d, unicodedata.normalize(bicim, a))
        with open(p, 'wb') as f:
            f.write(veri); f.flush(); os.fsync(f.fileno())
        out.append((hashlib.sha256(veri).hexdigest(), p))
    for a in adlar[:3]:
        p = os.path.join(d, unicodedata.normalize(bicim, alt), unicodedata.normalize(bicim, a))
        veri = b'alt\n' * 100
        with open(p, 'wb') as f:
            f.write(veri); f.flush(); os.fsync(f.fileno())
        out.append((hashlib.sha256(veri).hexdigest(), p))
print(len(out), 'dosya yazıldı')
