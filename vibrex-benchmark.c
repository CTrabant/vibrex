/*********************************************************************************
 * Performance benchmark for vibrex, comparing it to PCRE2 and the system
 * (POSIX) regex.
 *
 * This program benchmarks the compilation and matching performance of three
 * different regular expression engines on a variety of patterns and texts.
 *
 * To compile: cc -O2 `pcre2-config --cflags` -o vibrex-benchmark vibrex-benchmark.c vibrex.c `pcre2-config --libs8`
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "vibrex.h"
#include <regex.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

// --- Timing utility ---

/**
 * @brief Get high-resolution time.
 * @return The current time in seconds.
 */
static double
get_time_s ()
{
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

// --- Benchmark Functions ---

/**
 * @brief Benchmark the vibrex engine.
 */
static void
benchmark_vibrex (const char *name, const char *pattern, const char *text, int iterations)
{
  printf ("--- Vibrex: %s ---\n", name);
  double start_time, end_time, compile_time, match_time;

  start_time     = get_time_s ();
  vibrex_t *rex = vibrex_compile (pattern);
  end_time       = get_time_s ();
  compile_time   = end_time - start_time;

  if (!rex)
  {
    printf ("Vibrex compilation failed.\n");
    return;
  }

  /* Warm-up run to load caches, etc. */
  (void)vibrex_match (rex, text);

  start_time = get_time_s ();
  int match_count = 0;
  for (int i = 0; i < iterations; i++)
  {
    if (vibrex_match (rex, text))
    {
      match_count++;
    }
  }
  end_time   = get_time_s ();
  match_time = end_time - start_time;

  printf ("Compilation time: %.6f s\n", compile_time);
  printf ("Matching time (%d iterations): %.6f s\n", iterations, match_time);
  printf ("Average match time: %.9f s\n", match_time / iterations);
  printf ("Matches found: %d/%d\n", match_count, iterations);

  vibrex_free (rex);
}

/**
 * @brief Benchmark the PCRE2 engine.
 */
static void
benchmark_pcre2 (const char *name, const char *pattern, const char *text, int iterations)
{
  printf ("--- PCRE2: %s ---\n", name);
  double start_time, end_time, compile_time, match_time;
  pcre2_code *re;
  int errorcode;
  PCRE2_SIZE erroroffset;

  start_time = get_time_s ();
  re         = pcre2_compile ((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
  end_time   = get_time_s ();
  compile_time = end_time - start_time;

  if (re == NULL)
  {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message (errorcode, buffer, sizeof (buffer));
    printf ("PCRE2 compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
    return;
  }

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern (re, NULL);

  /* Warm-up run */
  (void)pcre2_match (re, (PCRE2_SPTR)text, strlen (text), 0, 0, match_data, NULL);

  start_time = get_time_s ();
  int match_count = 0;
  for (int i = 0; i < iterations; i++)
  {
    int rc = pcre2_match (re, (PCRE2_SPTR)text, strlen (text), 0, 0, match_data, NULL);
    if (rc >= 0)
    {
      match_count++;
    }
  }
  end_time   = get_time_s ();
  match_time = end_time - start_time;

  printf ("Compilation time: %.6f s\n", compile_time);
  printf ("Matching time (%d iterations): %.6f s\n", iterations, match_time);
  printf ("Average match time: %.9f s\n", match_time / iterations);
  printf ("Matches found: %d/%d\n", match_count, iterations);

  pcre2_match_data_free (match_data);
  pcre2_code_free (re);
}

/**
 * @brief Benchmark the POSIX (libc) regex engine.
 */
static void
benchmark_posix (const char *name, const char *pattern, const char *text, int iterations)
{
  printf ("--- POSIX (libc): %s ---\n", name);
  double start_time, end_time, compile_time, match_time;
  regex_t regex;
  int reti;

  start_time = get_time_s ();
  reti       = regcomp (&regex, pattern, REG_EXTENDED | REG_NOSUB);
  end_time   = get_time_s ();
  compile_time = end_time - start_time;

  if (reti)
  {
    char msgbuf[100];
    regerror (reti, &regex, msgbuf, sizeof (msgbuf));
    fprintf (stderr, "POSIX regex compilation failed: %s\n", msgbuf);
    return;
  }

  /* Warm-up run */
  (void)regexec (&regex, text, 0, NULL, 0);

  start_time = get_time_s ();
  int match_count = 0;
  for (int i = 0; i < iterations; i++)
  {
    if (regexec (&regex, text, 0, NULL, 0) == 0)
    {
      match_count++;
    }
  }
  end_time   = get_time_s ();
  match_time = end_time - start_time;

  printf ("Compilation time: %.6f s\n", compile_time);
  printf ("Matching time (%d iterations): %.6f s\n", iterations, match_time);
  printf ("Average match time: %.9f s\n", match_time / iterations);
  printf ("Matches found: %d/%d\n", match_count, iterations);

  regfree (&regex);
}

// --- Test Cases ---
typedef struct
{
  const char *name;
  const char *pattern;
  const char *text;
} benchmark_case;

/* A long string of text for searching */
static const char *long_text = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. The quick brown fox jumps over the lazy dog.";

/* Special case: many alternations, all start/end anchored and containing '.*' */
static const char *many_alts_pattern =
    "^The.*jumps over the lazy dog.$|"
    "^Lorem.*laborum.$|"
    "^Duis.*pariatur.$|"
    "^acme.*corp.$|"
    "^foo.*bar$|"
    "^widgets.*inc$|"
    "^alpha.*beta$|"
    "^gamma.*delta$|"
    "^epsilon.*zeta$|"
    "^eta.*theta$|"
    "^iota.*kappa$|"
    "^lambda.*mu$|"
    "^nu.*xi$|"
    "^omicron.*pi$|"
    "^rho.*sigma$|"
    "^tau.*upsilon$|"
    "^phi.*chi$|"
    "^psi.*omega$";

static const char *many_alts_text_first   = "The quick brown fox jumps over the lazy dog.";
static const char *many_alts_text_last    = "psi and some other text omega";
static const char *many_alts_text_nomatch = "The quick brown fox jumps over the lazy cat.";

int
main (int argc, char *argv[])
{
  int iterations = 10000;
  if (argc > 1)
  {
    iterations = atoi (argv[1]);
  }

  benchmark_case tests[] = {
      {"Simple literal match", "brown", long_text},
      {"Simple literal no match", "blue", long_text},
      {"Dot star", "quis.*laboris", long_text},
      {"Alternation match", "fox|dog|cat", long_text},
      {"Alternation no match", "bird|fish|cow", long_text},
      {"Character class", "[a-z]+", "abcdefghijklmnopqrstuvwxyz"},
      {"Anchored start", "^Lorem", long_text},
      {"Anchored end", "dog.$", long_text},
      {"Many alts, first match", many_alts_pattern, many_alts_text_first},
      {"Many alts, last match", many_alts_pattern, many_alts_text_last},
      {"Many alts, no match", many_alts_pattern, many_alts_text_nomatch},
  };

  int num_tests = sizeof (tests) / sizeof (tests[0]);

  printf ("Running benchmarks with %d iterations per test.\n", iterations);

  for (int i = 0; i < num_tests; i++)
  {
    printf ("\n======================================================\n");
    printf ("Benchmark: %s\n", tests[i].name);
    printf ("Pattern: '%.45s...'\n", tests[i].pattern);
    printf ("Text: '%.50s...'\n", tests[i].text);
    printf ("------------------------------------------------------\n");
    benchmark_vibrex (tests[i].name, tests[i].pattern, tests[i].text, iterations);
    printf ("\n");
    benchmark_pcre2 (tests[i].name, tests[i].pattern, tests[i].text, iterations);
    printf ("\n");
    benchmark_posix (tests[i].name, tests[i].pattern, tests[i].text, iterations);
  }

  printf ("\n======================================================\n");
  printf ("Benchmark finished.\n");

  return 0;
}