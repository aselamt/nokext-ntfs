#!/usr/bin/env python3
"""Test verisi üretir: bir büyük dosya, yüzlerce küçük dosya, Türkçe ve ayrışık (NFD) adlı dosyalar.

Kullanım: python3 testveri.py <hedef-klasör> [büyük-dosya-MiB, varsayılan 1024]
Ardından: python3 manifest.py <hedef-klasör> > kaynak.tsv   (SHA-256 listesi)
Test diskine kopyalayıp çıkar-tak yaptıktan sonra aynı manifest'i diskte üretip karşılaştırın.
"""
import os, sys, random, unicodedata

root = sys.argv[1]
big_mib = int(sys.argv[2]) if len(sys.argv) > 2 else 1024
random.seed(20261107)

def w(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'wb') as f:
        f.write(data)

# 1) büyük dosya (rastgele içerik)
big = os.path.join(root, 'buyuk', f'buyuk_{big_mib}MiB.bin')
os.makedirs(os.path.dirname(big), exist_ok=True)
with open(big, 'wb') as f:
    for _ in range(big_mib):
        f.write(os.urandom(1024 * 1024))

# 2) 400 küçük dosya, 4 alt klasörde, 0 bayt - 300 KB
for i in range(400):
    size = 0 if i % 97 == 0 else random.randint(1, 300 * 1024)
    w(os.path.join(root, 'kucuk', f'grup{i % 4}', f'dosya_{i:04d}.dat'), os.urandom(size))

# 3) Türkçe ve özel karakterli adlar (NFC)
tr = ['çğıöşü ÇĞİÖŞÜ.txt', 'Kütüphane notları.txt', 'İstanbul – Ağustos 2020.txt',
      'boşluklu   ad.txt', "tırnak'lı ve (parantez) [köşeli].txt", 'emoji 📁 dosya.txt',
      'nokta.ile.bitmeyen.ad']
for n in tr:
    n = unicodedata.normalize('NFC', n)
    w(os.path.join(root, 'turkce', n), (n * 50).encode())
w(os.path.join(root, 'turkce', unicodedata.normalize('NFC', 'Şükrü klasörü'), 'içerik ğüşiöç.txt'), b'ic\n' * 1000)

# 4) ayrışık (NFD) adlar
nfd = ['Sempozyum Afişi.pdf', 'Ses Kayıtları Şubat.m4a', 'Selçuk İlahiyat Dergisi.pdf', 'AYASOFYA TÜM GRUP-SON.xlsx']
for n in nfd:
    d = unicodedata.normalize('NFD', n)
    assert d != unicodedata.normalize('NFC', n)
    w(os.path.join(root, 'nfd', d), os.urandom(random.randint(10_000, 2_000_000)))
w(os.path.join(root, 'nfd', unicodedata.normalize('NFD', 'Ayrışık Klasör'),
               unicodedata.normalize('NFD', 'öğe ü.txt')), b'nfd\n' * 500)
