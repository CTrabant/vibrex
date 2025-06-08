/*********************************************************************************
 * A simple command-line tool to demonstrate the usage of the vibrex
 * regular expression library.
 *
 * It takes two arguments:
 * 1. A regular expression pattern.
 * 2. A string to match against the pattern.
 *
 * The program compiles the pattern, performs the match, and reports
 * the result (Matched or Not Matched) along with the time taken for
 * the matching operation.
 *
 * Usage: ./vibrex-cli <pattern> <string>
 *********************************************************************************/

#include "vibrex.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int
main (int argc, char *argv[])
{
  if (argc < 3)
  {
    fprintf (stderr, "Usage: %s <pattern> <string>\n", argv[0]);
    return 1;
  }

  const char *pattern_str = argv[1];
  const char *text        = argv[2];

  vibrex_t *compiled_pattern = vibrex_compile (pattern_str);
  if (!compiled_pattern)
  {
    fprintf (stderr, "Error compiling pattern: %s\n", pattern_str);
    return 1;
  }

  struct timespec start, end;
  clock_gettime (CLOCK_MONOTONIC, &start);

  bool matched = vibrex_match (compiled_pattern, text);

  clock_gettime (CLOCK_MONOTONIC, &end);

  double time_taken = (end.tv_sec - start.tv_sec) * 1e9;
  time_taken        = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;

  printf ("Pattern:  \"%s\"\n", pattern_str);
  printf ("Text:     \"%s\"\n", text);
  printf ("Status:   %s\n", matched ? "Matched" : "Not Matched");
  printf ("Time:     %f seconds\n", time_taken);

  vibrex_free (compiled_pattern);

  return matched ? 0 : 1;
}
