# AGENTS.md

> Canonical reference for AI coding agents working on this project.
> Auto-generated from the source headers by Reasonix Code.

---

## Project identity

**ccac** — a header-only Aho-Corasick multi-pattern matching library in C, compatible with C99 / C11 / C++ / MSVC.

- **License:** BSD 3-Clause
- **Author:** CandyMi
- **Repository:** [github.com/CandyMi/ccac](https://github.com/CandyMi/ccac)
- **Header-only.** No build system required to use — `#include` and go. CMake provided for tests + benchmarks.
- **Core traits:** O(n) search time independent of dictionary size, UTF-8 natively supported via ccalg's `ccunicode.h`, two container backends (hashmap vs red-black tree).

---

## Repository layout

```
ccac/
├── ccac.h           # Variant A: cchashmap-backed automaton
├── ccac1.h          # Variant B: ccmap-backed automaton (ABI-compatible drop-in)
├── 3rd/ccalg/       # Git submodule — containers + unicode codec
│   ├── include/
│   │   ├── cchashmap.h
│   │   ├── ccmap.h
│   │   ├── ccunicode.h
│   │   └── ccvector.h
│   └── ...
├── AGENTS.md        # This file — canonical design spec & API reference
├── README.md        # English README (GitHub default)
├── README.zh.md     # Chinese README
├── LICENSE
├── CMakeLists.txt
├── tests/           # Unit tests (C)
│   ├── test_ccac.c
│   ├── test_ccac_robust.c
│   ├── test_ccac_stress.c
│   └── test_unicode.c
├── bench/           # Benchmarks
│   └── bench_ccac.c
└── scripts/         # Data-generation helpers
    ├── gen_words.py
    └── gen_stress_text.py
```

### File map

| File | Description |
| --- | --- |
| [ccac.h](ccac.h) | Aho-Corasick automaton backed by `cchashmap` (chained hash map) |
| [ccac1.h](ccac1.h) | Aho-Corasick automaton backed by `ccmap` (red-black tree) |
| [3rd/ccalg](https://github.com/CandyMi/ccalg) | Git submodule — provides `cchashmap.h`, `ccmap.h`, `ccunicode.h`, `ccvector.h` |

---

## Architecture

### Two variants, one API

`ccac.h` and `ccac1.h` expose **byte-identical public types and function signatures**. They differ only in the child-node container:

| Variant | Container | Lookup | Memory |
| --- | --- | --- | --- |
| `ccac.h` | `cchashmap` (hash) | O(1) average | Buckets (allocated) |
| `ccac1.h` | `ccmap` (red-black tree) | O(log σ) | Intrusive (zero alloc) |

The choice is a compile-time decision — pick one header and `#include` it.

### Data flow

```
UTF-8 text ──► ccunicode.h ──► UCS-4 codepoints ──► AC automaton ──► matches
                      │                                            │
                      │  ccunicode_to_codepoint()                  │  trie node = codepoint
                      │  ccunicode_from_codepoint()                │  child lookup via cchashmap/ccmap
                      │                                            │  fail links via BFS
```

### Public types

```c
typedef struct ccac_match {
  size_t s;  // start byte offset (inclusive)
  size_t e;  // end byte offset (exclusive, e-s = word length)
} ccac_match_t;

// Half-open [s, e) — safe for printf("%.*s", e-s, text+s), no copy needed.

typedef struct ccac {
  ccac_word_node_t *root;   // root trie node (word=0, len=0)
  ccac_word_node_t *nlist;  // linked list of all allocated nodes
  size_t            size;   // unique word count
} ccac_t;
```

### Public API

```c
bool ccac_init   (ccac_t *ac);
void ccac_destroy(ccac_t *ac);

bool ccac_build  (ccac_t *ac, const char *words, size_t len, int delimiter);
bool ccac_add    (ccac_t *ac, const char *word, size_t len, bool *dup);

bool ccac_match (ccac_t *ac, const char *text, size_t len,
                  ccac_match_t *matches, int *nmatch);
```

#### ccac_match modes

| `matches` | Behavior |
| --- | --- |
| `!= NULL` | Record mode: writes up to `*nmatch` hits, stops at capacity |
| `== NULL` | Test mode: returns on first match, `*nmatch` = 1 if found, 0 if not |

### ccunicode.h codec (from ccalg)

UTF-8 ↔ UCS-4 implementation from the ccalg submodule (`3rd/ccalg/include/ccunicode.h`):

| Function | Direction | Return |
| --- | --- | --- |
| `ccunicode_to_codepoint(str, len, &val)` | UTF-8 → UCS-4 | Bytes consumed (1–4), or 0 on error |
| `ccunicode_from_codepoint(val, str, &len)` | UCS-4 → UTF-8 | `true` on success |

Design:
- ASCII fast path returns immediately without touching the lookup table
- 256-byte first-byte classification table (eliminates if-else chain)
- Unrolled switch with fall-through for continuation byte processing
- Portable fallthrough macro (`ccunicode_fallthrough`) for GCC/Clang/MSVC

### Internal: trie node

```c
typedef struct ccac_word_node {
  uint32_t len;    // 0 = internal node, >0 = terminal (UTF-8 byte length)
  uint32_t word;   // UCS-4 codepoint at this node
  <container> map; // child nodes keyed by codepoint
  <container_node_t> node; // intrusive link
  struct ccac_word_node *fail; // failure link (BFS-built)
  struct ccac_word_node *next; // cleanup linked-list link
} ccac_word_node_t;
```

### Failure link construction

BFS from root — for each node `v` with child `c` on codepoint `x`:
1. Follow `v->fail` until a node with child `x` is found (or root)
2. `c->fail` = that child, or root if none

Time: O(total nodes).

---

## Portability

### Compiler support

| Compiler | C Standard | Notes |
| --- | --- | --- |
| GCC | C99+ | `-Wall -Wextra` clean |
| Clang | C99+ | `-Wall -Wextra` clean, `-Wimplicit-fallthrough` suppressed |
| MSVC | C11+ | Fallthrough via `__fallthrough`, inline via `__inline` |

### Headers included

Each variant header includes:
- `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<stdlib.h>`, `<string.h>`
- `ccunicode.h` (via `3rd/ccalg/include/`), `<container>.h` (cchashmap or ccmap)

---

## Dependency graph

```
ccac.h ──────► 3rd/ccalg/include/ccunicode.h
  │
  └──► 3rd/ccalg/include/cchashmap.h       (standalone, bucket array inline)

ccac1.h ─────► 3rd/ccalg/include/ccunicode.h
  │
  └──► 3rd/ccalg/include/ccmap.h        (standalone, no allocation)
```

### Initial checkout

```bash
git clone --recurse-submodules https://github.com/CandyMi/ccac.git
# or, after a normal clone:
git submodule update --init --recursive
```

---

## Build, Test, and Benchmark

### CMake (recommended)

```bash
# Ensure submodule is initialised
git submodule update --init --recursive

# Configure + build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# Run all tests
ctest --test-dir build --output-on-failure

# Build + run everything (one-shot)
cmake --build build --target check -j4
ctest --test-dir build --output-on-failure

# Benchmark (needs PCRE2)
cmake --build build --target bench

# Install headers
cmake --install build --prefix /usr/local
```

All intermediate artifacts (objects, test binaries, generated data) stay inside `build/`.

### Manual compilation

```bash
INC="-I. -I3rd/ccalg/include"   # ccalg headers from submodule

# Codec tests (285 assertions)
cc -std=c99 -Wall -Wextra $INC -o test_unicode tests/test_unicode.c && ./test_unicode

# Basic (46 assertions)
cc -std=c99 -Wall -Wextra $INC -o test_ccac tests/test_ccac.c && ./test_ccac

# Robustness (~40k assertions)
cc -std=c99 -Wall -Wextra $INC -o test_ccac_robust tests/test_ccac_robust.c && ./test_ccac_robust

# Stress (50k words, 3 MB text, 100% recall validated)
python3 scripts/gen_words.py
python3 scripts/gen_stress_text.py words.zh.txt stress_text.txt 3
cc -std=c99 -O2 -Wall -Wextra $INC -o test_ccac_stress tests/test_ccac_stress.c
./test_ccac_stress words.zh.txt stress_text.txt stress_text.txt.pos

# Benchmark
cc -std=c99 -O2 $INC -o bench_ccac bench/bench_ccac.c -lpcre2-8
./bench_ccac words.zh.txt stress_text.txt
```

### Test coverage

| Test File | Assertions |
| --- | --- |
| `tests/test_unicode.c` | 285 |
| `tests/test_ccac.c` | 46 |
| `tests/test_ccac_robust.c` | ~40,482 |
| `tests/test_ccac_stress.c` | Recall validation (39,403 embedded words) |

---

## Editing conventions

### Language rules (mandatory)

- **README.md** — always English (GitHub default).
- **README.zh.md** — always Chinese (中文).
- **AGENTS.md** — always English.

### Other conventions

- **AGENTS.md** is the canonical reference — keep it in sync with header changes.
- **Macro prefixes are per-header.** `CCAC_` for ccac internals, `CCMAP_` for ccmap, etc.
- **unicode codec** is provided by ccalg's `ccunicode.h` (via `3rd/ccalg/include/`).
- **ccac.h and ccac1.h must remain ABI-compatible.** Same types, same signatures.
- **All public functions are NULL-safe.**
- **New tests** go in `tests/` with the `test_ccac_*.c` naming pattern.

---

*This document is generated by Reasonix Code from the source headers.*
