/*********************************************************************************
 * Performance benchmark for vibrex, comparing it to PCRE2 and the system
 * (POSIX) regex.
 *
 * This program benchmarks the compilation and matching performance of three
 * different regular expression engines on a variety of patterns and texts.
 *
 * To compile:
 * cc -O2 `pcre2-config --cflags` -o vibrex-benchmark vibrex-benchmark.c vibrex.c `pcre2-config --libs8`
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

typedef struct
{
  double compile_time;
  double match_time;
} benchmark_result;

/**
 * @brief Benchmark the vibrex engine.
 */
static bool
benchmark_vibrex (const char *name, const char *pattern, const char *text, int iterations, benchmark_result *result)
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
    result->compile_time = -1;
    result->match_time   = -1;
    return false;
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

  result->compile_time = compile_time;
  result->match_time   = match_time;

  vibrex_free (rex);
  return true;
}

/**
 * @brief Benchmark the PCRE2 engine.
 */
static bool
benchmark_pcre2 (const char *name, const char *pattern, const char *text, int iterations, benchmark_result *result)
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
    result->compile_time = -1;
    result->match_time   = -1;
    return false;
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

  result->compile_time = compile_time;
  result->match_time   = match_time;

  pcre2_match_data_free (match_data);
  pcre2_code_free (re);
  return true;
}

/**
 * @brief Benchmark the system (libc) regex engine.
 */
static bool
benchmark_system (const char *name, const char *pattern, const char *text, int iterations, benchmark_result *result)
{
  printf ("--- system (libc): %s ---\n", name);
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
    fprintf (stderr, "system regex compilation failed: %s\n", msgbuf);
    result->compile_time = -1;
    result->match_time   = -1;
    return false;
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

  result->compile_time = compile_time;
  result->match_time   = match_time;

  regfree (&regex);
  return true;
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
  bool run_system_tests = true;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp (argv[i], "--no-system") == 0)
    {
      run_system_tests = false;
    }
    else if (atoi (argv[i]) > 0)
    {
      iterations = atoi (argv[i]);
    }
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

  benchmark_result vibrex_results[num_tests];
  benchmark_result pcre2_results[num_tests];
  benchmark_result system_results[num_tests];

  printf ("Running benchmarks with %d iterations per test.\n", iterations);

  for (int i = 0; i < num_tests; i++)
  {
    printf ("\n======================================================\n");
    printf ("Benchmark: %s\n", tests[i].name);
    printf ("Pattern: '%.45s...'\n", tests[i].pattern);
    printf ("Text: '%.50s...'\n", tests[i].text);
    printf ("------------------------------------------------------\n");
    benchmark_vibrex (tests[i].name, tests[i].pattern, tests[i].text, iterations, &vibrex_results[i]);
    printf ("\n");
    benchmark_pcre2 (tests[i].name, tests[i].pattern, tests[i].text, iterations, &pcre2_results[i]);
    if (run_system_tests)
    {
      printf ("\n");
      benchmark_system (tests[i].name, tests[i].pattern, tests[i].text, iterations, &system_results[i]);
    }
  }

  printf ("\n======================================================\n");
  printf ("Benchmark finished.\n");

  // --- Summary ---
  double vibrex_total_compile = 0, vibrex_total_match = 0;
  double pcre2_total_compile = 0, pcre2_total_match = 0;
  double system_total_compile = 0, system_total_match = 0;

  for (int i = 0; i < num_tests; i++)
  {
    if (vibrex_results[i].compile_time >= 0)
      vibrex_total_compile += vibrex_results[i].compile_time;
    if (vibrex_results[i].match_time >= 0)
      vibrex_total_match += vibrex_results[i].match_time;

    if (pcre2_results[i].compile_time >= 0)
      pcre2_total_compile += pcre2_results[i].compile_time;
    if (pcre2_results[i].match_time >= 0)
      pcre2_total_match += pcre2_results[i].match_time;

    if (run_system_tests)
    {
      if (system_results[i].compile_time >= 0)
        system_total_compile += system_results[i].compile_time;
      if (system_results[i].match_time >= 0)
        system_total_match += system_results[i].match_time;
    }
  }

  double vibrex_total = vibrex_total_compile + vibrex_total_match;
  double pcre2_total  = pcre2_total_compile + pcre2_total_match;
  double system_total = system_total_compile + system_total_match;

  printf ("\n======================================================\n");
  printf ("Benchmark Summary (Total Times for %d iterations per test)\n", iterations);
  printf ("------------------------------------------------------\n");
  printf ("%-10s | %-15s | %-15s | %-15s\n", "Engine", "Compile Time (s)", "Match Time (s)", "Total Time (s)");
  printf ("-----------|-----------------|-----------------|-----------------\n");
  printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "Vibrex", vibrex_total_compile, vibrex_total_match, vibrex_total);
  printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "PCRE2", pcre2_total_compile, pcre2_total_match, pcre2_total);
  if (run_system_tests)
  {
    printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "system", system_total_compile, system_total_match, system_total);
  }

  if (run_system_tests)
  {
    printf ("\nRelative Performance (higher is better, system = 1.00x)\n");
    printf ("------------------------------------------------------\n");
    printf ("%-10s | %-15s | %-15s | %-15s\n", "Engine", "Compile Speed", "Match Speed", "Overall Speed");
    printf ("-----------|-----------------|-----------------|-----------------\n");

    if (vibrex_total_compile > 0)
      printf ("%-10s | %-15.2fx | %-15.2fx | %-15.2fx\n", "Vibrex", system_total_compile / vibrex_total_compile, system_total_match / vibrex_total_match, system_total / vibrex_total);
    else
      printf ("%-10s | %-15s | %-15.2fx | %-15.2fx\n", "Vibrex", "N/A", system_total_match / vibrex_total_match, system_total / vibrex_total);

    if (pcre2_total_compile > 0)
      printf ("%-10s | %-15.2fx | %-15.2fx | %-15.2fx\n", "PCRE2", system_total_compile / pcre2_total_compile, system_total_match / pcre2_total_match, system_total / pcre2_total);
    else
      printf ("%-10s | %-15s | %-15.2fx | %-15.2fx\n", "PCRE2", "N/A", system_total_match / pcre2_total_match, system_total / pcre2_total);

    printf ("%-10s | %-15.2fx | %-15.2fx | %-15.2fx\n", "system", 1.00, 1.00, 1.00);
  }
  printf ("======================================================\n");

  return 0;
}