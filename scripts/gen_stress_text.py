#!/usr/bin/env python3
"""Fast stress-test text generator.  Usage: python3 gen_stress_text.py [words_file] [out] [MB]"""
import sys, random, os

random.seed(123)
wf = sys.argv[1] if len(sys.argv) > 1 else "words.zh.txt"
out = sys.argv[2] if len(sys.argv) > 2 else "stress_text.txt"
target_mb = int(sys.argv[3]) if len(sys.argv) > 3 else 3

with open(wf, encoding="utf-8") as f:
    words = [line.strip().encode("utf-8") for line in f if line.strip()]
print(f"Loaded {len(words)} words")

NOISE = "的一是不了在人有我他这来之个们到说子大时中国和地产为生要以会可下过就去能对学都如同行得经用年自现那其理前所而事法开把也道面看长成还后多天样起小重新实家将两应它但已此与又并通些那想种者被部力进等里当从给使条文义之类主度高问体回定展"
NOISE_BYTES = [c.encode("utf-8") for c in NOISE]
PUNCT = [b',', b'\xef\xbc\x8c', b'\xef\xbc\x81', b'\n']

target = target_mb * 1024 * 1024
buf = bytearray()
emb = []  # (offset, word_bytes)

while len(buf) < target:
    r = random.random()
    if r < 0.25:
        w = random.choice(words)
        emb.append((len(buf), w))
        buf.extend(w)
    elif r < 0.60:
        n = random.randint(2, 30)
        for _ in range(n):
            buf.extend(random.choice(NOISE_BYTES))
    else:
        buf.extend(random.choice(PUNCT))

# truncate to valid UTF-8 boundary
buf = buf[:target]
# strip incomplete multi-byte sequence at end
while len(buf) > 0 and (buf[-1] & 0xC0) == 0x80:
    buf.pop()
if len(buf) > 0 and buf[-1] >= 0xC0:
    # check if it's a starter byte for an incomplete sequence
    seq_len = 1
    if (buf[-1] & 0xE0) == 0xC0: seq_len = 2
    elif (buf[-1] & 0xF0) == 0xE0: seq_len = 3
    elif (buf[-1] & 0xF8) == 0xF0: seq_len = 4
    # count continuation bytes available
    cnt = 0
    for i in range(len(buf)-2, -1, -1):
        if (buf[i] & 0xC0) == 0x80: cnt += 1
        else: break
    if cnt + 1 < seq_len:
        for _ in range(cnt + 1): buf.pop()

with open(out, "wb") as f:
    f.write(buf)

# save embedded positions (only those fully within buffer)
pos_file = out + ".pos"
count = 0
with open(pos_file, "w", encoding="utf-8") as f:
    for off, w in emb:
        if off + len(w) <= len(buf):
            f.write(f"{off}\t{w.decode('utf-8', errors='replace')}\n")
            count += 1

print(f"Wrote {out} ({len(buf)/1024/1024:.1f} MB), {count} embedded positions → {pos_file}")
