# vibrex

A high-performance regex engine by machines for machines in portable C, designed
for speed for a limited subset of regular expression features.

This engine was written by LLMs with guidance regarding features, optimizations,
and test review from a human, who didn't bother to look at the actual code.

## Features
- ASCII and extended ASCII support only
- Fast alternation and matching
- Supported regex metacharacters:
  - `.`    - Any single character
  - `*`    - Zero or more of the preceding atom
  - `+`    - One or more of the preceding atom
  - `?`    - Zero or one of the preceding atom
  - `^`    - Anchor to the start of the string
  - `$`    - Anchor to the end of the string
  - `|`    - Alternation (logical OR)
  - `\`    - Escape for regex metacharacters
  - `()`   - Group syntax (no capturing), including support for:
    - `()?`  - Optional groups
  - `[]`   - Character class, including support for:
    - `[a-z]`  - Character class range
    - `[^abc]` - Negated character class
    - `[a-z]?` - Optional character class
- No external dependencies
- Public domain (CC0 1.0 Universal)

## License
This project is released into the public domain under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

## Build
To build the library, demo, and test programs:

```
make           # builds the static library libvibrex.a and command line tool
make test      # builds and runs vibrex-test
make benchark  # Builds and runs vibrex-benchmark for testing against PCRE2, system regex
make compare   # builds vibrex-compare for bulk comparison testing
```

## Usage example
Here's a minimal example of compiling a pattern, matching a string, and freeing the pattern:

```c
#include <stdio.h>
#include "vibrex.h"

int main(void)
{
    const char *compile_error = NULL;

    vibrex_t *pattern = vibrex_compile("h.llo", &compile_error);
    if (!pattern)
    {
      fprintf(stderr, "Failed to compile pattern: %s\n", (compile_error) ? compile_error : "Unknown");
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

... <snip>

======================================================
Benchmark Summary (Total Times for 1000000 iterations per test)
------------------------------------------------------
Engine     | Compile Time (s) | Match Time (s)  | Total Time (s)
-----------|-----------------|-----------------|-----------------
Vibrex     | 0.000307        | 5.594755        | 5.595062
PCRE2      | 0.000237        | 7.540149        | 7.540386
system     | 0.000514        | 33.368911       | 33.369425

Relative Performance (higher is better, system = 1.00x)
------------------------------------------------------
Engine     | Compile Speed   | Match Speed     | Overall Speed
-----------|-----------------|-----------------|-----------------
Vibrex     | 1.67           x | 5.96           x | 5.96           x
PCRE2      | 2.17           x | 4.43           x | 4.43           x
system     | 1.00           x | 1.00           x | 1.00           x
======================================================
```
