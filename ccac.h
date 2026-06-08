/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Aho-Corasick multi-pattern matching automaton.
**
**  ── Features ──
**
**    - UTF-8 input with internal UCS-4 (UTF-32) encoding via unicode.h
**    - Custom word delimiter in ccac_build()
**    - O(n) search time (n = text length), independent of dictionary size
**    - Header-only, no external dependencies beyond cchashmap + unicode.h
**
**  ── Usage ──
**
**    ccac_t ac; ccac_init(&ac);
**
**    // bulk build from a newline-separated word list
**    const char *dict = "hello\nworld\n测试\n";
**    ccac_build(&ac, dict, strlen(dict), '\n');
**
**    // add a single word later (fail links are rebuilt automatically)
**    ccac_add(&ac, "extra", 5, NULL);
**
**    // search
**    ccac_match_t matches[64]; int n = 64;
**    ccac_find(&ac, text, text_len, matches, &n);
**    for (int i = 0; i < n; i++)
**      printf("match [%zu, %zu): %.*s\n", matches[i].s, matches[i].e,
**             (int)(matches[i].e - matches[i].s), text + matches[i].s);
**
**    ccac_destroy(&ac);
*/

#ifndef CCAC_H
#define CCAC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ── inline dispatch ─────────────────────────────────────────────────── */

#if defined(_MSC_VER) && !defined(__cplusplus)
  #define CCAC_INLINE static __inline
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
  #define CCAC_INLINE static inline
#else
  #define CCAC_INLINE static
#endif

/* ── configure cchashmap for this translation unit ────────────────────── */

#define CCHASHMAP_DEFAULT_SLOT 8
#include "cchashmap.h"
#include "unicode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════════════════
**  Types
** ════════════════════════════════════════════════════════════════════════ */

/**
 * Trie node in the Aho-Corasick automaton.
 *
 * Each node represents one UCS-4 codepoint.  Terminal nodes (`len > 0`)
 * mark the end of a dictionary word; `len` records the original UTF-8
 * byte length so the match start offset can be computed during search.
 *
 * @note All fields are internal — users only interact via the public API.
 */
typedef struct ccac_word_node {
  uint32_t              len;   /**< 0 = internal, >0 = terminal (UTF-8 byte length) */
  uint32_t             word;   /**< UCS-4 codepoint at this node */
  cchashmap_t           map;   /**< children keyed by codepoint */
  cchashmap_node_t     node;   /**< intrusive link in parent's cchashmap */
  struct ccac_word_node *fail; /**< failure link (reference only, not owned) */
  struct ccac_word_node *next; /**< linked-list link for cleanup tracking */
} ccac_word_node_t;

/**
 * Aho-Corasick automaton instance.
 *
 * Holds the root trie node, a cleanup list of all allocated nodes,
 * and the unique word count.  All memory is managed internally —
 * call @ref ccac_init before use and @ref ccac_destroy to release.
 */
typedef struct ccac {
  ccac_word_node_t   *root;    /**< root node (word=0, len=0, fail=root) */
  ccac_word_node_t   *nlist;   /**< head of allocated-node linked list */
  size_t              size;    /**< unique word count */
} ccac_t;

/**
 * Match result produced by @ref ccac_find.
 *
 * `s` and `e` are UTF-8 byte offsets — `text + s` is the first byte of
 * the matched word and `e - s` is its byte length.
 */
typedef struct ccac_match {
  size_t s; /**< start byte offset in the original UTF-8 text */
  size_t e; /**< one-past-the-end byte offset */
} ccac_match_t;

/* ════════════════════════════════════════════════════════════════════════
**  Internal: hash / equal for cchashmap (probe by codepoint)
** ════════════════════════════════════════════════════════════════════════ */

static uint64_t ccac_hash_fn(const cchashmap_node_t *n, size_t seed) {
  (void)seed;
  return CCHASHMAP_CONTAINER(n, ccac_word_node_t, node)->word;
}

static bool ccac_equal_fn(const cchashmap_node_t *a, const cchashmap_node_t *b) {
  return CCHASHMAP_CONTAINER(a, ccac_word_node_t, node)->word ==
         CCHASHMAP_CONTAINER(b, ccac_word_node_t, node)->word;
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: node allocation & helpers
** ════════════════════════════════════════════════════════════════════════ */

CCAC_INLINE ccac_word_node_t *ccac_alloc_node(ccac_t *ac, uint32_t word) {
  ccac_word_node_t *n = (ccac_word_node_t *)calloc(1, sizeof(*n));
  if (!n) return NULL;
  n->word = word;
  cchashmap_init(&n->map, ccac_hash_fn, ccac_equal_fn);
  /* link into ac->nlist for cleanup */
  n->next = ac->nlist;
  ac->nlist = n;
  return n;
}

CCAC_INLINE cchashmap_node_t *ccac_map_get(cchashmap_t *m, uint32_t word) {
  ccac_word_node_t probe;
  memset(&probe, 0, sizeof(probe));
  probe.word = word;
  return cchashmap_get(m, &probe.node);
}

/**
 * Initialise an empty automaton.
 *
 * Allocates the root node.  The instance must be released with
 * @ref ccac_destroy when no longer needed.
 *
 * @param ac  pointer to an uninitialised @ref ccac_t
 * @return    true on success, false if `ac` is NULL or allocation fails
 */
CCAC_INLINE bool ccac_init(ccac_t *ac) {
  if (!ac) return false;
  memset(ac, 0, sizeof(*ac));
  ac->root = ccac_alloc_node(ac, 0);
  if (!ac->root) return false;
  ac->root->fail = ac->root; /* self-loop for root */
  ac->nlist = ac->root;
  ac->size = 0;
  return true;
}

/**
 * Release all resources held by the automaton.
 *
 * Frees every trie node and its internal hash-map.  Safe to call with
 * NULL (no-op).  After return the instance is zeroed and may be reused
 * via @ref ccac_init.
 *
 * @param ac  automaton to destroy, or NULL
 */
CCAC_INLINE void ccac_destroy(ccac_t *ac) {
  if (!ac) return;
  ccac_word_node_t *n = ac->nlist;
  while (n) {
    ccac_word_node_t *nx = n->next;
    cchashmap_destroy(&n->map);
    free(n);
    n = nx;
  }
  memset(ac, 0, sizeof(*ac));
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: insert one UTF-8 word into trie (no fail-link update)
**  Returns true if a new word was added (terminal node newly set).
** ════════════════════════════════════════════════════════════════════════ */

CCAC_INLINE bool ccac_insert_one(ccac_t *ac, const char *word,
                                 size_t word_len, bool *dup) {
  if (dup) *dup = false;
  ccac_word_node_t *cur = ac->root;
  const char *cp = word;
  const char *end = word + word_len;

  while (cp < end) {
    uint32_t codepoint;
    int n = ccac_unicode_to_codepoint(cp, (int)(end - cp), &codepoint);
    if (n <= 0) return false;
    cp += n;

    cchashmap_node_t *found = ccac_map_get(&cur->map, (uint32_t)codepoint);
    if (found) {
      cur = CCHASHMAP_CONTAINER(found, ccac_word_node_t, node);
    } else {
      ccac_word_node_t *child = ccac_alloc_node(ac, (uint32_t)codepoint);
      if (!child) return false;
      cchashmap_set(&cur->map, &child->node, NULL);
      cur = child;
    }
  }

  if (cur != ac->root && cur->len == 0) {
    cur->len = (uint32_t)word_len;
    ac->size++;
    return true;
  }
  if (dup && cur != ac->root && cur->len > 0) *dup = true;
  return false;  /* already present (or empty word) */
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: rebuild all failure links via BFS
** ════════════════════════════════════════════════════════════════════════ */

CCAC_INLINE bool ccac_rebuild_fail(ccac_t *ac) {
  if (ac->size == 0) return true;

  /* reset all fail links (root is self-referencing, others → NULL) */
  for (ccac_word_node_t *n = ac->nlist; n; n = n->next)
    n->fail = NULL;
  ac->root->fail = ac->root;

  /* count nodes for exact queue allocation */
  size_t node_count = 0;
  for (ccac_word_node_t *n = ac->nlist; n; n = n->next) node_count++;

  /* BFS queue */
  size_t qcap = node_count < 4 ? 4 : node_count;
  ccac_word_node_t **queue = (ccac_word_node_t **)malloc(
      qcap * sizeof(ccac_word_node_t *));
  if (!queue) return false;
  size_t qhead = 0, qtail = 0;

#define QPUSH(n)  queue[qtail++] = (n)

  /* Enqueue root's children, fail = root */
  {
    ccvector_t *bk = &ac->root->map.buckets;
    size_t ncap = ccvector_capacity(bk);
    for (size_t i = 0; i < ncap; i++) {
      cchashmap_node_t **pp = (cchashmap_node_t **)ccvector_at(bk, i);
      if (!pp || !*pp) continue;
      cchashmap_node_t *cur = *pp;
      while (cur) {
        ccac_word_node_t *c = CCHASHMAP_CONTAINER(cur, ccac_word_node_t, node);
        c->fail = ac->root;
        QPUSH(c);
        cur = cur->next;
      }
    }
  }

  while (qhead < qtail) {
    ccac_word_node_t *v = queue[qhead++];
    ccvector_t *bk = &v->map.buckets;
    size_t ncap = ccvector_capacity(bk);
    for (size_t i = 0; i < ncap; i++) {
      cchashmap_node_t **pp = (cchashmap_node_t **)ccvector_at(bk, i);
      if (!pp || !*pp) continue;
      cchashmap_node_t *cur = *pp;
      while (cur) {
        ccac_word_node_t *c =
            CCHASHMAP_CONTAINER(cur, ccac_word_node_t, node);
        ccac_word_node_t *f = v->fail;
        while (f != ac->root &&
               ccac_map_get(&f->map, c->word) == NULL)
          f = f->fail;
        cchashmap_node_t *fn = ccac_map_get(&f->map, c->word);
        c->fail = (fn && fn != &c->node)
                    ? CCHASHMAP_CONTAINER(fn, ccac_word_node_t, node)
                    : ac->root;
        QPUSH(c);
        cur = cur->next;
      }
    }
  }

  free(queue);
  return true;
#undef QPUSH
}

/**
 * Build the automaton from a delimited UTF-8 word list.
 *
 * Parses `words` into segments separated by `delimiter`, inserts each
 * unique word into the trie, then constructs failure links via BFS.
 * Duplicate words are silently ignored.
 *
 * @param ac         initialised automaton (from @ref ccac_init)
 * @param words      UTF-8 encoded word list
 * @param len        byte length of `words`
 * @param delimiter  character that separates words (e.g. `'\\n'`, `','`, `'|'`)
 * @return           true on success, false on invalid arguments or
 *                   allocation failure
 */
CCAC_INLINE bool ccac_build(ccac_t *ac, const char *words, size_t len,
                            int delimiter) {
  if (!ac || !ac->root || !words) return false;
  if (len == 0) return true;

  const char *p   = words;
  const char *end = words + len;

  while (p < end) {
    const char *seg = p;
    while (p < end && (unsigned char)*p != (unsigned char)delimiter) p++;
    size_t seg_len = (size_t)(p - seg);
    if (p < end) p++;
    if (seg_len == 0) continue;
    ccac_insert_one(ac, seg, seg_len, NULL);
  }

  return ccac_rebuild_fail(ac);
}

/**
 * Add a single word to an already-built automaton.
 *
 * Inserts the word into the trie and rebuilds all failure links.
 * This is safe to call between @ref ccac_find invocations.
 *
 * @param ac    built automaton
 * @param word  UTF-8 encoded word
 * @param len   byte length of `word`
 * @param dup   optional out-parameter: set to `true` if the word already
 *              existed, `false` otherwise (may be NULL)
 * @return      `true` if the word was added (new), `false` on duplicate
 *              (`*dup` set to true) or error (`*dup` set to false)
 */
CCAC_INLINE bool ccac_add(ccac_t *ac, const char *word, size_t len,
                          bool *dup) {
  if (!ac || !ac->root || !word || len == 0) {
    if (dup) *dup = false;
    return false;
  }
  if (!ccac_insert_one(ac, word, len, dup)) {
    /* dup already set by insert_one if it was a duplicate */
    return false;
  }
  return ccac_rebuild_fail(ac);
}

/**
 * Search `text` for dictionary words.
 *
 * Runs the Aho-Corasick automaton over the UTF-8 input.
 *
 * Two modes:
 *
 * **Record mode** (`matches != NULL`): writes up to `*nmatch` hits into
 * `matches`, then stops.  `*nmatch` receives the actual count written
 * (may be less than input capacity if fewer matches exist).
 *
 * **Test mode** (`matches == NULL`): scans only until the first match,
 * then returns.  `*nmatch` receives 1 if any word was found, 0 if none.
 * Use this for a cheap "does the text contain any blocked word?" check.
 *
 * @param ac       built automaton
 * @param text     UTF-8 encoded text to scan
 * @param len      byte length of `text`
 * @param matches  output buffer, or NULL for existence-only check
 * @param nmatch   [in] capacity of `matches`  [out] matches written
 * @return         true on success, false on invalid arguments
 */
CCAC_INLINE bool ccac_find(ccac_t *ac, const char *text, size_t len,
                           ccac_match_t *matches, int *nmatch) {
  if (!ac || !ac->root || !text || !nmatch || *nmatch < 0) return false;
  if (len == 0) { *nmatch = 0; return true; }

  int max_match = *nmatch;
  int count = 0;

  ccac_word_node_t *cur = ac->root;
  size_t byte_pos = 0;

  while (byte_pos < len) {
    uint32_t codepoint;
    int n = ccac_unicode_to_codepoint(text + byte_pos, (int)(len - byte_pos), &codepoint);
    if (n <= 0) break;

    /* follow fail links until we find a child or reach root */
    while (cur != ac->root &&
           ccac_map_get(&cur->map, (uint32_t)codepoint) == NULL) {
      cur = cur->fail;
    }

    cchashmap_node_t *fn = ccac_map_get(&cur->map, (uint32_t)codepoint);
    if (fn) {
      cur = CCHASHMAP_CONTAINER(fn, ccac_word_node_t, node);
    } else {
      cur = ac->root;
    }

    /* collect matches ending at this position (output chain) */
    {
      ccac_word_node_t *tmp = cur;
      while (tmp != ac->root) {
        if (tmp->len > 0) {
          if (matches) {
            if (count >= max_match) goto done;
            matches[count].s = byte_pos + (size_t)n - tmp->len;
            matches[count].e = byte_pos + (size_t)n;
          } else {
            /* test mode: first match → done */
            *nmatch = 1;
            return true;
          }
          count++;
        }
        tmp = tmp->fail;
      }
    }

    byte_pos += (size_t)n;
  }

done:
  *nmatch = matches ? count : 0;
  return true;
}

CCAC_INLINE bool ccac_exists(ccac_t *ac, const char *s, size_t len) {
  int nmatchs = 2; ccac_match_t matchs[2];
  return ccac_find(ac, s, len, matchs, &nmatchs) && nmatchs > 0;
}

#ifdef __cplusplus
}
#endif

#endif /* CCAC_H */
