/*
**  LICENSE: BSD
**  Author: CandyMi[https://github.com/candymi]
**
**  Aho-Corasick multi-pattern matching automaton  (ccmap variant).
**
**  Identical public API to ccac.h, but uses ccmap (red-black tree)
**  instead of cchashmap for child-node storage.  See ccac.md 
**  ：cchashmap vs ccmap" for the trade-off analysis.
**
**  ── ABI compatibility ──
**
**    ccac_t, ccac_match_t, and all function signatures are byte-identical
**    to ccac.h.  The two headers are drop-in replacements at link time.
**
**  ── Usage ──
**
**    ccac_t ac; ccac_init(&ac);
**
**    const char *dict = "hello\nworld\n测试\n";
**    ccac_build(&ac, dict, strlen(dict), '\n');
**
**    ccac_add(&ac, "extra", 5, NULL);
**
**    ccac_match_t matches[64]; int n = 64;
**    ccac_match(&ac, text, text_len, matches, &n);
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

/* ── child-node container: ccmap (red-black tree) ────────────────────── */

#include "ccmap.h"
#include "unicode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ════════════════════════════════════════════════════════════════════════
**  Types  (ABI-identical to ccac.h)
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
  uint32_t                len; /**< 0 = internal, >0 = terminal (UTF-8 byte length) */
  uint32_t               word; /**< UCS-4 codepoint at this node */
  ccmap_t                 map; /**< children keyed by codepoint (red-black tree) */
  ccmap_node_t           node; /**< intrusive link in parent's ccmap */
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
 * Match result produced by @ref ccac_match.
 *
 * `s` and `e` are UTF-8 byte offsets — `text + s` is the first byte of
 * the matched word and `e - s` is its byte length.
 */
typedef struct ccac_match {
  size_t s; /**< start byte offset in the original UTF-8 text */
  size_t e; /**< one-past-the-end byte offset */
} ccac_match_t;

/* ════════════════════════════════════════════════════════════════════════
**  Internal: ccmap compare function (ordered by codepoint)
** ════════════════════════════════════════════════════════════════════════ */

static int64_t ccac_cmp_fn(const ccmap_node_t *a, const ccmap_node_t *b) {
  uint32_t wa = CCMAP_CONTAINER(a, ccac_word_node_t, node)->word;
  uint32_t wb = CCMAP_CONTAINER(b, ccac_word_node_t, node)->word;
  return (int64_t)wa - (int64_t)wb;
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: node allocation & helpers
** ════════════════════════════════════════════════════════════════════════ */

CCAC_INLINE ccac_word_node_t *ccac_alloc_node(ccac_t *ac, uint32_t word) {
  ccac_word_node_t *n = (ccac_word_node_t *)calloc(1, sizeof(*n));
  if (!n) return NULL;
  n->word = word;
  ccmap_init(&n->map, ccac_cmp_fn);
  n->next = ac->nlist;
  ac->nlist = n;
  return n;
}

CCAC_INLINE ccmap_node_t *ccac_map_get(ccmap_t *m, uint32_t word) {
  ccac_word_node_t probe;
  memset(&probe, 0, sizeof(probe));
  probe.word = word;
  return ccmap_find(m, &probe.node);
}

/* ════════════════════════════════════════════════════════════════════════
**  ccac_init
** ════════════════════════════════════════════════════════════════════════ */

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
  ac->root->fail = ac->root;
  ac->nlist = ac->root;
  ac->size = 0;
  return true;
}

/* ════════════════════════════════════════════════════════════════════════
**  ccac_destroy
** ════════════════════════════════════════════════════════════════════════ */

/**
 * Release all resources held by the automaton.
 *
 * ccmap is purely intrusive — no internal allocations to free.
 * Safe to call with NULL (no-op).  After return the instance is zeroed
 * and may be reused via @ref ccac_init.
 *
 * @param ac  automaton to destroy, or NULL
 */
CCAC_INLINE void ccac_destroy(ccac_t *ac) {
  if (!ac) return;
  ccac_word_node_t *n = ac->nlist;
  while (n) {
    ccac_word_node_t *nx = n->next;
    free(n);
    n = nx;
  }
  memset(ac, 0, sizeof(*ac));
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: insert one UTF-8 word into trie (no fail-link update)
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

    ccmap_node_t *found = ccac_map_get(&cur->map, (uint32_t)codepoint);
    if (found) {
      cur = CCMAP_CONTAINER(found, ccac_word_node_t, node);
    } else {
      ccac_word_node_t *child = ccac_alloc_node(ac, (uint32_t)codepoint);
      if (!child) return false;
      ccmap_insert(&cur->map, &child->node, NULL);
      cur = child;
    }
  }

  if (cur != ac->root && cur->len == 0) {
    cur->len = (uint32_t)word_len;
    ac->size++;
    return true;
  }
  if (dup && cur != ac->root && cur->len > 0) *dup = true;
  return false;
}

/* ════════════════════════════════════════════════════════════════════════
**  Internal: rebuild all failure links via BFS
** ════════════════════════════════════════════════════════════════════════ */

CCAC_INLINE bool ccac_rebuild_fail(ccac_t *ac) {
  if (ac->size == 0) return true;

  /* reset all fail links */
  for (ccac_word_node_t *n = ac->nlist; n; n = n->next)
    n->fail = NULL;
  ac->root->fail = ac->root;

  /* count nodes for exact queue allocation */
  size_t node_count = 0;
  for (ccac_word_node_t *n = ac->nlist; n; n = n->next) node_count++;

  size_t qcap = node_count < 4 ? 4 : node_count;
  ccac_word_node_t **queue = (ccac_word_node_t **)malloc(
      qcap * sizeof(ccac_word_node_t *));
  if (!queue) return false;
  size_t qhead = 0, qtail = 0;

#define QPUSH(n)  queue[qtail++] = (n)

  /* root's children: fail = root */
  for (ccmap_node_t *cn = ccmap_begin(&ac->root->map);
       cn != ccmap_end(&ac->root->map); cn = ccmap_next(cn)) {
    ccac_word_node_t *c = CCMAP_CONTAINER(cn, ccac_word_node_t, node);
    c->fail = ac->root;
    QPUSH(c);
  }

  /* BFS over remaining nodes */
  while (qhead < qtail) {
    ccac_word_node_t *v = queue[qhead++];

    for (ccmap_node_t *cn = ccmap_begin(&v->map);
         cn != ccmap_end(&v->map); cn = ccmap_next(cn)) {
      ccac_word_node_t *c = CCMAP_CONTAINER(cn, ccac_word_node_t, node);

      /* compute c->fail */
      ccac_word_node_t *f = v->fail;
      while (f != ac->root &&
             ccac_map_get(&f->map, c->word) == NULL)
        f = f->fail;

      ccmap_node_t *fn = ccac_map_get(&f->map, c->word);
      c->fail = (fn && fn != &c->node)
                  ? CCMAP_CONTAINER(fn, ccac_word_node_t, node)
                  : ac->root;

      QPUSH(c);
    }
  }

  free(queue);
  return true;
#undef QPUSH
}

/* ════════════════════════════════════════════════════════════════════════
**  ccac_build  —  parse UTF-8 words, build trie + failure links
** ════════════════════════════════════════════════════════════════════════ */

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

/* ════════════════════════════════════════════════════════════════════════
**  ccac_add  —  add a single word to a built automaton
** ════════════════════════════════════════════════════════════════════════ */

/**
 * Add a single word to an already-built automaton.
 *
 * Inserts the word into the trie and rebuilds all failure links.
 * This is safe to call between @ref ccac_match invocations.
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
  if (!ccac_insert_one(ac, word, len, dup))
    return false;
  return ccac_rebuild_fail(ac);
}

/* ════════════════════════════════════════════════════════════════════════
**  ccac_match  —  search UTF-8 text
** ════════════════════════════════════════════════════════════════════════ */

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
CCAC_INLINE bool ccac_match(ccac_t *ac, const char *text, size_t len,
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

    ccmap_node_t *fn = ccac_map_get(&cur->map, (uint32_t)codepoint);
    if (fn) {
      cur = CCMAP_CONTAINER(fn, ccac_word_node_t, node);
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
  return ccac_match(ac, s, len, matchs, &nmatchs) && nmatchs > 0;
}

#ifdef __cplusplus
}
#endif

#endif /* CCAC_H */
