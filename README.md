# vibrex

A limited regular expression engine in portable C, designed for speed and memory
efficiency. Supports a useful subset of regex features, with a simple API for
compiling, matching, and freeing patterns.

## Features
- ASCII and extended ASCII support
- Fast alternation and matching
- Supported regex features:
  - `.` (dot): any character
  - `*`, `+`, `?`: quantifiers (zero or more, one or more, zero or one)
  - `^`, `$`: anchors (start/end of string)
  - `|`: alternation (logical OR)
  - `[...]`: character classes and ranges
  - `[^...]`: negated character classes
  - `(?:...)`: non-capturing groups
- No external dependencies
- Public domain (CC0 1.0 Universal)

## License
This project is released into the public domain under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

## Build
To build the library, demo, and test programs:

```
make           # builds the static library libvibrex.a
make test      # builds and runs test_vibrex
make demo      # builds demo_vibrex
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

## Demo
Run the demo to see various regex patterns in action:

```
./demo_vibrex
```

## More
- See `demo_vibrex.c` for more usage examples.
- See `test_vibrex.c` for tests.