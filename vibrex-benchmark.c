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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  int match_count;
} benchmark_result;

/**
 * @brief Benchmark the vibrex engine.
 */
static bool
benchmark_vibrex (const char *name, const char *pattern, const char *text, int iterations, benchmark_result *result)
{
  printf ("--- Vibrex: %s ---\n", name);
  double start_time, end_time, compile_time, match_time;
  const char *error_message = NULL;

  start_time    = get_time_s ();
  vibrex_t *rex = vibrex_compile (pattern, &error_message);
  end_time      = get_time_s ();
  compile_time  = end_time - start_time;

  if (!rex)
  {
    printf ("Vibrex compilation failed: %s\n", error_message ? error_message : "Unknown error");
    result->compile_time = -1;
    result->match_time   = -1;
    result->match_count  = -1;
    return false;
  }

  /* Warm-up run to load caches, etc. */
  (void)vibrex_match (rex, text);

  start_time      = get_time_s ();
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
  result->match_count  = match_count;

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

  start_time   = get_time_s ();
  re           = pcre2_compile ((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
  end_time     = get_time_s ();
  compile_time = end_time - start_time;

  if (re == NULL)
  {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message (errorcode, buffer, sizeof (buffer));
    printf ("PCRE2 compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
    result->compile_time = -1;
    result->match_time   = -1;
    result->match_count  = -1;
    return false;
  }

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern (re, NULL);

  size_t textsize = strlen (text);

  /* Warm-up run */
  (void)pcre2_match (re, (PCRE2_SPTR)text, textsize, 0, 0, match_data, NULL);

  start_time      = get_time_s ();
  int match_count = 0;
  for (int i = 0; i < iterations; i++)
  {
    int rc = pcre2_match (re, (PCRE2_SPTR)text, textsize, 0, 0, match_data, NULL);
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
  result->match_count  = match_count;

  pcre2_match_data_free (match_data);
  pcre2_code_free (re);
  return true;
}

/**
 * @brief Benchmark the PCRE2 JIT engine.
 */
static bool
benchmark_pcre2_jit (const char *name, const char *pattern, const char *text, int iterations, benchmark_result *result)
{
  printf ("--- PCRE2-JIT: %s ---\n", name);
  double start_time, end_time, compile_time, match_time;
  pcre2_code *re;
  int errorcode;
  PCRE2_SIZE erroroffset;

  start_time   = get_time_s ();
  re           = pcre2_compile ((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED, 0, &errorcode, &erroroffset, NULL);
  end_time     = get_time_s ();
  compile_time = end_time - start_time;

  if (re == NULL)
  {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message (errorcode, buffer, sizeof (buffer));
    printf ("PCRE2-JIT compilation failed at offset %d: %s\n", (int)erroroffset, buffer);
    result->compile_time = -1;
    result->match_time   = -1;
    result->match_count  = -1;
    return false;
  }

  // Enable JIT compilation
  int jit_rc = pcre2_jit_compile (re, PCRE2_JIT_COMPLETE);
  if (!(jit_rc == 0 || jit_rc == PCRE2_ERROR_JIT_UNSUPPORTED))
  {
    printf ("JIT compilation failed.\n");
    pcre2_code_free (re);
    return 1;
  }

  pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);

  size_t textsize = strlen (text);

  /* Warm-up run */
  (void)pcre2_match (re, (PCRE2_SPTR)text, textsize, 0, 0, match_data, NULL);

  start_time      = get_time_s ();
  int match_count = 0;
  for (int i = 0; i < iterations; i++)
  {
    int rc = pcre2_match (re, (PCRE2_SPTR)text, textsize, 0, 0, match_data, NULL);
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
  result->match_count  = match_count;

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

  start_time   = get_time_s ();
  reti         = regcomp (&regex, pattern, REG_EXTENDED | REG_NOSUB);
  end_time     = get_time_s ();
  compile_time = end_time - start_time;

  if (reti)
  {
    char msgbuf[100];
    regerror (reti, &regex, msgbuf, sizeof (msgbuf));
    fprintf (stderr, "system regex compilation failed: %s\n", msgbuf);
    result->compile_time = -1;
    result->match_time   = -1;
    result->match_count  = -1;
    return false;
  }

  /* Warm-up run */
  (void)regexec (&regex, text, 0, NULL, 0);

  start_time      = get_time_s ();
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
  result->match_count  = match_count;

  regfree (&regex);
  return true;
}

static void
print_usage (const char *prog_name)
{
  printf ("Usage: %s [options] [iterations]\n\n", prog_name);
  printf ("A performance benchmark for vibrex, comparing it to PCRE2 and the system regex library.\n\n");
  printf ("Options:\n");
  printf ("  --no-system    Do not run benchmarks against the system (libc) regex library.\n");
  printf ("  -h, --help     Display this help message and exit.\n\n");
  printf ("Arguments:\n");
  printf ("  iterations     Number of matching iterations for each test. Defaults to 100000.\n");
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

/* Special case: many alternations, all start anchored and some containing '.*' */
static const char *many_alts_pattern =
    "FDSN:NET_STA_LOC_L_H_N/MSEED3?$|"
    "FDSN:NET_STA_LOC_L_H_E/MSEED3?$|"
    "FDSN:NET_STA_LOC_L_H_Z/MSEED3?$|"
    "FDSN:XY_STA_10_B_H_.*/MSEED3?$|"
    "FDSN:YY_ST1_.*_.*_.*_Z/MSEED3?$|"
    "FDSN:YY_ST2_.*_.*_.*_Z/MSEED3?$|"
    "FDSN:YY_ST3_.*_.*_.*_Z/MSEED3?$|"
    "FDSN:NET_ALL_.*/MSEED3?$|"
    "FDSN:NET_CHAN_00_[HBL]_H_[ENZ]/MSEED3?$|"
    "FDSN:NET_STA1__.*_.*_Z/MSEED3?$|"
    "FDSN:NET_STA2__.*_.*_Z/MSEED3?$|"
    "FDSN:NET_STA3__.*_.*_Z/MSEED3?$";

static const char *many_alts_text_first   = "FDSN:NET_STA_LOC_L_H_N/MSEED";
static const char *many_alts_text_last    = "FDSN:NET_STA3__C_H_A/MSEED3";
static const char *many_alts_text_nomatch = "The quick brown fox jumps over the lazy cat.";

/* Additional test texts */
static const char *numeric_text          = "12345 67890 abc123def 456ghi789 000111222333444555666777888999";
static const char *mixed_case_text       = "HelloWorld FDSN:TestStation_01_BHZ ThisIsATest";
static const char *special_chars_text    = "test@example.com http://www.test.org/path?param=value 192.168.1.1";
static const char *repeated_pattern_text = "aaaaaaaaaabbbbbbbbbbccccccccccddddddddddeeeeeeeeee";
static const char *very_long_text        = "This is a very long string that contains many words and should test the performance of regex engines when dealing with longer input texts. It contains various patterns including numbers like 12345, special characters like @#$%, and repeating sections like abcdefgh abcdefgh abcdefgh. The purpose is to see how well different regex engines handle longer input when searching for patterns that may or may not exist within the text.";

int
main (int argc, char *argv[])
{
  int iterations        = 100000;
  bool run_system_tests = true;

  for (int i = 1; i < argc; i++)
  {
    if (strcmp (argv[i], "--no-system") == 0)
    {
      run_system_tests = false;
    }
    else if (strcmp (argv[i], "-h") == 0 || strcmp (argv[i], "--help") == 0)
    {
      print_usage (argv[0]);
      return 0;
    }
    else if (atoi (argv[i]) > 0)
    {
      iterations = atoi (argv[i]);
    }
  }

  benchmark_case tests[] = {
      // Basic literal matching
      {"Simple literal match", "brown", long_text},
      {"Simple literal no match", "blue", long_text},

      // Quantifiers and wildcards
      {"Dot star", "quis.*laboris", long_text},
      {"Greedy plus quantifier", "a+", repeated_pattern_text},
      {"Optional quantifier", "colou?r", "The color and colour are both valid"},

      // Character classes
      {"Character class", "[a-z]+", "abcdefghijklmnopqrstuvwxyz"},
      {"Negated character class", "[^0-9]+", numeric_text},
      {"Complex character class", "[a-zA-Z0-9_.-]+", special_chars_text},

      // Anchoring
      {"Anchored start", "^Lorem", long_text},
      {"Anchored end", "dog.$", long_text},
      {"Both anchors", "^This.*text.$", very_long_text},

      // Alternations
      {"Alternation match", "fox|dog|cat", long_text},
      {"Alternation no match", "bird|fish|cow", long_text},
      {"Nested alternation", "(cat|dog)|(bird|fish)", "I saw a cat today"},

      // Performance stress tests
      {"End of long text match", "text\\.$", very_long_text},
      {"Multiple matches in long text", "a", very_long_text},

      // Real-world patterns
      {"Email pattern", "[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]+", special_chars_text},
      {"URL pattern", "https?://[a-zA-Z0-9.-]+", special_chars_text},

      // Edge cases
      {"Multiple consecutive wildcards", "a.*b.*c", "axbxc and axxxbxxxcxxx"},
      {"Escaped special chars", "\\[\\]\\(\\)\\{\\}\\*\\+\\?", "[](){}*+?"},
      {"Long literal", "abcdefghijklmnopqrstuvwxyz", "The alphabet: abcdefghijklmnopqrstuvwxyz is here"},

      // FDSN benchmark tests
      {"FDSN station code", "FDSN:[A-Z0-9]+_[A-Z0-9]+_[A-Z0-9]*_[A-Z0-9]+_[A-Z]+_[A-Z]/MSEED3?", mixed_case_text},
      {"Many alts, first match", many_alts_pattern, many_alts_text_first},
      {"Many alts, last match", many_alts_pattern, many_alts_text_last},
      {"Many alts, no match", many_alts_pattern, many_alts_text_nomatch},
  };

  int num_tests = sizeof (tests) / sizeof (tests[0]);

  benchmark_result vibrex_results[num_tests];
  benchmark_result pcre2_results[num_tests];
  benchmark_result pcre2_jit_results[num_tests];
  benchmark_result system_results[num_tests];

  printf ("Running benchmarks with %d iterations per test.\n", iterations);

  for (int i = 0; i < num_tests; i++)
  {
    printf ("\n======================================================\n");
    printf ("Benchmark: %s\n", tests[i].name);
    if (strlen(tests[i].pattern) > 45)
      printf ("Pattern: '%.45s...'\n", tests[i].pattern);
    else
      printf ("Pattern: '%s'\n", tests[i].pattern);
    if (strlen(tests[i].text) > 70)
      printf ("Text: '%.70s...'\n", tests[i].text);
    else
      printf ("Text: '%s'\n", tests[i].text);
    printf ("------------------------------------------------------\n");
    benchmark_vibrex (tests[i].name, tests[i].pattern, tests[i].text, iterations, &vibrex_results[i]);
    printf ("\n");
    benchmark_pcre2 (tests[i].name, tests[i].pattern, tests[i].text, iterations, &pcre2_results[i]);
    printf ("\n");
    benchmark_pcre2_jit (tests[i].name, tests[i].pattern, tests[i].text, iterations, &pcre2_jit_results[i]);
    if (run_system_tests)
    {
      printf ("\n");
      benchmark_system (tests[i].name, tests[i].pattern, tests[i].text, iterations, &system_results[i]);
    }

    // Check if match counts differ between engines
    bool match_count_error = false;
    if (vibrex_results[i].match_count >= 0 && pcre2_results[i].match_count >= 0)
    {
      if (vibrex_results[i].match_count != pcre2_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between Vibrex (%d) and PCRE2 (%d)!\n",
                vibrex_results[i].match_count, pcre2_results[i].match_count);
        match_count_error = true;
      }
    }
    if (vibrex_results[i].match_count >= 0 && pcre2_jit_results[i].match_count >= 0)
    {
      if (vibrex_results[i].match_count != pcre2_jit_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between Vibrex (%d) and PCRE2-JIT (%d)!\n",
                vibrex_results[i].match_count, pcre2_jit_results[i].match_count);
        match_count_error = true;
      }
    }
    if (pcre2_results[i].match_count >= 0 && pcre2_jit_results[i].match_count >= 0)
    {
      if (pcre2_results[i].match_count != pcre2_jit_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between PCRE2 (%d) and PCRE2-JIT (%d)!\n",
                pcre2_results[i].match_count, pcre2_jit_results[i].match_count);
        match_count_error = true;
      }
    }
    if (run_system_tests && vibrex_results[i].match_count >= 0 && system_results[i].match_count >= 0)
    {
      if (vibrex_results[i].match_count != system_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between Vibrex (%d) and system (%d)!\n",
                vibrex_results[i].match_count, system_results[i].match_count);
        match_count_error = true;
      }
    }
    if (run_system_tests && pcre2_results[i].match_count >= 0 && system_results[i].match_count >= 0)
    {
      if (pcre2_results[i].match_count != system_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between PCRE2 (%d) and system (%d)!\n",
                pcre2_results[i].match_count, system_results[i].match_count);
        match_count_error = true;
      }
    }
    if (run_system_tests && pcre2_jit_results[i].match_count >= 0 && system_results[i].match_count >= 0)
    {
      if (pcre2_jit_results[i].match_count != system_results[i].match_count)
      {
        printf ("\nERROR: Match count mismatch between PCRE2-JIT (%d) and system (%d)!\n",
                pcre2_jit_results[i].match_count, system_results[i].match_count);
        match_count_error = true;
      }
    }

    if (match_count_error)
    {
      printf ("BENCHMARK FAILED: Engines produced different match counts for test '%s'\n", tests[i].name);
      printf ("This indicates a correctness issue with one or more regex engines.\n");
      return 1;
    }
  }

  printf ("\n======================================================\n");
  printf ("Benchmark finished.\n");

  // --- Summary ---
  double vibrex_total_compile = 0, vibrex_total_match = 0;
  double pcre2_total_compile = 0, pcre2_total_match = 0;
  double pcre2_jit_total_compile = 0, pcre2_jit_total_match = 0;
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

    if (pcre2_jit_results[i].compile_time >= 0)
      pcre2_jit_total_compile += pcre2_jit_results[i].compile_time;
    if (pcre2_jit_results[i].match_time >= 0)
      pcre2_jit_total_match += pcre2_jit_results[i].match_time;

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
  double pcre2_jit_total = pcre2_jit_total_compile + pcre2_jit_total_match;
  double system_total = system_total_compile + system_total_match;

  printf ("\n======================================================\n");
  printf ("Benchmark Summary (Total Times for %d iterations per test)\n", iterations);
  printf ("------------------------------------------------------\n");
  printf ("%-10s | %-15s | %-15s | %-15s\n", "Engine", "Compile Time (s)", "Match Time (s)", "Total Time (s)");
  printf ("-----------|-----------------|-----------------|-----------------\n");
  printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "Vibrex", vibrex_total_compile, vibrex_total_match, vibrex_total);
  printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "PCRE2", pcre2_total_compile, pcre2_total_match, pcre2_total);
  printf ("%-10s | %-15.6f | %-15.6f | %-15.6f\n", "PCRE2-JIT", pcre2_jit_total_compile, pcre2_jit_total_match, pcre2_jit_total);
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

    if (pcre2_jit_total_compile > 0)
      printf ("%-10s | %-15.2fx | %-15.2fx | %-15.2fx\n", "PCRE2-JIT", system_total_compile / pcre2_jit_total_compile, system_total_match / pcre2_jit_total_match, system_total / pcre2_jit_total);
    else
      printf ("%-10s | %-15s | %-15.2fx | %-15.2fx\n", "PCRE2-JIT", "N/A", system_total_match / pcre2_jit_total_match, system_total / pcre2_jit_total);

    printf ("%-10s | %-15.2fx | %-15.2fx | %-15.2fx\n", "system", 1.00, 1.00, 1.00);
  }
  printf ("======================================================\n");

  return 0;
}