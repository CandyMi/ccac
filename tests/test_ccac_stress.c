/*
**  Stress test for ccac — loads words.zh.txt (50k words),
**  builds automaton, searches generated text, validates recall.
**
**  Build:  cc -std=c99 -O2 -Wall -Wextra -I.. -o test_ccac_stress
**              test_ccac_stress.c && ./test_ccac_stress
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "../ccac.h"

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

/* ── timing helper ────────────────────────────────────────────────────── */

static double now_ms(void) {
  return (double)clock() / (double)CLOCKS_PER_SEC * 1000.0;
}

/* ── main ─────────────────────────────────────────────────────────────── */

int main(int argc, char **argv) {
  const char *word_file  = argc > 1 ? argv[1] : "words.zh.txt";
  const char *text_file  = argc > 2 ? argv[2] : "stress_text.txt";
  const char *match_file = argc > 3 ? argv[3] : NULL;  /* optional: expected matches */

  printf("=== ccac stress test ===\n\n");

  /* ── load dictionary ──────────────────────────────────────────────── */
  printf("[1/4] Loading dictionary: %s\n", word_file);
  size_t dict_len;
  char *dict = read_file(word_file, &dict_len);
  if (!dict) { fprintf(stderr, "FAIL: cannot read %s\n", word_file); return 1; }
  printf("      %zu bytes, ~%zu lines\n", dict_len,
         dict_len > 0 ? (size_t)(dict[dict_len-1] == '\n') : 0);

  /* ── build automaton ──────────────────────────────────────────────── */
  printf("[2/4] Building AC automaton...\n");
  ccac_t ac; ccac_init(&ac);

  double t0 = now_ms();
  if (!ccac_build(&ac, dict, dict_len, '\n')) {
    fprintf(stderr, "FAIL: ccac_build failed\n"); free(dict); return 1;
  }
  double t1 = now_ms();
  printf("      %zu unique words, %.0f ms (%.0f words/sec)\n",
         ac.size, t1 - t0, ac.size / ((t1 - t0) / 1000.0));
  free(dict);

  /* ── load test text ───────────────────────────────────────────────── */
  printf("[3/4] Loading test text: %s\n", text_file);
  size_t text_len;
  char *text = read_file(text_file, &text_len);
  if (!text) { fprintf(stderr, "FAIL: cannot read %s\n", text_file); ccac_destroy(&ac); return 1; }
  printf("      %.2f MB\n", text_len / (1024.0 * 1024.0));

  /* ── search ───────────────────────────────────────────────────────── */
  printf("[4/4] Searching...\n");

  /* first pass: existence check (test mode) */
  int has_match = 0;
  t0 = now_ms();
  ccac_match(&ac, text, text_len, NULL, &has_match);
  t1 = now_ms();
  printf("      test mode (any?): %s, %.0f ms\n",
         has_match ? "yes" : "no", (t1 - t0));

  if (!has_match) {
    printf("\nRESULT: 0 matches — nothing to validate\n");
    free(text); ccac_destroy(&ac); return 0;
  }

  /* second pass: collect matches (cap at 10M) */
  int nm = 10000000;
  ccac_match_t *matches = (ccac_match_t *)malloc(
      (size_t)nm * sizeof(ccac_match_t));
  if (!matches) { free(text); ccac_destroy(&ac); return 1; }

  t0 = now_ms();
  ccac_match(&ac, text, text_len, matches, &nm);
  t1 = now_ms();

  printf("      found: %d matches in %.0f ms (%.1f MB/s)\n",
         nm, t1 - t0,
         (text_len / (1024.0 * 1024.0)) / ((t1 - t0) / 1000.0));

  /* ── validate every match ─────────────────────────────────────────── */
  printf("\n── Validation ──\n");
  int ok = 0, bad = 0;
  /* sample up to 1000 matches for validation */
  int sample = nm < 1000 ? nm : 1000;
  int step = nm / sample;
  if (step < 1) step = 1;
  for (int i = 0; i < nm; i += step) {
    size_t s = matches[i].s;
    size_t e = matches[i].e;
    if (s < text_len && e <= text_len && s < e) {
      ok++;
    } else {
      bad++;
      if (bad <= 5)
        printf("  BAD match[%d]: s=%zu e=%zu (text_len=%zu)\n", i, s, e, text_len);
    }
  }

  printf("  sampled %d/%d matches: %d valid, %d invalid\n",
         sample, nm, ok, bad);

  /* ── validate known embedded words ────────────────────────────────── */
  if (match_file) {
    printf("\n── Recall check (embedded words) ──\n");
    char *pos_data = read_file(match_file, &dict_len);  /* reuse dict_len */
    if (pos_data) {
      /* format: "byte_offset\tword\n" */
      int total_emb = 0, found_emb = 0;
      char *line = strtok(pos_data, "\n");
      while (line) {
        char *tab = strchr(line, '\t');
        if (tab) {
          *tab = '\0';
          size_t expected_s = (size_t)strtoull(line, NULL, 10);
          const char *expected_w = tab + 1;
          size_t expected_len = strlen(expected_w);
          total_emb++;

          /* check if found in matches */
          for (int j = 0; j < nm; j++) {
            if (matches[j].s == expected_s &&
                matches[j].e == expected_s + expected_len) {
              found_emb++;
              break;
            }
          }
        }
        line = strtok(NULL, "\n");
      }
      printf("  embedded: %d, found in results: %d (%.1f%%)\n",
             total_emb, found_emb,
             total_emb > 0 ? 100.0 * found_emb / total_emb : 0.0);
      free(pos_data);
    }
  }

  free(matches);
  free(text);
  ccac_destroy(&ac);

  printf("\nDONE.\n");
  return bad > 0 ? 1 : 0;
}
