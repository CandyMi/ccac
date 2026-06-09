/*
**  Test suite for ccac (Aho-Corasick automaton)
**  Build:  cc -std=c99 -Wall -Wextra -I.. -o test_ccac test_ccac.c && ./test_ccac
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "../ccac.h"

/* ── test harness ─────────────────────────────────────────────────────── */
static int passed, failed;
#define TEST(name) static void test_##name(void)
#define ASSERT(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) do { printf("  %s\n", #name); test_##name(); } while(0)

/* ── helper ───────────────────────────────────────────────────────────── */
static bool has_match(ccac_t *ac, const char *text, size_t len) {
  int n = 0;
  ccac_match(ac, text, len, NULL, &n);
  return n > 0;
}

/* ════════════════════════════════════════════════════════════════════════
**  Tests
** ════════════════════════════════════════════════════════════════════════ */

/* ── lifecycle ────────────────────────────────────────────────────────── */

TEST(init_destroy) {
  ccac_t ac;
  ASSERT(ccac_init(&ac));
  ASSERT(ac.size == 0);
  ASSERT(ac.root != NULL);
  ccac_destroy(&ac);
}

TEST(init_null) {
  ASSERT(!ccac_init(NULL));
}

TEST(destroy_null) {
  ccac_destroy(NULL); /* must not crash */
}

/* ── build ────────────────────────────────────────────────────────────── */

TEST(build_empty) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "", 0, '\n'));
  ASSERT(ac.size == 0);
  ccac_destroy(&ac);
}

TEST(build_empty_segments) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "\n\n\n", 3, '\n'));
  ASSERT(ac.size == 0);
  ccac_destroy(&ac);
}

TEST(build_single_word) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "hello", 5, '\n'));
  ASSERT(ac.size == 1);
  ccac_destroy(&ac);
}

TEST(build_duplicate_words) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "abc\nabc\nabc\n", 11, '\n'));
  ASSERT(ac.size == 1);  /* duplicates not counted */
  ccac_destroy(&ac);
}

TEST(build_comma_delim) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "a,b,c", 5, ','));
  ASSERT(ac.size == 3);
  ccac_destroy(&ac);
}

TEST(build_pipe_delim) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(ccac_build(&ac, "foo|bar|baz", 11, '|'));
  ASSERT(ac.size == 3);
  ccac_destroy(&ac);
}

/* ── find: basic ──────────────────────────────────────────────────────── */

TEST(find_empty_text) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "hello\nworld\n", 12, '\n');
  ASSERT(!has_match(&ac, "", 0));
  ccac_destroy(&ac);
}

TEST(find_no_match) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "hello\nworld\n", 12, '\n');
  ASSERT(!has_match(&ac, "xyz", 3));
  ccac_destroy(&ac);
}

TEST(find_exact_single) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "hello\n", 6, '\n');
  const char *t = "hello";
  ccac_match_t m[4]; int n = 4;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 1);
  ASSERT(m[0].s == 0 && m[0].e == 5);
  ASSERT(memcmp(t + m[0].s, "hello", 5) == 0);
  ccac_destroy(&ac);
}

TEST(find_multiple_non_overlapping) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "ab\ncd\n", 6, '\n');
  const char *t = "abXcd";
  ccac_match_t m[4]; int n = 4;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 2);
  ccac_destroy(&ac);
}

/* ── find: overlapping / fail-chain ───────────────────────────────────── */

TEST(find_overlapping) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "ab\nbc\n", 6, '\n');
  const char *t = "abc";
  ccac_match_t m[4]; int n = 4;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 2);  /* "ab" at [0,2), "bc" at [1,3) */
  int found_ab = 0, found_bc = 0;
  for (int i = 0; i < n; i++) {
    if (m[i].s == 0 && m[i].e == 2) found_ab = 1;
    if (m[i].s == 1 && m[i].e == 3) found_bc = 1;
  }
  ASSERT(found_ab);
  ASSERT(found_bc);
  ccac_destroy(&ac);
}

TEST(find_fail_chain) {
  /* classic: "ushers" with {"he","she","his","hers"} */
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "he\nshe\nhis\nhers\n", 16, '\n');
  const char *t = "ushers";
  ccac_match_t m[8]; int n = 8;
  ccac_match(&ac, t, strlen(t), m, &n);
  /* expect: she[1,4), he[2,4), hers[2,6) */
  ASSERT(n == 3);
  int found_she = 0, found_he = 0, found_hers = 0;
  for (int i = 0; i < n; i++) {
    if (m[i].s == 1 && m[i].e == 4) found_she = 1;
    if (m[i].s == 2 && m[i].e == 4) found_he = 1;
    if (m[i].s == 2 && m[i].e == 6) found_hers = 1;
  }
  ASSERT(found_she && found_he && found_hers);
  ccac_destroy(&ac);
}

/* ── find: nmatch truncation ──────────────────────────────────────────── */

TEST(find_truncation) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "a\nb\nc\nd\ne\n", 10, '\n');
  const char *t = "abcde";
  ccac_match_t m[3]; int n = 3;  /* only room for 3 */
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 3);  /* truncated */
  ccac_destroy(&ac);
}

TEST(find_test_mode) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "hello\nworld\n", 12, '\n');
  /* test mode: NULL matches → existence check */
  int n = 0;
  ccac_match(&ac, "hello world", 11, NULL, &n);
  ASSERT(n == 1);  /* found at least one */
  n = 0;
  ccac_match(&ac, "xyz", 3, NULL, &n);
  ASSERT(n == 0);  /* nothing found */
  ccac_destroy(&ac);
}

/* ── UTF-8 / Chinese ──────────────────────────────────────────────────── */

TEST(utf8_chinese_single) {
  ccac_t ac; ccac_init(&ac);
  const char *d = "测试\n";
  ccac_build(&ac, d, strlen(d), '\n');
  const char *t = "这是一个测试文本";
  ccac_match_t m[4]; int n = 4;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 1);
  ASSERT(memcmp(t + m[0].s, "测试", (size_t)(m[0].e - m[0].s)) == 0);
  ccac_destroy(&ac);
}

TEST(utf8_chinese_multiple) {
  ccac_t ac; ccac_init(&ac);
  const char *d = "敏感\n词汇\n过滤\n";
  ccac_build(&ac, d, strlen(d), '\n');
  const char *t = "这是敏感词汇需要过滤处理";
  ccac_match_t m[8]; int n = 8;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 3);
  ccac_destroy(&ac);
}

TEST(utf8_chinese_overlap) {
  /* "中国人" and "中国" overlapping */
  ccac_t ac; ccac_init(&ac);
  const char *d = "中国\n中国人\n";
  ccac_build(&ac, d, strlen(d), '\n');
  const char *t = "我是中国人";
  ccac_match_t m[8]; int n = 8;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 2);  /* 中国 + 中国人 both found */
  ccac_destroy(&ac);
}

/* ── null / edge ──────────────────────────────────────────────────────── */

TEST(build_null) {
  ccac_t ac; ccac_init(&ac);
  ASSERT(!ccac_build(&ac, NULL, 10, '\n'));
  ASSERT(!ccac_build(NULL, "x", 1, '\n'));
  /* ac still valid after failed build */
  ccac_destroy(&ac);
}

TEST(find_null) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "hello\n", 6, '\n');
  int n = 10;
  ASSERT(!ccac_match(NULL, "hello", 5, NULL, &n));
  ASSERT(!ccac_match(&ac, NULL, 5, NULL, &n));
  ASSERT(!ccac_match(&ac, "hello", 5, NULL, NULL));
  ccac_destroy(&ac);
}

/* ── single-character words ───────────────────────────────────────────── */

TEST(single_char) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "a\nb\nc\n", 6, '\n');
  ASSERT(has_match(&ac, "abc", 3));
  ASSERT(has_match(&ac, "a b c", 5));
  ccac_destroy(&ac);
}

/* ── repeated pattern ─────────────────────────────────────────────────── */

TEST(repeated_pattern) {
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "ab\n", 3, '\n');
  const char *t = "abababab";
  ccac_match_t m[8]; int n = 8;
  ccac_match(&ac, t, strlen(t), m, &n);
  ASSERT(n == 4);
  for (int i = 0; i < n; i++)
    ASSERT(m[i].e - m[i].s == 2);
  ccac_destroy(&ac);
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  printf("ccac unit tests\n");

  RUN(init_destroy);
  RUN(init_null);
  RUN(destroy_null);

  RUN(build_empty);
  RUN(build_empty_segments);
  RUN(build_single_word);
  RUN(build_duplicate_words);
  RUN(build_comma_delim);
  RUN(build_pipe_delim);

  RUN(find_empty_text);
  RUN(find_no_match);
  RUN(find_exact_single);
  RUN(find_multiple_non_overlapping);
  RUN(find_overlapping);
  RUN(find_fail_chain);
  RUN(find_truncation);
  RUN(find_test_mode);
  RUN(find_null);

  RUN(utf8_chinese_single);
  RUN(utf8_chinese_multiple);
  RUN(utf8_chinese_overlap);

  RUN(single_char);
  RUN(repeated_pattern);

  RUN(build_null);

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
