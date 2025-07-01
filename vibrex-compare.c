/********************************************************************************
 * compare_vibrex - Compare vibrex regex matching with system regex.
 *
 * This program takes a file containing regular expressions and a file
 * containing strings to test against. It compares the matching behavior
 * of the vibrex library against the system's standard POSIX regex library.
 *
 * Usage:
 *   compare_vibrex [-v] <regex_list_file> <test_string_file>
 *
 * Arguments:
 *   -v: Optional flag for verbose output. Reports all successful checks.
 *
 *   regex_list_file: A file where each line contains an expected match
 *     status followed by a regular expression pattern. The format is:
 *     STATUS PATTERN
 *     Where STATUS is one of MATCH_TRUE, MATCH_FALSE, or MATCH_UNSET.
 *
 *   test_string_file: A file containing input strings to be matched,
 *     one string per line.
 *
 * The program reports two types of failures:
 *
 * 1. Mismatch: If vibrex and the system regex library produce different
 *    match results for the same pattern and string.
 *
 * 2. Unexpected: If the match result differs from the expected status
 *    provided in the regex_list_file (ignored for MATCH_UNSET).
 *
 * It exits with a non-zero status if any failures occur or if there are
 * issues with file I/O or memory allocation.
 *********************************************************************************/

#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vibrex.h"

#define MAX_LINE 4096

typedef enum
{
  MATCH_UNSET = -1,
  MATCH_FALSE = 0,
  MATCH_TRUE  = 1
} match_status;

typedef struct
{
  const char *pattern;
  match_status expected_match;
} RegexTest;

int
main (int argc, char **argv)
{
  int verbose                  = 0;
  const char *test_string_file = NULL;
  const char *regex_list_file  = NULL;

  for (int argi = 1; argi < argc; ++argi)
  {
    if (strcmp (argv[argi], "-v") == 0)
    {
      verbose = 1;
    }
    else if (argv[argi][0] != '-')
    {
      if (regex_list_file == NULL)
      {
        regex_list_file = argv[argi];
      }
      else if (test_string_file == NULL)
      {
        test_string_file = argv[argi];
      }
      else
      {
        fprintf (stderr, "Too many arguments provided.\n");
        fprintf (stderr, "Usage: %s [-v] <regex_list_file> <test_string_file>\n", argv[0]);
        return 1;
      }
    }
    else
    {
      fprintf (stderr, "Unrecognized argument: %s\n", argv[argi]);
      fprintf (stderr, "Usage: %s [-v] <regex_list_file> <test_string_file>\n", argv[0]);
      return 1;
    }
  }

  if (test_string_file == NULL || regex_list_file == NULL)
  {
    fprintf (stderr, "Error: regex_list_file and test_string_file are required arguments.\n");
    fprintf (stderr, "Usage: %s [-v] <regex_list_file> <test_string_file>\n", argv[0]);
    return 1;
  }

  FILE *regex_file = fopen (regex_list_file, "r");
  if (!regex_file)
  {
    perror ("Failed to open regex list file");
    return 1;
  }

  // Count lines to allocate memory for tests
  int num_tests_alloc = 0;
  char line_buffer[MAX_LINE];
  while (fgets (line_buffer, sizeof (line_buffer), regex_file))
  {
    num_tests_alloc++;
  }
  rewind (regex_file);

  if (num_tests_alloc == 0)
  {
    fprintf (stderr, "Regex file is empty: %s\n", regex_list_file);
    fclose (regex_file);
    return 0;
  }

  RegexTest *tests = malloc (num_tests_alloc * sizeof (RegexTest));
  if (!tests)
  {
    perror ("Failed to allocate memory for tests");
    fclose (regex_file);
    return 1;
  }

  int num_tests = 0;
  while (fgets (line_buffer, sizeof (line_buffer), regex_file))
  {
    line_buffer[strcspn (line_buffer, "\r\n")] = 0; // Remove trailing newline
    if (strlen (line_buffer) > 0)
    {
      // Parse the line into an expected match status and a pattern.
      // A line is of the form:
      // STATUS PATTERN
      // Where STATUS is one of: MATCH_TRUE, MATCH_FALSE, MATCH_UNSET
      // And PATTERN is a regular expression.
      // Trim whitespace from the beginning and end of the pattern.
      char *pattern = strchr (line_buffer, ' ');
      if (pattern == NULL)
      {
        fprintf (stderr, "Invalid line format: %s\n", line_buffer);
        return 1;
      }
      *pattern         = '\0';
      char *status_str = line_buffer;
      pattern++;
      while (isspace ((unsigned char)*pattern))
      {
        pattern++;
      }

      char *end = pattern + strlen (pattern) - 1;
      while (end > pattern && isspace ((unsigned char)*end))
      {
        *end = '\0';
        end--;
      }

      if (*pattern == '\0')
      {
        fprintf (stderr, "Invalid line format (empty pattern): %s\n", line_buffer);
        return 1;
      }

      tests[num_tests].pattern = strdup (pattern);
      if (strcmp (status_str, "MATCH_TRUE") == 0)
      {
        tests[num_tests].expected_match = MATCH_TRUE;
      }
      else if (strcmp (status_str, "MATCH_FALSE") == 0)
      {
        tests[num_tests].expected_match = MATCH_FALSE;
      }
      else if (strcmp (status_str, "MATCH_UNSET") == 0)
      {
        tests[num_tests].expected_match = MATCH_UNSET;
      }
      else
      {
        fprintf (stderr, "Invalid status: %s\n", status_str);
        return 1;
      }

      num_tests++;
    }
  }
  fclose (regex_file);

  // Allocate and compile an array of vibrex_t pointers for each test
  vibrex_t **vibrexes = malloc (num_tests * sizeof (vibrex_t *));
  for (int i = 0; i < num_tests; i++)
  {
    const char *error_message = NULL;
    vibrexes[i]               = vibrex_compile (tests[i].pattern, &error_message);
    if (!vibrexes[i])
    {
      fprintf (stderr, "Could not compile regex: %s", tests[i].pattern);
      if (error_message)
      {
        fprintf (stderr, " (%s)", error_message);
      }
      fprintf (stderr, "\n");
      return 1;
    }
  }

  // Allocate and compile an array of regex_t entries for each test
  regex_t *regexes = malloc (num_tests * sizeof (regex_t));
  for (int i = 0; i < num_tests; i++)
  {
    if (regcomp (&regexes[i], tests[i].pattern, REG_EXTENDED))
    {
      fprintf (stderr, "Could not compile regex: %s\n", tests[i].pattern);
      return 1;
    }
  }

  FILE *file = fopen (test_string_file, "r");
  if (!file)
  {
    perror ("Failed to open test string file");
    return 1;
  }

  char line[MAX_LINE];
  int line_num = 0;
  while (fgets (line, sizeof (line), file))
  {
    line_num++;
    line[strcspn (line, "\r\n")] = 0; // Remove trailing newline

    for (int i = 0; i < num_tests; i++)
    {
      match_status matched_vibrex = (vibrex_match (vibrexes[i], line) ? MATCH_TRUE : MATCH_FALSE);
      match_status matched_regex  = (regexec (&regexes[i], line, 0, NULL, 0) == 0 ? MATCH_TRUE : MATCH_FALSE);

      if (matched_vibrex != matched_regex)
      {
        printf ("Line %d: \"%s\" [Pattern: \"%s\"] => FAIL (vibrex mismatch with system regexec)\n",
                line_num, line, tests[i].pattern);
      }
      else if (tests[i].expected_match != MATCH_UNSET && tests[i].expected_match != matched_vibrex)
      {
        printf ("Line %d: \"%s\" [Pattern: \"%s\"] => FAIL (expected %s, got %s)\n",
                line_num, line, tests[i].pattern,
                tests[i].expected_match == MATCH_TRUE ? "match" : "no match",
                matched_vibrex == MATCH_TRUE ? "match" : "no match");
      }
      else if (verbose)
      {
        printf ("Line %d: \"%s\" [Pattern: \"%s\"] => OK\n", line_num, line, tests[i].pattern);
      }
    }
  }

  for (int i = 0; i < num_tests; i++)
  {
    vibrex_free (vibrexes[i]);
  }
  free (vibrexes);

  for (int i = 0; i < num_tests; i++)
  {
    regfree (&regexes[i]);
  }
  free (regexes);

  for (int i = 0; i < num_tests; i++)
  {
    free ((char *)tests[i].pattern);
  }
  free (tests);

  fclose (file);
  return 0;
}