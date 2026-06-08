/*
**  Robustness tests for ccac — edge cases, stress, fuzzing.
*/
#include "ccac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int passed, failed;
#define T(cond) do { \
  if (!(cond)) { printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failed++; } \
  else passed++; \
} while(0)
#define RUN(name) printf("  %s\n", #name)

/* ── invalid UTF-8 ───────────────────────────────────────────────────── */

void test_invalid_utf8_build(void) {
  RUN(invalid_utf8_build);
  ccac_t ac; ccac_init(&ac);
  /* 0xFF is never valid UTF-8 */
  const char bad[] = "ab\xff" "cd\n";
  int ok = ccac_build(&ac, bad, sizeof(bad)-1, '\n');
  (void)ok;  /* should not crash — just skip or fail gracefully */
  ccac_destroy(&ac);
  T(1);
}

void test_invalid_utf8_find(void) {
  RUN(invalid_utf8_find);
  ccac_t ac; ccac_init(&ac);
  ccac_build(&ac, "abc\n", 4, '\n');
  const char bad[] = "ab\xff" "cd";
  int n = 0;
  ccac_find(&ac, bad, sizeof(bad)-1, NULL, &n);
  ccac_destroy(&ac);
  T(1);  /* must not crash */
}

/* ── very long word ───────────────────────────────────────────────────── */

void test_long_word(void) {
  RUN(long_word);
  ccac_t ac; ccac_init(&ac);

  /* build a 200-byte ASCII word */
  char word[256];
  memset(word, 'x', 200);
  word[200] = '\n';
  word[201] = '\0';
  ccac_build(&ac, word, 201, '\n');

  /* search */
  char text[512];
  memset(text, 'x', 400);
  text[400] = '\0';
  int n=4; ccac_match_t m[4];
  ccac_find(&ac, text, 400, m, &n);
  T(n >= 1);  /* should find overlapping matches */
  ccac_destroy(&ac);
}

/* ── wide trie: many single-char branches from root ───────────────────── */

void test_wide_root(void) {
  RUN(wide_root);
  ccac_t ac; ccac_init(&ac);

  /* 500 distinct leading characters */
  char dict[4096]; int pos=0;
  for (int i=0; i<500; i++)
    pos += sprintf(dict+pos, "%03d\n", i);
  ccac_build(&ac, dict, (size_t)pos, '\n');
  T(ac.size == 500);

  /* search for each */
  for (int i=0; i<500; i++) {
    char needle[8];
    sprintf(needle, "%03d", i);
    int n=0;
    ccac_find(&ac, needle, 3, NULL, &n);
    T(n == 1);
  }
  ccac_destroy(&ac);
}

/* ── deep trie: shared long prefix ──────────────────────────────────── */

void test_deep_prefix(void) {
  RUN(deep_prefix);
  ccac_t ac; ccac_init(&ac);

  /* words: a, ab, abc, abcd, ... abcdefghij */
  char dict[128]; int pos=0;
  for (int len=1; len<=10; len++) {
    for (int j=0; j<len; j++) dict[pos++] = (char)('a' + j);
    dict[pos++] = '\n';
  }
  ccac_build(&ac, dict, (size_t)pos, '\n');
  T(ac.size == 10);

  /* search "abcdefghij" — should find all 10 */
  const char *t = "abcdefghij";
  int n=16; ccac_match_t m[16];
  ccac_find(&ac, t, (size_t)strlen(t), m, &n);
  T(n == 10);
  ccac_destroy(&ac);
}

/* ── rebuild cycle ────────────────────────────────────────────────────── */

void test_rebuild_cycle(void) {
  RUN(rebuild_cycle);
  ccac_t ac; ccac_init(&ac);

  for (int cyc=0; cyc<20; cyc++) {
    char tmp[256];
    sprintf(tmp, "word%d\n", cyc);
    bool dup;
    ccac_build(&ac, tmp, strlen(tmp), '\n'); (void)dup;
    /* then add a few more */
    for (int j=0; j<5; j++) {
      char w[16]; sprintf(w, "add%d_%d", cyc, j);
      ccac_add(&ac, w, strlen(w), &dup);
    }
    /* verify */
    char needle[16]; sprintf(needle, "add%d_2", cyc);
    int n=0;
    ccac_find(&ac, needle, strlen(needle), NULL, &n);
    T(n == 1);
  }
  ccac_destroy(&ac);
}

/* ── zero-length edge cases ───────────────────────────────────────────── */

void test_zero_len(void) {
  RUN(zero_len);
  ccac_t ac;

  /* init + destroy empty */
  ccac_init(&ac);
  T(ac.size == 0);
  T(ac.root != NULL);
  ccac_destroy(&ac);

  /* build empty */
  ccac_init(&ac);
  T(ccac_build(&ac, "", 0, '\n'));
  T(ac.size == 0);
  ccac_destroy(&ac);

  /* find on empty text */
  ccac_init(&ac);
  ccac_build(&ac, "abc\n", 4, '\n');
  int n = 99;
  ccac_find(&ac, "", 0, NULL, &n);
  T(n == 0);
  ccac_destroy(&ac);

  /* add zero-length word */
  ccac_init(&ac);
  bool dup;
  T(!ccac_add(&ac, "", 0, &dup));
  T(!dup);
  ccac_destroy(&ac);
}

/* ── NULL safety ──────────────────────────────────────────────────────── */

void test_null_safety(void) {
  RUN(null_safety);
  ccac_t ac; ccac_init(&ac); ccac_build(&ac, "x\n",2,'\n');
  int n;

  T(!ccac_find(NULL, "x", 1, NULL, &n));
  T(!ccac_find(&ac, NULL, 1, NULL, &n));
  T(!ccac_find(&ac, "x", 1, NULL, NULL));
  T(!ccac_build(NULL, "x", 1, '\n'));
  T(!ccac_build(&ac, NULL, 1, '\n'));
  T(!ccac_add(NULL, "x", 1, NULL));
  T(!ccac_add(&ac, NULL, 1, NULL));

  ccac_destroy(&ac);
  ccac_destroy(NULL);   /* must not crash */
  T(1);
}

/* ── match count overflow ────────────────────────────────────────────── */

void test_match_overflow(void) {
  RUN(match_overflow);
  ccac_t ac; ccac_init(&ac);

  /* dict: single char 'a'  →  every 'a' in text is a match */
  ccac_build(&ac, "a\n", 2, '\n');

  /* text: 100 'a's */
  char text[128]; memset(text, 'a', 100); text[100]='\0';

  /* request only 3 matches */
  ccac_match_t m[3]; int n = 3;
  ccac_find(&ac, text, 100, m, &n);
  T(n == 3);
  T(m[0].e - m[0].s == 1);

  /* test mode (NULL) */
  n = 0;
  ccac_find(&ac, text, 100, NULL, &n);
  T(n == 1);  /* existence only */

  ccac_destroy(&ac);
}

/* ── single-character dictionary entries ─────────────────────────────── */

void test_single_char_dict(void) {
  RUN(single_char_dict);
  ccac_t ac; ccac_init(&ac);

  /* 26 single-letter words */
  char dict[64]; int pos=0;
  for (char c='a'; c<='z'; c++) { dict[pos++]=c; dict[pos++]='\n'; }
  ccac_build(&ac, dict, (size_t)pos, '\n');
  T(ac.size == 26);

  /* text containing all letters */
  const char *t = "abcdefghijklmnopqrstuvwxyz";
  ccac_match_t m[32]; int n=32;
  ccac_find(&ac, t, strlen(t), m, &n);
  T(n == 26);

  ccac_destroy(&ac);
}

/* ── random fuzz ──────────────────────────────────────────────────────── */

void test_fuzz(void) {
  RUN(fuzz);
  srand(42);

  for (int round=0; round<100; round++) {
    ccac_t ac; ccac_init(&ac);

    /* random dictionary: 50-200 words, 1-8 chars each */
    int nw = 50 + rand() % 150;
    char dict[8192]; int dp=0;
    for (int i=0; i<nw; i++) {
      int wl = 1 + rand() % 8;
      for (int j=0; j<wl; j++)
        dict[dp++] = (char)('a' + rand() % 26);
      dict[dp++] = '\n';
    }
    ccac_build(&ac, dict, (size_t)dp, '\n');

    /* random text: 500-2000 chars */
    int tl = 500 + rand() % 1500;
    char *text = (char *)malloc((size_t)tl+1);
    for (int i=0; i<tl; i++)
      text[i] = (char)('a' + rand() % 26);
    text[tl] = '\0';

    /* test mode */
    int hit = 0;
    ccac_find(&ac, text, (size_t)tl, NULL, &hit);

    /* record up to 100 matches */
    ccac_match_t *m = (ccac_match_t *)malloc(100*sizeof(*m));
    int n = 100;
    ccac_find(&ac, text, (size_t)tl, m, &n);

    /* validate every match */
    for (int i=0; i<n; i++) {
      size_t slen = m[i].e - m[i].s;
      T(slen > 0);
      T(slen <= 8);
      T(m[i].s < (size_t)tl);
      T(m[i].e <= (size_t)tl);
    }

    free(m); free(text);
    ccac_destroy(&ac);
  }
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(void) {
  printf("ccac robustness tests\n");

  test_invalid_utf8_build();
  test_invalid_utf8_find();
  test_long_word();
  test_wide_root();
  test_deep_prefix();
  test_rebuild_cycle();
  test_zero_len();
  test_null_safety();
  test_match_overflow();
  test_single_char_dict();
  test_fuzz();

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed > 0 ? 1 : 0;
}
