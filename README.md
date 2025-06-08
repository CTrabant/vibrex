# vibrex

A regular expression engine in portable C, designed for speed and memory
efficiency for a limited subset of regular expression features.

## Features
- ASCII and extended ASCII support only
- Fast alternation and matching
- Supported regex features:
  - `.` (dot): any character
  - `*`, `+`, `?`: quantifiers (zero or more, one or more, zero or one)
  - `^`, `$`: anchors (start/end of string)
  - `|`: alternation (logical OR)
  - `[...]`: character classes and ranges
  - `[^...]`: negated character classes
  - `(?:...)?`, `(...)?`: non-capturing and optional groups
- Key limits: no alterations in groups, no literal escape, no UTF.
- No external dependencies
- Public domain (CC0 1.0 Universal)

## License
This project is released into the public domain under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

## Build
To build the library, demo, and test programs:

```
make           # builds the static library libvibrex.a and command line tool
make test      # builds and runs vibrex-test
make compare   # builds vibrex-compare for bulk comparison testing
make benchark  # Builds vibrex-benchmark for testing against PCRE2, system regex
```

## Usage Example
Here's a minimal example of compiling a pattern, matching a string, and freeing the pattern:

```c
#include <stdio.h>
#include "vibrex.h"

int main(void)
{
    vibrex_t *pattern = vibrex_compile("h.llo");
    if (!pattern)
    {
      fprintf(stderr, "Failed to compile pattern\n");
      return 1;
    }

    const char *text = "hello";
    if (vibrex_match(pattern, text))
    {
      printf("'%s' matches!\n", text);
    }
    else
    {
      printf("'%s' does not match.\n", text);
    }

    vibrex_free(pattern);

    return 0;
}
```

## Command line tool
The vibrex-cli program can be used to test a pattern against a string:

```console
./vibrex-cli 'la.* dog$' 'happy but lazy dog'
Pattern:  "la.* dog$"
Text:     "happy but lazy dog"
Status:   Matched
Time:     0.000001 seconds
```

## Benchmark tool
The vibrex-benchmark program can be used to perform performance tests
of various patterns against vibrex, [PCRE2](https://www.pcre.org/), and
the system's regex (POSIX) implementation.

A benchmark run with 1000000 iterations per test:
```console
./vibrex-benchmark 1000000
Running benchmarks with 1000000 iterations per test.

======================================================
Benchmark: Simple literal match
Pattern: 'brown...'
Text: 'Lorem ipsum dolor sit amet, consectetur adipiscing...'
------------------------------------------------------
--- Vibrex: Simple literal match ---
Compilation time: 0.000001 s
Matching time (1000000 iterations): 0.051043 s
Average match time: 0.000000051 s
Matches found: 1000000/1000000

--- PCRE2: Simple literal match ---
Compilation time: 0.001170 s
Matching time (1000000 iterations): 0.101587 s
Average match time: 0.000000102 s
Matches found: 1000000/1000000

--- system (libc): Simple literal match ---
Compilation time: 0.000016 s
Matching time (1000000 iterations): 2.273629 s
Average match time: 0.000002274 s
Matches found: 1000000/1000000

... <snip>

======================================================
Benchmark finished.

======================================================
Benchmark Summary (Total Times for 1000000 iterations per test)
------------------------------------------------------
Engine     | Compile Time (s) | Match Time (s)  | Total Time (s)
-----------|-----------------|-----------------|-----------------
Vibrex     | 0.000099        | 1.749638        | 1.749737
PCRE2      | 0.001288        | 6.201734        | 6.203022
system     | 0.000287        | 24.452989       | 24.453276

Relative Performance (higher is better, system = 1.00x)
------------------------------------------------------
Engine     | Compile Speed   | Match Speed     | Overall Speed
-----------|-----------------|-----------------|-----------------
Vibrex     | 2.90           x | 13.98          x | 13.98          x
PCRE2      | 0.22           x | 3.94           x | 3.94           x
system     | 1.00           x | 1.00           x | 1.00           x
======================================================
```
