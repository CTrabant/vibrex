#include <stdio.h>
#include <stdlib.h>
#include "vibrex.h"

struct demo_case
{
  const char *pattern;
  const char *texts[5]; // Up to 4 test strings + NULL
};

int
main (void)
{
  // Demonstrate various vibrex features
  struct demo_case cases[] = {
      // Dot: any character
      {"c.t", {"cat", "cot", "cut", "ct", NULL}},
      // Star: zero or more
      {"ab*c", {"ac", "abc", "abbc", "abbbc", NULL}},
      // Plus: one or more
      {"ab+c", {"ac", "abc", "abbc", "abbbc", NULL}},
      // Anchors: start and end
      {"^hello$", {"hello", "hello world", "say hello", "ahello", NULL}},
      // Character class
      {"h[ae]llo", {"hello", "hallo", "hollo", "hxllo", NULL}},
      // Range in class
      {"[0-9]+", {"123", "abc", "a1b2", "", NULL}},
      // Optional (using star for zero or more)
      {"colou?r", {"color", "colour", "colouur", "colr", NULL}},
      // Dot-star (wildcard)
      {"a.*z", {"abz", "a123z", "az", "a z", NULL}},
      // NULL pattern to end
      {NULL, {NULL}}};

  for (int i = 0; cases[i].pattern != NULL; ++i)
  {
    printf ("\nPattern: '%s'\n", cases[i].pattern);
    vibrex_t *pattern = vibrex_compile (cases[i].pattern);
    if (!pattern)
    {
      fprintf (stderr, "  Failed to compile pattern: %s\n", cases[i].pattern);
      continue;
    }
    for (int j = 0; cases[i].texts[j] != NULL; ++j)
    {
      bool matched = vibrex_match (pattern, cases[i].texts[j]);
      printf ("  Text: '%s' => %s\n", cases[i].texts[j], matched ? "MATCH" : "NO MATCH");
    }
    vibrex_free (pattern);
  }
  return 0;
}