/*
**  ccunicode.h vs utf8proc — codec performance comparison
**
**  Build:
**    cc -std=c99 -O2 -I. -I3rd/ccalg/include -I3rd/utf8proc \
**       -o bench_codec bench/bench_codec.c 3rd/utf8proc/utf8proc.c
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "ccunicode.h"
#include "utf8proc.h"

/* ── timing ───────────────────────────────────────────────────────────── */

static double now_ms(void) {
  return (double)clock() / (double)CLOCKS_PER_SEC * 1000.0;
}

/* ── text generators ──────────────────────────────────────────────────── */

static size_t gen_ascii(char *buf, size_t n) {
  memset(buf, 'x', n);
  for (size_t i = 0; i < n; i++)
    if (i % 80 == 79) buf[i] = '\n';
  return n;
}

static size_t gen_mixed(char *buf, size_t n) {
  /* mix ASCII, 2-byte, 3-byte, 4-byte */
  const char *samples[] = {
    "Hello World. ",               /* ASCII */
    "中文测试。",                   /* 3-byte */
    "\xC2\xA9 \xC2\xA2 ",          /* 2-byte */
    "\xF0\x9F\x98\x80 ",           /* 4-byte (😀) */
  };
  size_t ns = sizeof(samples) / sizeof(samples[0]);
  size_t pos = 0;
  while (pos + 20 < n) {
    const char *s = samples[rand() % ns];
    size_t sl = strlen(s);
    if (pos + sl + 1 > n) break;
    memcpy(buf + pos, s, sl);
    pos += sl;
  }
  buf[pos] = '\0';
  return pos;
}

/* ── bench helpers ────────────────────────────────────────────────────── */

typedef struct bench_result {
  double us_ms;
  double utf8proc_ms;
  size_t iterations;
} bench_result_t;

/* ── decode benchmark ────────────────────────────────────────────────── */

static bench_result_t bench_decode(const char *name,
                                    const char *text, size_t text_len,
                                    int rounds) {
  double t0, t1;
  bench_result_t r = {0};
  r.iterations = 0;

  /* ccunicode.h */
  t0 = now_ms();
  for (int round = 0; round < rounds; round++) {
    size_t pos = 0;
    while (pos < text_len) {
      uint32_t cp;
      int n = ccunicode_to_codepoint(text + pos, (int)(text_len - pos), &cp);
      if (n <= 0) break;
      pos += (size_t)n;
      r.iterations++;
    }
  }
  t1 = now_ms();
  r.us_ms = t1 - t0;

  /* utf8proc */
  r.iterations = 0;
  t0 = now_ms();
  for (int round = 0; round < rounds; round++) {
    size_t pos = 0;
    while (pos < text_len) {
      utf8proc_int32_t cp;
      utf8proc_ssize_t n = utf8proc_iterate(
        (const utf8proc_uint8_t *)(text + pos),
        (utf8proc_ssize_t)(text_len - pos), &cp);
      if (n <= 0) break;
      pos += (size_t)n;
      r.iterations++;
    }
  }
  t1 = now_ms();
  r.utf8proc_ms = t1 - t0;

  printf("  %-12s  ccunicode.h %8.2f ms  |  utf8proc %8.2f ms  |  ratio %.2f×\n",
         name, r.us_ms, r.utf8proc_ms, r.utf8proc_ms / r.us_ms);
  return r;
}

/* ── encode benchmark ────────────────────────────────────────────────── */

static bench_result_t bench_encode(const char *name,
                                    uint32_t start, uint32_t end,
                                    int rounds) {
  double t0, t1;
  bench_result_t r = {0};
  volatile int sink;

  /* ccunicode.h */
  t0 = now_ms();
  for (int round = 0; round < rounds; round++) {
    for (uint32_t cp = start; cp <= end; cp++) {
      if (cp >= 0xD800 && cp <= 0xDFFF) continue;
      char buf[4]; int len;
      ccunicode_from_codepoint(cp, buf, &len);
      sink = (unsigned char)buf[0];
    }
  }
  t1 = now_ms();
  r.us_ms = t1 - t0;
  r.iterations = (size_t)(end - start + 1) * (size_t)rounds;

  /* utf8proc */
  t0 = now_ms();
  for (int round = 0; round < rounds; round++) {
    for (uint32_t cp = start; cp <= end; cp++) {
      if (cp >= 0xD800 && cp <= 0xDFFF) continue;
      utf8proc_uint8_t buf[4];
      utf8proc_encode_char((utf8proc_int32_t)cp, buf);
      sink = buf[0];
    }
  }
  t1 = now_ms();
  r.utf8proc_ms = t1 - t0;
  (void)sink;

  printf("  %-12s  ccunicode.h %8.2f ms  |  utf8proc %8.2f ms  |  ratio %.2f×\n",
         name, r.us_ms, r.utf8proc_ms,
         r.us_ms > 0 ? r.utf8proc_ms / r.us_ms : 0);
  return r;
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  srand(42);

  size_t cap = 10 * 1024 * 1024;  /* 10 MB */
  char *text = (char *)malloc(cap + 1);
  if (!text) return 1;

  printf("=== ccunicode.h vs utf8proc — codec benchmark ===\n\n");
  printf("  ccunicode.h version: self-contained (Unicode 17.0 / RFC 3629)\n");
  printf("  utf8proc version : %s\n\n", utf8proc_version());

  /* ── decode ─────────────────────────────────────────────────────── */

  printf("── Decode (UTF-8 → UCS-4) on 10 MB text ──\n");

  size_t len = gen_ascii(text, cap);
  bench_decode("ASCII", text, len, 30);

  len = gen_mixed(text, cap);
  bench_decode("Mixed", text, len, 10);

  printf("\n");

  /* ── encode ─────────────────────────────────────────────────────── */

  printf("── Encode (UCS-4 → UTF-8) ──\n");

  bench_encode("BMP (0-FFFF)", 0, 0xFFFF, 100);
  bench_encode("ASCII (0-7F)",  0, 0x7F, 20000);
  bench_encode("Full range",    0, 0x10FFFF, 3);

  printf("\n");
  printf("  Ratio > 1.0 = ccunicode.h is faster\n");
  printf("  Ratio < 1.0 = utf8proc is faster\n");

  free(text);
  return 0;
}
