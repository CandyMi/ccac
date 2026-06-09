/*
**  Performance comparison: ccac vs. naive(strstr) vs. PCRE2
**
**  Build:
**    cc -std=c99 -O2 -I. -o bench_ccac bench_ccac.c \
**       $(pcre2-config --cflags --libs-posix 2>/dev/null || echo '-lpcre2-8')
**
**  Run:
**    python3 scripts/gen_stress_text.py words.zh.txt stress_text.txt 3
**    ./bench_ccac words.zh.txt stress_text.txt
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "ccac.h"

/* ── file helpers ─────────────────────────────────────────────────────── */

static char *read_file(const char *path, size_t *out_len) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); return NULL; }
  char *buf = (char *)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return NULL; }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[n] = '\0';
  *out_len = n;
  return buf;
}

static double now_ms(void) {
  return (double)clock() / (double)CLOCKS_PER_SEC * 1000.0;
}

/* ── helper: count newline-separated words ────────────────────────────── */

static size_t count_lines(const char *s, size_t len) {
  size_t n = 0;
  for (size_t i = 0; i < len; i++)
    if (s[i] == '\n') n++;
  return n;
}

/* ── helper: split words into array ───────────────────────────────────── */

static char **split_words(const char *dict, size_t dict_len, size_t *count) {
  *count = count_lines(dict, dict_len);
  char **arr = (char **)malloc(*count * sizeof(char *));
  if (!arr) { *count = 0; return NULL; }
  const char *p = dict, *end = dict + dict_len;
  size_t idx = 0;
  while (p < end) {
    const char *seg = p;
    while (p < end && *p != '\n') p++;
    size_t slen = (size_t)(p - seg);
    if (p < end) p++;
    if (slen == 0) continue;
    arr[idx] = (char *)malloc(slen + 1);
    if (!arr[idx]) break;
    memcpy(arr[idx], seg, slen);
    arr[idx][slen] = '\0';
    idx++;
  }
  *count = idx;
  return arr;
}

/* ════════════════════════════════════════════════════════════════════════
**  Method 1: ccac (Aho-Corasick)
** ════════════════════════════════════════════════════════════════════════ */

static double bench_ccac(const char *dict, size_t dict_len,
                         const char *text, size_t text_len,
                         int *out_matches) {
  ccac_t ac;
  ccac_init(&ac);

  double t0 = now_ms();
  if (!ccac_build(&ac, dict, dict_len, '\n')) {
    fprintf(stderr, "ccac build failed\n");
    ccac_destroy(&ac);
    return -1;
  }
  double t_build = now_ms() - t0;

  int n = 2000000000;
  t0 = now_ms();
  ccac_match(&ac, text, text_len, NULL, &n);
  double t_search = now_ms() - t0;

  *out_matches = n;
  ccac_destroy(&ac);

  printf("  ccac          build %7.0f ms | search %7.0f ms | %d matches\n",
         t_build, t_search, n);
  return t_build + t_search;
}

/* ════════════════════════════════════════════════════════════════════════
**  Method 2: array iteration + strstr  (naive baseline)
** ════════════════════════════════════════════════════════════════════════ */

static double bench_naive(const char *dict, size_t dict_len,
                          const char *text, size_t text_len,
                          int *out_matches) {
  int matches = 0;
  double t0, t_build, t_search;

  t0 = now_ms();
  size_t nw;
  char **words = split_words(dict, dict_len, &nw);
  if (!words) return -1;

  /* dedup by sorting + compacting */
  /* skip complex dedup — just warn about potential overcount */
  t_build = now_ms() - t0;

  /* strstr each word over the whole text */
  t0 = now_ms();
  for (size_t i = 0; i < nw; i++) {
    const char *hay = text;
    const char *needle = words[i];
    size_t nlen = strlen(needle);
    if (nlen == 0) continue;
    while ((hay = (const char *)memmem(hay, text_len - (size_t)(hay - text),
                                       needle, nlen)) != NULL) {
      matches++;
      hay++;
    }
  }
  t_search = now_ms() - t0;

  *out_matches = matches;

  for (size_t i = 0; i < nw; i++) free(words[i]);
  free(words);

  printf("  naive(strstr) build %7.0f ms | search %7.0f ms | %d matches\n",
         t_build, t_search, matches);
  return t_build + t_search;
}

/* ════════════════════════════════════════════════════════════════════════
**  Method 3: PCRE2  (single alternation regex)
** ════════════════════════════════════════════════════════════════════════ */

static double bench_pcre2(const char *dict, size_t dict_len,
                          const char *text, size_t text_len,
                          int *out_matches) {
  int errcode;
  PCRE2_SIZE erroffset;

  double t0 = now_ms();

  /* build pattern: word1|word2|...|wordN */
  /* estimate size: sum of all word lengths + separators */
  size_t est = dict_len * 2 + 16;
  char *pattern = (char *)malloc(est);
  if (!pattern) return -1;

  size_t pat_len = 0;
  const char *p = dict, *end = dict + dict_len;
  int first = 1;
  while (p < end) {
    const char *seg = p;
    while (p < end && *p != '\n') p++;
    size_t slen = (size_t)(p - seg);
    if (p < end) p++;
    if (slen == 0) continue;

    /* escape regex metacharacters in the word */
    if (!first) { pattern[pat_len++] = '|'; }
    first = 0;

    for (size_t i = 0; i < slen; i++) {
      char c = seg[i];
      if (c == '\\' || c == '|' || c == '(' || c == ')' ||
          c == '[' || c == ']' || c == '{' || c == '}' ||
          c == '.' || c == '^' || c == '$' || c == '*' ||
          c == '+' || c == '?' || c == '/') {
        if (pat_len + 2 > est) { est *= 2; pattern = (char *)realloc(pattern, est); }
        pattern[pat_len++] = '\\';
      }
      if (pat_len + 1 > est) { est *= 2; pattern = (char *)realloc(pattern, est); }
      pattern[pat_len++] = c;
    }
  }
  pattern[pat_len] = '\0';

  /* compile */
  pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, pat_len,
                                  PCRE2_NO_UTF_CHECK, &errcode, &erroffset, NULL);
  double t_build = now_ms() - t0;

  if (!re) {
    PCRE2_UCHAR errbuf[256];
    pcre2_get_error_message(errcode, errbuf, sizeof(errbuf));
    fprintf(stderr, "PCRE2 compile error at %d: %s\n", (int)erroffset, errbuf);
    fprintf(stderr, "... near: %.*s\n", 80, pattern + (erroffset > 40 ? erroffset - 40 : 0));
    free(pattern);
    return -1;
  }

  free(pattern);

  /* match */
  t0 = now_ms();
  pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
  int matches = 0;
  size_t offset = 0;

  while (offset < text_len) {
    int rc = pcre2_match(re, (PCRE2_SPTR)text, text_len, offset, 0, md, NULL);
    if (rc < 0) break;
    matches++;
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);
    size_t next = (size_t)ovector[1];
    if (next <= offset) next = offset + 1;  /* avoid infinite loop on zero-length */
    offset = next;
  }
  double t_search = now_ms() - t0;

  *out_matches = matches;
  pcre2_match_data_free(md);
  pcre2_code_free(re);

  printf("  PCRE2         build %7.0f ms | search %7.0f ms | %d matches\n",
         t_build, t_search, matches);
  return t_build + t_search;
}

/* ════════════════════════════════════════════════════════════════════════
**  main
** ════════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
  const char *word_file = argc > 1 ? argv[1] : "words.zh.txt";
  const char *text_file = argc > 2 ? argv[2] : "stress_text.txt";
  int naive_limit = argc > 3 ? atoi(argv[3]) : 1000;
  size_t text_cap  = (size_t)(argc > 4 ? atol(argv[4]) : 100000);

  printf("=== ccac vs naive(strstr) vs PCRE2 ===\n");
  printf("dictionary : %s\n", word_file);
  printf("text       : %s\n\n", text_file);

  /* load */
  size_t dict_len, text_len;
  char *dict = read_file(word_file, &dict_len);
  char *text = read_file(text_file, &text_len);
  if (!dict || !text) {
    fprintf(stderr, "Cannot read input files\n");
    free(dict); free(text);
    return 1;
  }

  size_t word_count = count_lines(dict, dict_len);
  printf("words  : %zu\n", word_count);
  printf("text   : %.2f MB (%zu bytes)\n", text_len / (1024.0 * 1024.0), text_len);
  printf("naive/PCRE2 capped at %d words + %zu KB text\n\n",
         naive_limit, text_cap / 1024);

  /* ── ccac: full dict, full text ─────────────────────────────────── */
  int m_ccac, m_map, m_pcre2;
  printf("── ccac (full: %zu words, %.2f MB) ──\n", word_count,
         text_len / (1024.0 * 1024.0));
  double t_ccac = bench_ccac(dict, dict_len, text, text_len, &m_ccac);

  /* ── subset dict/text for slow methods ──────────────────────────── */
  /* trim dict to first N lines */
  size_t trim_at = 0;
  int lines = 0;
  while (lines < naive_limit && trim_at < dict_len)
    if (dict[trim_at++] == '\n') lines++;
  size_t sub_dict_len = trim_at;
  size_t sub_text_len = text_len < text_cap ? text_len : text_cap;

  printf("\n── naive(strstr) (capped: %d words, %zu KB) ──\n",
         naive_limit, sub_text_len / 1024);
  double t_map = bench_naive(dict, sub_dict_len, text, sub_text_len, &m_map);

  printf("\n── PCRE2 (capped: %d words, %zu KB) ──\n",
         naive_limit, sub_text_len / 1024);
  double t_pcre2 = bench_pcre2(dict, sub_dict_len, text, sub_text_len, &m_pcre2);

  /* ── summary ────────────────────────────────────────────────────── */
  printf("\n── Summary ──\n");
  printf("  ccac  full  (%zu w, %.2f MB) : %8.0f ms total\n",
         word_count, text_len / (1024.0 * 1024.0), t_ccac);
  printf("  naive capped (%d w, %zu KB)  : %8.0f ms total\n",
         naive_limit, sub_text_len / 1024, t_map);
  printf("  PCRE2 capped (%d w, %zu KB)  : %8.0f ms total\n",
         naive_limit, sub_text_len / 1024, t_pcre2);

  /* project naive to full scale */
  double naive_proj = t_map * ((double)word_count / naive_limit)
                            * ((double)text_len / sub_text_len);
  printf("\n  naive projected (full scale) : %.0f ms  (%.0f min)\n",
         naive_proj, naive_proj / 60000.0);
  printf("  ccac vs naive(full)          : %.0f× faster\n",
         naive_proj / t_ccac);
  if (t_pcre2 > 0) {
    // double pcre2_proj = t_pcre2 * ((double)text_len / sub_text_len);
    printf("  ccac vs PCRE2(capped text)   : %.1f× faster\n",
           t_pcre2 / t_ccac);
  }

  printf("\n  recall (capped, vs ccac full = %d):\n", m_ccac);
  if (m_map >= 0)
    printf("    naive(strstr) : %d matches\n", m_map);
  if (m_pcre2 >= 0)
    printf("    PCRE2         : %d matches\n", m_pcre2);

  free(dict);
  free(text);
  return 0;
}
