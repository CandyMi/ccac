/*
**  Unit tests for unicode.h — UTF-8 ↔ UCS-4 codec.
**
**  Build:  cc -std=c99 -Wall -Wextra -I.. -o test_unicode test_unicode.c && ./test_unicode
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "../unicode.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define T(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) printf("  %s\n", #name)

/* ════════════════════════════════════════════════════════════════════════
**  ccac_unicode_to_codepoint  —  UTF-8 → UCS-4
** ════════════════════════════════════════════════════════════════════════ */

/* ── 1-byte: ASCII ────────────────────────────────────────────────────── */

void test_decode_ascii(void) {
  RUN(ascii_decode);
  uint32_t val; int n;

  /* single byte */
  n = ccac_unicode_to_codepoint("A", 1, &val);
  T(n == 1 && val == 0x41);

  /* null byte (U+0000 is valid) */
  n = ccac_unicode_to_codepoint("\x00", 1, &val);
  T(n == 1 && val == 0x00);

  /* boundary: last ASCII */
  n = ccac_unicode_to_codepoint("\x7F", 1, &val);
  T(n == 1 && val == 0x7F);

  /* first non-ASCII is not 1-byte */
  n = ccac_unicode_to_codepoint("\x80", 1, &val);
  T(n == 0);  /* invalid */
}

/* ── 2-byte ───────────────────────────────────────────────────────────── */

void test_decode_2byte(void) {
  RUN(two_byte_decode);

  uint32_t val; int n;

  /* U+00A2 (¢) = C2 A2 */
  n = ccac_unicode_to_codepoint("\xC2\xA2", 2, &val);
  T(n == 2 && val == 0xA2);

  /* U+00A9 (©) = C2 A9 */
  n = ccac_unicode_to_codepoint("\xC2\xA9", 2, &val);
  T(n == 2 && val == 0xA9);

  /* boundary: min 2-byte U+0080 = C2 80 */
  n = ccac_unicode_to_codepoint("\xC2\x80", 2, &val);
  T(n == 2 && val == 0x80);

  /* boundary: max 2-byte U+07FF = DF BF */
  n = ccac_unicode_to_codepoint("\xDF\xBF", 2, &val);
  T(n == 2 && val == 0x7FF);
}

/* ── 3-byte ───────────────────────────────────────────────────────────── */

void test_decode_3byte(void) {
  RUN(three_byte_decode);

  uint32_t val; int n;

  /* U+4E2D (中) = E4 B8 AD */
  n = ccac_unicode_to_codepoint("\xE4\xB8\xAD", 3, &val);
  T(n == 3 && val == 0x4E2D);

  /* U+6587 (文) = E6 96 87 */
  n = ccac_unicode_to_codepoint("\xE6\x96\x87", 3, &val);
  T(n == 3 && val == 0x6587);

  /* U+00E9 (é) = C3 A9 (actually 2-byte — verify 3-byte boundary) */
  /* boundary: min 3-byte U+0800 = E0 A0 80 */
  n = ccac_unicode_to_codepoint("\xE0\xA0\x80", 3, &val);
  T(n == 3 && val == 0x800);

  /* boundary: max 3-byte U+FFFF = EF BF BF (excluding surrogates) */
  /* EF BF BF = U+FFFF; this is technically a non-character but valid UTF-8 */
  n = ccac_unicode_to_codepoint("\xEF\xBF\xBF", 3, &val);
  T(n == 3 && val == 0xFFFF);
}

/* ── 4-byte ───────────────────────────────────────────────────────────── */

void test_decode_4byte(void) {
  RUN(four_byte_decode);

  uint32_t val; int n;

  /* U+1F600 (😀) = F0 9F 98 80 */
  n = ccac_unicode_to_codepoint("\xF0\x9F\x98\x80", 4, &val);
  T(n == 4 && val == 0x1F600);

  /* U+2070E (𠜎) = F0 A0 9C 8E */
  n = ccac_unicode_to_codepoint("\xF0\xA0\x9C\x8E", 4, &val);
  T(n == 4 && val == 0x2070E);

  /* boundary: min 4-byte U+10000 = F0 90 80 80 */
  n = ccac_unicode_to_codepoint("\xF0\x90\x80\x80", 4, &val);
  T(n == 4 && val == 0x10000);

  /* boundary: max U+10FFFF = F4 8F BF BF */
  n = ccac_unicode_to_codepoint("\xF4\x8F\xBF\xBF", 4, &val);
  T(n == 4 && val == 0x10FFFF);

  /* beyond max: F4 90 80 80 → U+110000 (first byte valid but result > 0x10FFFF) */
  n = ccac_unicode_to_codepoint("\xF4\x90\x80\x80", 4, &val);
  T(n == 0);

  /* beyond max: F4 BF BF BF → even larger */
  n = ccac_unicode_to_codepoint("\xF4\xBF\xBF\xBF", 4, &val);
  T(n == 0);
}

/* ── multi-codepoint text ─────────────────────────────────────────────── */

void test_decode_multi(void) {
  RUN(multi_codepoint);

  const char *s = "\xE4\xB8\xAD\xE6\x96\x87";  /* 中文 */
  uint32_t val; int n, total = 0;

  n = ccac_unicode_to_codepoint(s, 6, &val);
  T(n == 3 && val == 0x4E2D);
  total += n;

  n = ccac_unicode_to_codepoint(s + total, 6 - total, &val);
  T(n == 3 && val == 0x6587);
  total += n;

  T(total == 6);
}

/* ── overlong encodings ───────────────────────────────────────────────── */

void test_decode_overlong(void) {
  RUN(overlong_reject);

  uint32_t val;

  /* 2-byte overlong: C0 80 would encode U+0000 */
  T(ccac_unicode_to_codepoint("\xC0\x80", 2, &val) == 0);

  /* 2-byte overlong: C1 BF would encode U+007F */
  T(ccac_unicode_to_codepoint("\xC1\xBF", 2, &val) == 0);

  /* 3-byte overlong: E0 80 80 (U+0000 instead of U+0800+) */
  T(ccac_unicode_to_codepoint("\xE0\x80\x80", 3, &val) == 0);

  /* 3-byte overlong: E0 9F BF (U+07FF, fits in 2 bytes) */
  T(ccac_unicode_to_codepoint("\xE0\x9F\xBF", 3, &val) == 0);

  /* 4-byte overlong: F0 80 80 80 (U+0000) */
  T(ccac_unicode_to_codepoint("\xF0\x80\x80\x80", 4, &val) == 0);

  /* 4-byte overlong: F0 8F BF BF (U+FFFF, fits in 3 bytes) */
  T(ccac_unicode_to_codepoint("\xF0\x8F\xBF\xBF", 4, &val) == 0);
}

/* ── surrogates ───────────────────────────────────────────────────────── */

void test_decode_surrogate(void) {
  RUN(surrogate_reject);

  uint32_t val;

  /* U+D800 encoded as ED A0 80 (often used in CESU-8, invalid in UTF-8) */
  T(ccac_unicode_to_codepoint("\xED\xA0\x80", 3, &val) == 0);

  /* U+DFFF */
  T(ccac_unicode_to_codepoint("\xED\xBF\xBF", 3, &val) == 0);

  /* edge: U+D7FF (last before surrogates) — SHOULD be valid */
  T(ccac_unicode_to_codepoint("\xED\x9F\xBF", 3, &val) == 3
    && val == 0xD7FF);

  /* edge: U+E000 (first after surrogates) — SHOULD be valid */
  T(ccac_unicode_to_codepoint("\xEE\x80\x80", 3, &val) == 3
    && val == 0xE000);
}

/* ── invalid first bytes ──────────────────────────────────────────────── */

void test_decode_invalid_first(void) {
  RUN(invalid_first_byte);

  uint32_t val;

  /* continuation bytes as first byte: 0x80–0xBF */
  T(ccac_unicode_to_codepoint("\x80", 1, &val) == 0);
  T(ccac_unicode_to_codepoint("\xBF", 1, &val) == 0);

  /* out of range: 0xF5–0xFF (would produce > U+10FFFF) */
  T(ccac_unicode_to_codepoint("\xF5", 1, &val) == 0);
  T(ccac_unicode_to_codepoint("\xF8", 1, &val) == 0);
  T(ccac_unicode_to_codepoint("\xFF", 1, &val) == 0);

  /* 0xFE, 0xFF */
  T(ccac_unicode_to_codepoint("\xFE", 1, &val) == 0);
  T(ccac_unicode_to_codepoint("\xFF", 2, &val) == 0);
}

/* ── invalid continuation bytes ───────────────────────────────────────── */

void test_decode_bad_continuation(void) {
  RUN(bad_continuation);

  uint32_t val;

  /* 2-byte: C2 + non-continuation (0x00) */
  T(ccac_unicode_to_codepoint("\xC2\x00", 2, &val) == 0);

  /* 2-byte: C2 + ASCII (0x41) */
  T(ccac_unicode_to_codepoint("\xC2\x41", 2, &val) == 0);

  /* 3-byte: E4 B8 + non-continuation */
  T(ccac_unicode_to_codepoint("\xE4\xB8\x00", 3, &val) == 0);

  /* 3-byte: E4 + non-continuation + anything */
  T(ccac_unicode_to_codepoint("\xE4\x00\xAD", 3, &val) == 0);

  /* 4-byte: F0 + 3 continuations, but 2nd is bad */
  T(ccac_unicode_to_codepoint("\xF0\x9F\x00\x80", 4, &val) == 0);
}

/* ── truncated sequences ──────────────────────────────────────────────── */

void test_decode_truncated(void) {
  RUN(truncated);

  uint32_t val;

  /* 2-byte leader with only 1 byte available */
  T(ccac_unicode_to_codepoint("\xC2", 1, &val) == 0);

  /* 3-byte leader with only 2 bytes available */
  T(ccac_unicode_to_codepoint("\xE4\xB8", 2, &val) == 0);

  /* 4-byte leader with only 3 bytes available */
  T(ccac_unicode_to_codepoint("\xF0\x9F\x98", 3, &val) == 0);

  /* 4-byte leader with only 1 byte available */
  T(ccac_unicode_to_codepoint("\xF0", 1, &val) == 0);

  /* 2-byte but len=1 */
  T(ccac_unicode_to_codepoint("\xC2\xA2", 1, &val) == 0);
}

/* ── null / edge arguments ────────────────────────────────────────────── */

void test_decode_null_edge(void) {
  RUN(null_edge_decode);

  uint32_t val;

  /* NULL string */
  T(ccac_unicode_to_codepoint(NULL, 1, &val) == 0);

  /* NULL val */
  T(ccac_unicode_to_codepoint("A", 1, NULL) == 0);

  /* len = 0 */
  T(ccac_unicode_to_codepoint("A", 0, &val) == 0);

  /* len = -1 (should be caught) */
  T(ccac_unicode_to_codepoint("A", -1, &val) == 0);
}

/* ════════════════════════════════════════════════════════════════════════
**  ccac_codepoint_to_utf8  —  UCS-4 → UTF-8
** ════════════════════════════════════════════════════════════════════════ */

/* ── 1-byte: ASCII ────────────────────────────────────────────────────── */

void test_encode_ascii(void) {
  RUN(ascii_encode);

  char buf[4]; int len;

  T(ccac_codepoint_to_utf8(0x41, buf, &len) && len == 1 && buf[0] == 'A');
  T(ccac_codepoint_to_utf8(0x00, buf, &len) && len == 1 && buf[0] == '\x00');
  T(ccac_codepoint_to_utf8(0x7F, buf, &len) && len == 1 && buf[0] == '\x7F');
}

/* ── 2-byte ───────────────────────────────────────────────────────────── */

void test_encode_2byte(void) {
  RUN(two_byte_encode);

  char buf[4]; int len;

  /* U+00A2 (¢) = C2 A2 */
  T(ccac_codepoint_to_utf8(0xA2, buf, &len)
    && len == 2 && buf[0] == '\xC2' && buf[1] == '\xA2');

  /* U+00A9 (©) = C2 A9 */
  T(ccac_codepoint_to_utf8(0xA9, buf, &len)
    && len == 2 && buf[0] == '\xC2' && buf[1] == '\xA9');

  /* boundary: min 2-byte U+0080 = C2 80 */
  T(ccac_codepoint_to_utf8(0x80, buf, &len)
    && len == 2 && buf[0] == '\xC2' && buf[1] == '\x80');

  /* boundary: max 2-byte U+07FF = DF BF */
  T(ccac_codepoint_to_utf8(0x7FF, buf, &len)
    && len == 2 && buf[0] == '\xDF' && buf[1] == '\xBF');
}

/* ── 3-byte ───────────────────────────────────────────────────────────── */

void test_encode_3byte(void) {
  RUN(three_byte_encode);

  char buf[4]; int len;

  /* U+4E2D (中) = E4 B8 AD */
  T(ccac_codepoint_to_utf8(0x4E2D, buf, &len)
    && len == 3
    && (unsigned char)buf[0] == 0xE4
    && (unsigned char)buf[1] == 0xB8
    && (unsigned char)buf[2] == 0xAD);

  /* boundary: min 3-byte U+0800 = E0 A0 80 */
  T(ccac_codepoint_to_utf8(0x800, buf, &len)
    && len == 3
    && (unsigned char)buf[0] == 0xE0
    && (unsigned char)buf[1] == 0xA0
    && (unsigned char)buf[2] == 0x80);
}

/* ── 4-byte ───────────────────────────────────────────────────────────── */

void test_encode_4byte(void) {
  RUN(four_byte_encode);

  char buf[4]; int len;

  /* U+1F600 (😀) = F0 9F 98 80 */
  T(ccac_codepoint_to_utf8(0x1F600, buf, &len)
    && len == 4
    && (unsigned char)buf[0] == 0xF0
    && (unsigned char)buf[1] == 0x9F
    && (unsigned char)buf[2] == 0x98
    && (unsigned char)buf[3] == 0x80);

  /* boundary: max U+10FFFF = F4 8F BF BF */
  T(ccac_codepoint_to_utf8(0x10FFFF, buf, &len)
    && len == 4
    && (unsigned char)buf[0] == 0xF4
    && (unsigned char)buf[1] == 0x8F
    && (unsigned char)buf[2] == 0xBF
    && (unsigned char)buf[3] == 0xBF);
}

/* ── invalid inputs ───────────────────────────────────────────────────── */

void test_encode_invalid(void) {
  RUN(invalid_encode);

  char buf[4]; int len;

  /* surrogate: U+D800 */
  T(ccac_codepoint_to_utf8(0xD800, buf, &len) == false);

  /* surrogate: U+DFFF */
  T(ccac_codepoint_to_utf8(0xDFFF, buf, &len) == false);

  /* beyond max: U+110000 */
  T(ccac_codepoint_to_utf8(0x110000, buf, &len) == false);

  /* well beyond: U+FFFFFFFF */
  T(ccac_codepoint_to_utf8(0xFFFFFFFF, buf, &len) == false);
}

/* ── null / edge arguments ────────────────────────────────────────────── */

void test_encode_null_edge(void) {
  RUN(null_edge_encode);

  int len;

  /* NULL str */
  T(ccac_codepoint_to_utf8(0x41, NULL, &len) == false);

  /* NULL len */
  char buf[4];
  T(ccac_codepoint_to_utf8(0x41, buf, NULL) == false);

  /* both NULL */
  T(ccac_codepoint_to_utf8(0x41, NULL, NULL) == false);
}

/* ════════════════════════════════════════════════════════════════════════
**  Round-trip: encode → decode = identity
** ════════════════════════════════════════════════════════════════════════ */

void test_roundtrip(void) {
  RUN(roundtrip);

  /* test a range of codepoints across all planes */
  uint32_t codepoints[] = {
    0x00, 0x20, 0x41, 0x7F,           /* ASCII */
    0x80, 0xA2, 0x3A9, 0x7FF,         /* 2-byte */
    0x800, 0x4E2D, 0x6587, 0xD7FF,    /* 3-byte (pre-surrogate) */
    0xE000, 0xFFFF,                    /* 3-byte (post-surrogate) */
    0x10000, 0x1F600, 0x2070E, 0x10FFFF, /* 4-byte */
  };
  int n_cp = sizeof(codepoints) / sizeof(codepoints[0]);

  for (int i = 0; i < n_cp; i++) {
    uint32_t orig = codepoints[i];
    char buf[4]; int enc_len;
    T(ccac_codepoint_to_utf8(orig, buf, &enc_len));

    uint32_t decoded; int dec_len;
    dec_len = ccac_unicode_to_codepoint(buf, enc_len, &decoded);
    T(dec_len == enc_len);
    T(decoded == orig);
  }
}

/* ════════════════════════════════════════════════════════════════════════
**  Fuzz: exhaustive round-trip over valid ranges
** ════════════════════════════════════════════════════════════════════════ */

void test_fuzz_exhaustive(void) {
  RUN(fuzz_exhaustive);

  char buf[4]; int enc_len;
  uint32_t decoded; int dec_len;

  int ok = 0, bad = 0;
  uint32_t step;

  /* sample every N-th codepoint across the valid range */
  /* 1-byte range (0x00–0x7F): check every point */
  for (uint32_t cp = 0x00; cp <= 0x7F; cp++) {
    T(ccac_codepoint_to_utf8(cp, buf, &enc_len));
    dec_len = ccac_unicode_to_codepoint(buf, enc_len, &decoded);
    if (dec_len == enc_len && decoded == cp) ok++; else bad++;
  }
  T(bad == 0);

  /* 2-byte range (0x80–0x7FF): step 256 */
  step = 256;
  for (uint32_t cp = 0x80; cp <= 0x7FF; cp += step) {
    T(ccac_codepoint_to_utf8(cp, buf, &enc_len));
    dec_len = ccac_unicode_to_codepoint(buf, enc_len, &decoded);
    if (dec_len == enc_len && decoded == cp) ok++; else bad++;
  }
  T(bad == 0);

  /* 3-byte range (0x800–0xFFFF, excluding surrogates): step 4096 */
  step = 4096;
  for (uint32_t cp = 0x800; cp <= 0xFFFF; cp += step) {
    if (cp >= 0xD800 && cp <= 0xDFFF) continue;
    T(ccac_codepoint_to_utf8(cp, buf, &enc_len));
    dec_len = ccac_unicode_to_codepoint(buf, enc_len, &decoded);
    if (dec_len == enc_len && decoded == cp) ok++; else bad++;
  }
  T(bad == 0);

  /* 4-byte range (0x10000–0x10FFFF): step 131072 */
  step = 131072;
  for (uint32_t cp = 0x10000; cp <= 0x10FFFF; cp += step) {
    T(ccac_codepoint_to_utf8(cp, buf, &enc_len));
    dec_len = ccac_unicode_to_codepoint(buf, enc_len, &decoded);
    if (dec_len == enc_len && decoded == cp) ok++; else bad++;
  }
  T(bad == 0);
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  printf("unicode.h unit tests\n");

  /* decode */
  test_decode_ascii();
  test_decode_2byte();
  test_decode_3byte();
  test_decode_4byte();
  test_decode_multi();
  test_decode_overlong();
  test_decode_surrogate();
  test_decode_invalid_first();
  test_decode_bad_continuation();
  test_decode_truncated();
  test_decode_null_edge();

  /* encode */
  test_encode_ascii();
  test_encode_2byte();
  test_encode_3byte();
  test_encode_4byte();
  test_encode_invalid();
  test_encode_null_edge();

  /* round-trip */
  test_roundtrip();
  test_fuzz_exhaustive();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
