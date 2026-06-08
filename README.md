# ccac

Aho-Corasick multi-pattern keyword matching library — header-only C implementation.

[![License](https://img.shields.io/badge/license-BSD%203--Clause-blue.svg)](LICENSE)
[![C99](https://img.shields.io/badge/C-99%2B-green.svg)](ccac.h)

**Language:** [中文](README.zh.md) | English

## Features

- **O(n) search time** — independent of dictionary size, linear in text length only
- **Native UTF-8** — built-in UCS-4 codec, correct handling of CJK, emoji, and all Unicode planes
- **Two backends** — hash-map (`ccac.h`) or red-black tree (`ccac1.h`), ABI-compatible drop-ins
- **Two search modes** — record mode (collect all matches) and test mode (existence-only check)
- **Incremental build** — batch `ccac_build` plus dynamic `ccac_add` for live updates
- **Custom delimiters** — word lists accept any delimiter (newline, comma, pipe, etc.)
- **Zero external dependencies** — only the C standard library; required containers are bundled
- **Cross-platform** — C99+ / C11 / C++ / MSVC, `-Wall -Wextra` clean

## Quick Start

```bash
git clone --recurse-submodules https://github.com/CandyMi/ccac.git
```

```c
#include "ccac.h"

int main() {
    ccac_t ac;
    ccac_init(&ac);

    // Build from a delimited word list
    const char *dict = "hello\nworld\ntest\nsensitive\n";
    ccac_build(&ac, dict, strlen(dict), '\n');

    // Search text
    const char *text = "This text contains a sensitive word.";
    ccac_match_t matches[16];
    int n = 16;
    ccac_find(&ac, text, strlen(text), matches, &n);

    for (int i = 0; i < n; i++) {
        printf("match [%zu, %zu): %.*s\n",
               matches[i].s, matches[i].e,
               (int)(matches[i].e - matches[i].s),
               text + matches[i].s);
    }

    ccac_destroy(&ac);
    return 0;
}
```

## Build & Test

### CMake (recommended)

```bash
# Configure + build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# Run all tests
ctest --test-dir build --output-on-failure

# One-shot: build + test
cmake --build build --target check -j4
ctest --test-dir build --output-on-failure

# Benchmark (needs PCRE2)
cmake --build build --target bench

# Install headers
cmake --install build --prefix /usr/local
```

All intermediate artifacts stay inside `build/`.

### Manual

```bash
INC="-I. -I3rd/ccalg/include"

# Codec tests
cc -std=c99 -Wall -Wextra -I. -o test_unicode tests/test_unicode.c && ./test_unicode

# Unit tests
cc -std=c99 -Wall -Wextra $INC -o test_ccac tests/test_ccac.c && ./test_ccac

# Robustness tests
cc -std=c99 -Wall -Wextra $INC -o test_ccac_robust tests/test_ccac_robust.c && ./test_ccac_robust

# Stress test (needs generated data)
python3 scripts/gen_words.py
python3 scripts/gen_stress_text.py words.zh.txt stress_text.txt 3
cc -std=c99 -O2 -Wall -Wextra $INC -o test_ccac_stress tests/test_ccac_stress.c
./test_ccac_stress words.zh.txt stress_text.txt stress_text.txt.pos
```

## API

| Function | Description |
|----------|-------------|
| `ccac_init(ac)` | Initialize the automaton |
| `ccac_destroy(ac)` | Release all resources |
| `ccac_build(ac, words, len, delim)` | Bulk-build from a delimited word list |
| `ccac_add(ac, word, len, &dup)` | Add a single word dynamically |
| `ccac_find(ac, text, len, matches, &nmatch)` | Search text for dictionary words |

**`ccac_find` modes:**

| `matches` | Behavior |
|-----------|----------|
| `!= NULL` | Record mode — writes up to `*nmatch` results into the buffer |
| `== NULL` | Test mode — returns on first match; `*nmatch` = 1 if found, 0 if not |

### Match result

```c
typedef struct ccac_match {
    size_t s;  // start byte offset in the original UTF-8 text
    size_t e;  // one-past-the-end byte offset (e - s = byte length)
} ccac_match_t;
```

## ccac vs ccac1

| | `ccac.h` | `ccac1.h` |
|---|---|---|
| Child container | `cchashmap` (hash map) | `ccmap` (red-black tree) |
| Lookup complexity | O(1) average | O(log n) |
| Memory | Bucket array (auto-resize) | Zero internal allocation |
| Best for | Large alphabets, raw speed | Memory-constrained, small node counts |

Both headers expose **identical** types and function signatures — swap one `#include` to switch.

## File Layout

```
ccac/
├── ccac.h           # Variant A: hash-map backend
├── ccac1.h          # Variant B: red-black tree backend (ABI-compatible)
├── unicode.h        # UTF-8 ↔ UCS-4 codec (zero deps)
├── 3rd/ccalg/       # [git submodule] container library
├── CMakeLists.txt   # Build system
├── AGENTS.md        # Canonical reference for AI coding agents
├── tests/           # Test suite
│   ├── test_ccac.c
│   ├── test_ccac_robust.c
│   ├── test_ccac_stress.c
│   └── test_unicode.c
├── bench/           # Benchmarks
│   └── bench_ccac.c
└── scripts/         # Data generation
    ├── gen_words.py
    └── gen_stress_text.py
```

## Performance

50,000 Chinese words / 3 MB text (Apple M1):

| Metric | Value |
|--------|-------|
| Build speed | 469,867 words/sec |
| Search throughput | 63.2 MB/s |
| Recall | 100% |
| vs naive (strstr) projected | **2,357×** faster |
| vs PCRE2 capped | **1.6×** faster |

## How It Works

### Architecture

```mermaid
flowchart LR
    A["UTF-8 Text"] --> B["unicode.h<br/>codec"]
    B -->|"UCS-4"| C["AC Automaton"]
    D["Dictionary"] --> E["Build Trie"]
    E --> F["Failure Links<br/>(BFS)"]
    F --> C
    C --> G["Matches<br/>(offsets)"]
```

### Trie & Failure Links

Example: dictionary `{he, she, his, hers}`

```mermaid
flowchart TD
    R(("root"))
    R -->|"h"| H(("h"))
    R -->|"s"| S(("s"))
    H -->|"e ●"| HE(("he"))
    H -->|"i"| HI(("hi"))
    HI -->|"s ●"| HIS(("his"))
    S -->|"h"| SH(("sh"))
    SH -->|"e ●"| SHE(("she"))
    HE -->|"r"| HER(("her"))
    HER -->|"s ●"| HERS(("hers"))

    H -.->|"fail"| R
    S -.->|"fail"| R
    HE -.->|"fail"| SHE
    SH -.->|"fail"| H
    SHE -.->|"fail"| HE
    HIS -.->|"fail"| SHE
    HER -.->|"fail"| S
    HERS -.->|"fail"| SHE

    style HE fill:#e44,stroke:#333,color:#fff
    style SHE fill:#e44,stroke:#333,color:#fff
    style HIS fill:#e44,stroke:#333,color:#fff
    style HERS fill:#e44,stroke:#333,color:#fff
```

**●** = terminal node (end of a dictionary word). Dashed lines = failure links.

### Search Example

Scanning `"ushers"` against the trie above:

```mermaid
sequenceDiagram
    participant T as Text
    participant AC as AC Automaton
    participant M as Matches
    
    T->>AC: pos=0 'u'
    AC-->>AC: root → no 'u' → stay at root
    
    T->>AC: pos=1 's' → root child 's'
    T->>AC: pos=2 'h' → s→h (node sh)
    
    T->>AC: pos=3 'e'
    AC-->>AC: sh→'e' ✦ she (terminal!)
    M-->>M: ✅ [1,4) = "she"
    AC-->>AC: she→fail→he (terminal!)
    M-->>M: ✅ [2,4) = "he"
    
    T->>AC: pos=4 'r' → he→r (node her)
    
    T->>AC: pos=5 's'
    AC-->>AC: her→'s' ✦ hers (terminal!)
    M-->>M: ✅ [2,6) = "hers"
```

### Trie structure

Each node stores a single UCS-4 codepoint. Terminal nodes record the original UTF-8 byte length of the dictionary word, enabling O(1) match-start computation. Child lookup uses either a chained hash map or a red-black tree.

### Failure links

Built in one BFS pass from the root: for each node `v` with child `c` on codepoint `x`, follow `v->fail` until a node with child `x` is found (or root), then set `c->fail` accordingly. Total time: O(number of nodes).

### unicode.h codec

Self-contained UTF-8 ↔ UCS-4 implementation:

| Function | Direction |
|----------|-----------|
| `ccac_unicode_to_codepoint(str, len, &val)` | UTF-8 → UCS-4 |
| `ccac_codepoint_to_utf8(val, str, &len)` | UCS-4 → UTF-8 |

Performance design: ASCII fast path, 256-byte first-byte classification table, unrolled continuation-byte processing via fall-through switch.

## License

BSD 3-Clause © CandyMi
