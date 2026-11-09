import os, sys, hashlib, unicodedata
root = sys.argv[1]
for dp, dn, fn in os.walk(root):
    dn.sort()
    for n in sorted(fn):
        p = os.path.join(dp, n); h = hashlib.sha256()
        with open(p, 'rb') as f:
            for b in iter(lambda: f.read(1<<20), b''): h.update(b)
        rel = unicodedata.normalize('NFC', os.path.relpath(p, root))
        print(f"{h.hexdigest()}\t{os.path.getsize(p)}\t{rel}")
