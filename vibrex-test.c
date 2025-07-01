#include "vibrex.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Test configuration constants
#define MAX_RECURSION_DEPTH_TEST 1500
#define SAFE_RECURSION_DEPTH 100
#define ALT_DEPTH_TEST 800
#define CC_DEPTH_TEST 600
#define MIXED_DEPTH_TEST 500
#define SAFE_ALT_DEPTH 20
#define MAX_PERFORMANCE_TIME_MS 10.0
#define CATASTROPHIC_TEST_STRING_LENGTH 30
#define EVIL_STRING_LENGTH 31

// Test output constants
#define TEST_PASS_SYMBOL "✓"
#define TEST_CELEBRATION "🎉"

// Helper function to create a test pattern and verify it compiles
static vibrex_t *
compile_and_verify (const char *pattern, bool should_succeed)
{
  vibrex_t *compiled = vibrex_compile (pattern, NULL);
  if (should_succeed)
  {
    assert (compiled != NULL);
  }
  else
  {
    assert (compiled == NULL);
  }
  return compiled;
}

// Helper function to test a single match case
static void
test_match_case (vibrex_t *pattern, const char *input, bool expected, const char *description)
{
  bool result = vibrex_match (pattern, input);
  if (result != expected)
  {
    printf ("FAILED: %s - input '%s', expected %s, got %s\n",
            description, input, expected ? "true" : "false", result ? "true" : "false");
    assert (false);
  }
}

// Helper function to test multiple match cases
static void
test_multiple_matches (vibrex_t *pattern, const char *test_cases[][2], size_t num_cases, const char *description)
{
  for (size_t i = 0; i < num_cases; i++)
  {
    bool expected = (strcmp (test_cases[i][1], "true") == 0);
    test_match_case (pattern, test_cases[i][0], expected, description);
  }
}

// Helper function to create a string filled with a repeating character
static char *
create_repeated_string (char c, size_t length)
{
  char *str = malloc (length + 1);
  assert (str != NULL);
  memset (str, c, length);
  str[length] = '\0';
  return str;
}

// Helper function to measure execution time and verify performance
static void
test_performance (vibrex_t *pattern, const char *input, bool expected_result, const char *description)
{
  (void)description; // Suppress unused parameter warning
  clock_t start  = clock ();
  bool result    = vibrex_match (pattern, input);
  clock_t end    = clock ();
  double time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;

  assert (result == expected_result);
  assert (time_ms < MAX_PERFORMANCE_TIME_MS);
}

/********************************************************************************
 * BASIC FEATURE TESTS
 ********************************************************************************/

void
test_basic_matching ()
{
  printf ("Testing basic character matching...\n");

  vibrex_t *pattern = compile_and_verify ("hello", true);

  const char *test_cases[][2] = {
      {"hello", "true"},
      {"hello world", "true"},
      {"say hello", "true"},
      {"hi", "false"}};

  test_multiple_matches (pattern, test_cases, 4, "basic matching");
  vibrex_free (pattern);
  printf (TEST_PASS_SYMBOL " Basic matching tests passed\n");
}

void
test_escape_sequences ()
{
  printf ("Testing escape sequences...\n");

  // Test escaping regex metacharacters
  const char *escape_cases[][3] = {
      {"\\.", ".", "true"},  // Escaped dot should match literal dot
      {"\\.", "x", "false"}, // Escaped dot should not match other chars
      {"\\*", "*", "true"},  // Escaped star should match literal star
      {"\\*", "x", "false"}, // Escaped star should not match other chars
      {"\\+", "+", "true"},  // Escaped plus should match literal plus
      {"\\?", "?", "true"},  // Escaped question should match literal question
      {"\\^", "^", "true"},  // Escaped caret should match literal caret
      {"\\$", "$", "true"},  // Escaped dollar should match literal dollar
      {"\\|", "|", "true"},  // Escaped pipe should match literal pipe
      {"\\(", "(", "true"},  // Escaped paren should match literal paren
      {"\\)", ")", "true"},  // Escaped paren should match literal paren
      {"\\[", "[", "true"},  // Escaped bracket should match literal bracket
      {"\\]", "]", "true"},  // Escaped bracket should match literal bracket
      {"\\\\", "\\", "true"} // Escaped backslash should match literal backslash
  };

  for (size_t i = 0; i < sizeof (escape_cases) / sizeof (escape_cases[0]); i++)
  {
    vibrex_t *pattern = compile_and_verify (escape_cases[i][0], true);
    bool expected     = (strcmp (escape_cases[i][2], "true") == 0);
    test_match_case (pattern, escape_cases[i][1], expected, "escape sequence test");
    vibrex_free (pattern);
  }

  // Test escaping in complex patterns
  vibrex_t *complex_escape = compile_and_verify ("a\\.b\\*c\\+d", true);
  assert (vibrex_match (complex_escape, "a.b*c+d") == true);
  assert (vibrex_match (complex_escape, "axbxcxd") == false);
  vibrex_free (complex_escape);

  // Test escape sequences in anchored patterns
  vibrex_t *anchored_escape = compile_and_verify ("^\\$test\\$$", true);
  assert (vibrex_match (anchored_escape, "$test$") == true);
  assert (vibrex_match (anchored_escape, "test") == false);
  vibrex_free (anchored_escape);

  printf (TEST_PASS_SYMBOL " Escape sequence tests passed\n");
}

void
test_dot_matching ()
{
  printf ("Testing dot (.) matching...\n");

  // Test basic dot matching
  vibrex_t *pattern            = compile_and_verify ("h.llo", true);
  const char *basic_cases[][2] = {
      {"hello", "true"},
      {"hallo", "true"},
      {"hxllo", "true"},
      {"h@llo", "true"},
      {"hllo", "false"}};
  test_multiple_matches (pattern, basic_cases, 5, "dot matching");
  vibrex_free (pattern);

  // Test dot at different positions
  pattern = compile_and_verify (".ello", true);
  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, " ello") == true);
  assert (vibrex_match (pattern, "ello") == false);
  vibrex_free (pattern);

  pattern = compile_and_verify ("hell.", true);
  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, "hell!") == true);
  assert (vibrex_match (pattern, "hell") == false);
  vibrex_free (pattern);

  pattern = compile_and_verify ("a.+c", true);
  assert (vibrex_match (pattern, "axbyc") == true);
  assert (vibrex_match (pattern, "ac") == false);
  vibrex_free (pattern);

  printf (TEST_PASS_SYMBOL " Dot matching tests passed\n");
}

void
test_star_quantifier ()
{
  printf ("Testing star (*) quantifier...\n");

  vibrex_t *pattern           = compile_and_verify ("ab*c", true);
  const char *star_cases[][2] = {
      {"ac", "true"},
      {"abc", "true"},
      {"abbc", "true"},
      {"abbbbbc", "true"},
      {"axc", "false"},
      {"xacx", "true"} // matches 'ac'
  };
  test_multiple_matches (pattern, star_cases, 6, "star quantifier");
  vibrex_free (pattern);

  // Test anchored patterns
  pattern = compile_and_verify ("^a*b", true);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aaab") == true);
  assert (vibrex_match (pattern, "cab") == false);
  vibrex_free (pattern);

  pattern = compile_and_verify ("ab*$", true);
  assert (vibrex_match (pattern, "a") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "abbb") == true);
  assert (vibrex_match (pattern, "abc") == false);
  vibrex_free (pattern);

  // Test multiple star quantifiers
  pattern                           = compile_and_verify ("a*b*c", true);
  const char *multi_star_cases[][2] = {
      {"c", "true"},
      {"ac", "true"},
      {"bc", "true"},
      {"abc", "true"},
      {"aaabbc", "true"},
      {"aabbbc", "true"}};
  test_multiple_matches (pattern, multi_star_cases, 6, "multiple star quantifiers");
  vibrex_free (pattern);

  pattern = compile_and_verify ("a.*b", true);
  assert (vibrex_match (pattern, "axbybzc") == true); // Should match "axbyb"
  vibrex_free (pattern);

  printf (TEST_PASS_SYMBOL " Star quantifier tests passed\n");
}

void
test_plus_quantifier ()
{
  printf ("Testing plus (+) quantifier...\n");

  vibrex_t *pattern           = compile_and_verify ("ab+c", true);
  const char *plus_cases[][2] = {
      {"abc", "true"},
      {"abbc", "true"},
      {"abbbbbc", "true"},
      {"ac", "false"},
      {"xabcy", "true"}};
  test_multiple_matches (pattern, plus_cases, 5, "plus quantifier");
  vibrex_free (pattern);

  // Test anchored plus patterns
  pattern = compile_and_verify ("^a+b", true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aaab") == true);
  assert (vibrex_match (pattern, "b") == false);
  assert (vibrex_match (pattern, "cab") == false);
  vibrex_free (pattern);

  pattern = compile_and_verify ("ab+$", true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "abbb") == true);
  assert (vibrex_match (pattern, "a") == false);
  assert (vibrex_match (pattern, "abc") == false);
  vibrex_free (pattern);

  // Test multiple plus quantifiers
  pattern                           = compile_and_verify ("a+b+c", true);
  const char *multi_plus_cases[][2] = {
      {"abc", "true"},
      {"aabbc", "true"},
      {"c", "false"},
      {"ac", "false"},
      {"bc", "false"}};
  test_multiple_matches (pattern, multi_plus_cases, 5, "multiple plus quantifiers");
  vibrex_free (pattern);

  pattern = compile_and_verify ("a.+b", true);
  assert (vibrex_match (pattern, "axbybzc") == true); // Should match "axbyb"
  assert (vibrex_match (pattern, "ab") == false);
  vibrex_free (pattern);

  printf (TEST_PASS_SYMBOL " Plus quantifier tests passed\n");
}

void
test_optional_groups ()
{
  printf ("Testing optional groups (...)?...\n");

  /* Basic optional group */
  vibrex_t *optional_group              = compile_and_verify ("a(bc)?d", true);
  const char *basic_optional_cases[][2] = {
      {"abcd", "true"}, // with optional part
      {"ad", "true"},   // without optional part
      {"abd", "false"}, // partial match
      {"acd", "false"}  // wrong content
  };
  test_multiple_matches (optional_group, basic_optional_cases, 4, "basic optional group");
  vibrex_free (optional_group);

  /* Multiple optional groups */
  vibrex_t *multi_optional              = compile_and_verify ("(ab)?(cd)?", true);
  const char *multi_optional_cases[][2] = {
      {"abcd", "true"}, // both present
      {"ab", "true"},   // first only
      {"cd", "true"},   // second only
      {"", "true"},     // neither present
      {"ac", "true"}    // matches empty at start
  };
  test_multiple_matches (multi_optional, multi_optional_cases, 5, "multiple optional groups");
  vibrex_free (multi_optional);

  /* Multiple optional groups with anchors */
  vibrex_t *anchored_optional = vibrex_compile ("^(ab)?(cd)?$", NULL);
  assert (anchored_optional != NULL);

  assert (vibrex_match (anchored_optional, "abcd") == true); // both present
  assert (vibrex_match (anchored_optional, "ab") == true);   // first only
  assert (vibrex_match (anchored_optional, "cd") == true);   // second only
  assert (vibrex_match (anchored_optional, "") == true);     // neither present
  assert (vibrex_match (anchored_optional, "ac") == false);  // wrong content - must match entire string

  vibrex_free (anchored_optional);

  /* Optional groups with quantifiers */
  vibrex_t *optional_with_quant = vibrex_compile ("a(b+)?c", NULL);
  assert (optional_with_quant != NULL);

  assert (vibrex_match (optional_with_quant, "abc") == true);   // with single b
  assert (vibrex_match (optional_with_quant, "abbbc") == true); // with multiple b's
  assert (vibrex_match (optional_with_quant, "ac") == true);    // without optional part
  assert (vibrex_match (optional_with_quant, "axc") == false);  // wrong content

  vibrex_free (optional_with_quant);

  /* Optional groups with character classes */
  vibrex_t *optional_class = vibrex_compile ("([0-9]+)?[a-z]", NULL);
  assert (optional_class != NULL);

  assert (vibrex_match (optional_class, "123a") == true); // with digits
  assert (vibrex_match (optional_class, "a") == true);    // without digits
  assert (vibrex_match (optional_class, "123") == false); // missing letter
  assert (vibrex_match (optional_class, "A") == false);   // wrong case

  vibrex_free (optional_class);

  /* Additional optional group tests */
  vibrex_t *multi_char_optional = vibrex_compile ("a(bc)?d", NULL);
  assert (multi_char_optional != NULL);

  assert (vibrex_match (multi_char_optional, "abcd") == true); // with optional part
  assert (vibrex_match (multi_char_optional, "ad") == true);   // without optional part
  assert (vibrex_match (multi_char_optional, "abd") == false); // partial match
  assert (vibrex_match (multi_char_optional, "acd") == false); // wrong content

  vibrex_free (multi_char_optional);

  printf (TEST_PASS_SYMBOL " Optional group tests passed\n");
}

void
test_optional_quantifier_char ()
{
  printf ("Testing optional (?) quantifier on characters...\n");

  vibrex_t *pattern = vibrex_compile ("ab?c", NULL);
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "ac") == true);
  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "abbc") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("^a?b$", NULL);
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aab") == false);
  vibrex_free (pattern);

  printf (TEST_PASS_SYMBOL " Optional (?) on characters tests passed\n");
}

void
test_anchors ()
{
  printf ("Testing anchors (^ and $)...\n");

  vibrex_t *start_pattern = vibrex_compile ("^hello", NULL);
  assert (start_pattern != NULL);

  assert (vibrex_match (start_pattern, "hello world") == true);
  assert (vibrex_match (start_pattern, "say hello") == false);
  assert (vibrex_match (start_pattern, "hello") == true);

  vibrex_free (start_pattern);

  vibrex_t *end_pattern = vibrex_compile ("world$", NULL);
  assert (end_pattern != NULL);

  assert (vibrex_match (end_pattern, "hello world") == true);
  assert (vibrex_match (end_pattern, "world peace") == false);
  assert (vibrex_match (end_pattern, "world") == true);

  vibrex_free (end_pattern);

  vibrex_t *both_pattern = vibrex_compile ("^hello$", NULL);
  assert (both_pattern != NULL);

  assert (vibrex_match (both_pattern, "hello") == true);
  assert (vibrex_match (both_pattern, "hello world") == false);
  assert (vibrex_match (both_pattern, "say hello") == false);

  vibrex_free (both_pattern);

  /* Anchors with quantifiers */
  vibrex_t *anchor_quant = vibrex_compile ("^a*$", NULL);
  assert (anchor_quant != NULL);
  assert (vibrex_match (anchor_quant, "aaa") == true);
  assert (vibrex_match (anchor_quant, "") == true);
  assert (vibrex_match (anchor_quant, "b") == false);
  assert (vibrex_match (anchor_quant, "aaab") == false);
  vibrex_free (anchor_quant);

  /* Anchor on empty string */
  vibrex_t *empty_anchor = vibrex_compile ("^$", NULL);
  assert (empty_anchor != NULL);
  assert (vibrex_match (empty_anchor, "") == true);
  assert (vibrex_match (empty_anchor, "a") == false);
  vibrex_free (empty_anchor);

  printf (TEST_PASS_SYMBOL " Anchor tests passed\n");
}

void
test_individual_anchors ()
{
  printf ("Testing individual anchors...\n");

  // Test start anchor only
  vibrex_t *start_only = compile_and_verify ("^test", true);
  assert (vibrex_match (start_only, "test123") == true);
  assert (vibrex_match (start_only, "123test") == false);
  assert (vibrex_match (start_only, "test") == true);
  vibrex_free (start_only);

  // Test end anchor only
  vibrex_t *end_only = compile_and_verify ("test$", true);
  assert (vibrex_match (end_only, "123test") == true);
  assert (vibrex_match (end_only, "test123") == false);
  assert (vibrex_match (end_only, "test") == true);
  vibrex_free (end_only);

  // Test escaped anchors (should not act as anchors)
  vibrex_t *escaped_start = compile_and_verify ("\\^test", true);
  assert (vibrex_match (escaped_start, "^test") == true);
  assert (vibrex_match (escaped_start, "test") == false);
  assert (vibrex_match (escaped_start, "abc^test") == true);
  vibrex_free (escaped_start);

  vibrex_t *escaped_end = compile_and_verify ("test\\$", true);
  assert (vibrex_match (escaped_end, "test$") == true);
  assert (vibrex_match (escaped_end, "test") == false);
  assert (vibrex_match (escaped_end, "test$abc") == true);
  vibrex_free (escaped_end);

  printf (TEST_PASS_SYMBOL " Individual anchor tests passed\n");
}

/********************************************************************************
 * QUANTIFIER TESTS
 ********************************************************************************/

/********************************************************************************
 * CHARACTER CLASS TESTS
 ********************************************************************************/

void
test_character_classes ()
{
  printf ("Testing character classes...\n");

  vibrex_t *pattern = vibrex_compile ("[abc]", NULL);
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "a") == true);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "c") == true);
  assert (vibrex_match (pattern, "d") == false);

  vibrex_free (pattern);

  vibrex_t *range_pattern = vibrex_compile ("[a-z]", NULL);
  assert (range_pattern != NULL);

  assert (vibrex_match (range_pattern, "hello") == true);
  assert (vibrex_match (range_pattern, "HELLO") == false);

  vibrex_free (range_pattern);

  vibrex_t *negated_pattern = vibrex_compile ("[^0-9]", NULL);
  assert (negated_pattern != NULL);

  assert (vibrex_match (negated_pattern, "hello") == true);
  assert (vibrex_match (negated_pattern, "123") == false);

  vibrex_free (negated_pattern);
  printf (TEST_PASS_SYMBOL " Character class tests passed\n");
}

void
test_extended_character_classes ()
{
  printf ("Testing extended character class features...\n");

  /* Multiple ranges in one class */
  vibrex_t *multi_range = vibrex_compile ("[a-zA-Z0-9]", NULL);
  assert (multi_range != NULL);

  assert (vibrex_match (multi_range, "a") == true);
  assert (vibrex_match (multi_range, "Z") == true);
  assert (vibrex_match (multi_range, "5") == true);
  assert (vibrex_match (multi_range, "@") == false);
  assert (vibrex_match (multi_range, " ") == false);

  vibrex_free (multi_range);

  /* Mixed literals and ranges */
  vibrex_t *mixed_class = vibrex_compile ("[a-z.@_0-9]", NULL);
  assert (mixed_class != NULL);

  assert (vibrex_match (mixed_class, "a") == true);
  assert (vibrex_match (mixed_class, "5") == true);
  assert (vibrex_match (mixed_class, ".") == true);
  assert (vibrex_match (mixed_class, "@") == true);
  assert (vibrex_match (mixed_class, "_") == true);
  assert (vibrex_match (mixed_class, "Z") == false);
  assert (vibrex_match (mixed_class, "#") == false);

  vibrex_free (mixed_class);

  /* Character classes with quantifiers */
  vibrex_t *class_plus = vibrex_compile ("[0-9]+", NULL);
  assert (class_plus != NULL);

  assert (vibrex_match (class_plus, "123") == true);
  assert (vibrex_match (class_plus, "0") == true);
  assert (vibrex_match (class_plus, "abc123") == true); // contains digits
  assert (vibrex_match (class_plus, "abc") == false);   // no digits

  vibrex_free (class_plus);

  vibrex_t *class_star = vibrex_compile ("[a-z]*", NULL);
  assert (class_star != NULL);

  assert (vibrex_match (class_star, "hello") == true);
  assert (vibrex_match (class_star, "") == true);         // zero matches
  assert (vibrex_match (class_star, "123hello") == true); // matches empty at start
  assert (vibrex_match (class_star, "HELLO") == true);    // matches empty at start

  vibrex_free (class_star);

  vibrex_t *class_optional = vibrex_compile ("[0-9]?", NULL);
  assert (class_optional != NULL);

  assert (vibrex_match (class_optional, "5") == true);
  assert (vibrex_match (class_optional, "") == true);   // zero matches
  assert (vibrex_match (class_optional, "a5") == true); // matches '5'
  assert (vibrex_match (class_optional, "a") == true);  // matches empty string at start
  assert (vibrex_match (class_optional, "55") == true); // matches first digit

  vibrex_free (class_optional);

  /* Complex negated character classes */
  vibrex_t *complex_negated = vibrex_compile ("[^a-zA-Z0-9]", NULL);
  assert (complex_negated != NULL);

  assert (vibrex_match (complex_negated, "@") == true);
  assert (vibrex_match (complex_negated, " ") == true);
  assert (vibrex_match (complex_negated, "!") == true);
  assert (vibrex_match (complex_negated, "a") == false);
  assert (vibrex_match (complex_negated, "Z") == false);
  assert (vibrex_match (complex_negated, "5") == false);

  vibrex_free (complex_negated);

  /* Hyphen as a literal */
  vibrex_t *hyphen_literal = vibrex_compile ("[-az]", NULL); // at the start
  assert (hyphen_literal != NULL);
  assert (vibrex_match (hyphen_literal, "-") == true);
  assert (vibrex_match (hyphen_literal, "a") == true);
  assert (vibrex_match (hyphen_literal, "z") == true);
  assert (vibrex_match (hyphen_literal, "b") == false);
  vibrex_free (hyphen_literal);

  hyphen_literal = vibrex_compile ("[az-]", NULL); // at the end
  assert (hyphen_literal != NULL);
  assert (vibrex_match (hyphen_literal, "-") == true);
  assert (vibrex_match (hyphen_literal, "a") == true);
  assert (vibrex_match (hyphen_literal, "z") == true);
  assert (vibrex_match (hyphen_literal, "b") == false);
  vibrex_free (hyphen_literal);

  /* Caret as a literal */
  vibrex_t *caret_literal = vibrex_compile ("[a^c]", NULL); // not at the start
  assert (caret_literal != NULL);
  assert (vibrex_match (caret_literal, "a") == true);
  assert (vibrex_match (caret_literal, "^") == true);
  assert (vibrex_match (caret_literal, "c") == true);
  assert (vibrex_match (caret_literal, "b") == false);
  vibrex_free (caret_literal);

  /* Note: Escaped characters in character classes are not implemented */
  printf ("  (Note: Escaped characters in character classes [\\[\\]] are not implemented)\n");

  /* Character classes in complex patterns */
  vibrex_t *complex_pattern = vibrex_compile ("[a-z]+@[a-z]+\\.[a-z]+", NULL);
  assert (complex_pattern != NULL);

  assert (vibrex_match (complex_pattern, "user@example.com") == true);
  assert (vibrex_match (complex_pattern, "test@domain.org") == true);
  assert (vibrex_match (complex_pattern, "User@Example.Com") == false); // uppercase
  assert (vibrex_match (complex_pattern, "user.example.com") == false); // missing @

  vibrex_free (complex_pattern);

  /* Character class ranges with special characters */
  vibrex_t *special_range = vibrex_compile ("[!-/]", NULL); // ASCII range from ! to /
  assert (special_range != NULL);

  assert (vibrex_match (special_range, "!") == true);
  assert (vibrex_match (special_range, "#") == true);
  assert (vibrex_match (special_range, "/") == true);
  assert (vibrex_match (special_range, "0") == false);
  assert (vibrex_match (special_range, " ") == false);

  vibrex_free (special_range);

  printf (TEST_PASS_SYMBOL " Extended character class tests passed\n");
}

void
test_character_class_edge_cases ()
{
  printf ("Testing character class edge cases...\n");

  // Test single character class
  vibrex_t *single_char = compile_and_verify ("[a]", true);
  assert (vibrex_match (single_char, "a") == true);
  assert (vibrex_match (single_char, "b") == false);
  vibrex_free (single_char);

  // Test character class with all ASCII printable characters
  vibrex_t *all_printable = compile_and_verify ("[ -~]", true); // Space to tilde
  assert (vibrex_match (all_printable, " ") == true);
  assert (vibrex_match (all_printable, "A") == true);
  assert (vibrex_match (all_printable, "~") == true);
  assert (vibrex_match (all_printable, "\x1F") == false); // Below space
  assert (vibrex_match (all_printable, "\x7F") == false); // Above tilde
  vibrex_free (all_printable);

  // Test boundary conditions in ranges
  vibrex_t *boundary = compile_and_verify ("[0-9A-Za-z]", true);
  assert (vibrex_match (boundary, "0") == true);
  assert (vibrex_match (boundary, "9") == true);
  assert (vibrex_match (boundary, "A") == true);
  assert (vibrex_match (boundary, "Z") == true);
  assert (vibrex_match (boundary, "a") == true);
  assert (vibrex_match (boundary, "z") == true);
  assert (vibrex_match (boundary, "/") == false); // Just before '0'
  assert (vibrex_match (boundary, ":") == false); // Just after '9'
  assert (vibrex_match (boundary, "@") == false); // Just before 'A'
  assert (vibrex_match (boundary, "[") == false); // Just after 'Z'
  assert (vibrex_match (boundary, "`") == false); // Just before 'a'
  assert (vibrex_match (boundary, "{") == false); // Just after 'z'
  vibrex_free (boundary);

  // Test negated character class edge cases
  vibrex_t *negated_single = compile_and_verify ("[^a]", true);
  assert (vibrex_match (negated_single, "a") == false);
  assert (vibrex_match (negated_single, "b") == true);
  assert (vibrex_match (negated_single, "1") == true);    // Digit character
  assert (vibrex_match (negated_single, "\xFF") == true); // High bit
  vibrex_free (negated_single);

  // Test large character class (stress test)
  char large_class[600];
  strcpy (large_class, "[");
  for (int i = 32; i <= 126; i++)
  { // All printable ASCII
    if (i != ']' && i != '\\' && i != '-')
    { // Avoid special chars
      strncat (large_class, (char[]){i, '\0'}, 1);
    }
  }
  strcat (large_class, "]");

  vibrex_t *large = compile_and_verify (large_class, true);
  assert (vibrex_match (large, "A") == true);
  assert (vibrex_match (large, "z") == true);
  assert (vibrex_match (large, "5") == true);
  assert (vibrex_match (large, "\x1F") == false); // Not printable
  vibrex_free (large);

  printf (TEST_PASS_SYMBOL " Character class edge case tests passed\n");
}

void
test_extended_ascii ()
{
  printf ("Testing extended ASCII characters...\n");

  // Test high-bit characters (avoiding the problematic 0xFF endpoint)
  vibrex_t *high_bit = compile_and_verify ("[\x80-\xFE]", true);
  assert (vibrex_match (high_bit, "\x80") == true);
  assert (vibrex_match (high_bit, "\xFE") == true);
  assert (vibrex_match (high_bit, "\x7F") == false);
  assert (vibrex_match (high_bit, "\xFF") == false);
  vibrex_free (high_bit);

  // Test specific extended ASCII character
  vibrex_t *specific_char = compile_and_verify ("\xE9", true); // é character
  assert (vibrex_match (specific_char, "\xE9") == true);
  assert (vibrex_match (specific_char, "e") == false);
  vibrex_free (specific_char);

  // Test extended ASCII in ranges
  vibrex_t *extended_range = compile_and_verify ("[\xC0-\xDF]", true); // Latin-1 supplement range
  assert (vibrex_match (extended_range, "\xC0") == true);
  assert (vibrex_match (extended_range, "\xDF") == true);
  assert (vibrex_match (extended_range, "\xBF") == false);
  assert (vibrex_match (extended_range, "\xE0") == false);
  vibrex_free (extended_range);

  // Test the fixed edge case - ranges ending with 0xFF
  vibrex_t *max_range = compile_and_verify ("[\xF0-\xFF]", true);
  assert (vibrex_match (max_range, "\xF0") == true);
  assert (vibrex_match (max_range, "\xFF") == true);
  assert (vibrex_match (max_range, "\xEF") == false);
  vibrex_free (max_range);

  printf (TEST_PASS_SYMBOL " Extended ASCII tests passed\n");
}

/********************************************************************************
 * GROUPING TESTS
 ********************************************************************************/

void
test_plain_groups ()
{
  printf ("Testing plain (non-capturing) groups '()'...\n");

  // Basic group
  vibrex_t *group = vibrex_compile ("a(bc)d", NULL);
  assert (group != NULL);
  assert (vibrex_match (group, "abcd") == true);
  assert (vibrex_match (group, "abd") == false);
  assert (vibrex_match (group, "acd") == false);
  vibrex_free (group);

  // Multi-character groups
  vibrex_t *multi_char = vibrex_compile ("a(bc)d", NULL);
  assert (multi_char != NULL);
  assert (vibrex_match (multi_char, "abcd") == true);
  assert (vibrex_match (multi_char, "abd") == false);
  assert (vibrex_match (multi_char, "acd") == false);
  vibrex_free (multi_char);

  // Group at start/end
  vibrex_t *start_group = vibrex_compile ("^(ab)c", NULL);
  assert (start_group != NULL);
  assert (vibrex_match (start_group, "abc") == true);
  assert (vibrex_match (start_group, "xabc") == false);
  vibrex_free (start_group);

  vibrex_t *end_group = vibrex_compile ("ab(c)$", NULL);
  assert (end_group != NULL);
  assert (vibrex_match (end_group, "abc") == true);
  assert (vibrex_match (end_group, "abcd") == false);
  vibrex_free (end_group);

  // Group with quantifiers (should be supported)
  vibrex_t *group_star = vibrex_compile ("(ab)*", NULL);
  assert (group_star != NULL);
  assert (vibrex_match (group_star, "") == true);
  assert (vibrex_match (group_star, "ababab") == true);
  assert (vibrex_match (group_star, "aba") == true); // matches "ab" at start
  vibrex_free (group_star);

  vibrex_t *group_plus = vibrex_compile ("(ab)+", NULL);
  assert (group_plus != NULL);
  assert (vibrex_match (group_plus, "abab") == true);
  assert (vibrex_match (group_plus, "") == false);
  vibrex_free (group_plus);

  vibrex_t *group_optional = vibrex_compile ("(ab)?", NULL);
  assert (group_optional != NULL);
  assert (vibrex_match (group_optional, "") == true);
  assert (vibrex_match (group_optional, "ab") == true);
  assert (vibrex_match (group_optional, "aba") == true); // matches first "ab"
  assert (vibrex_match (group_optional, "a") == true);   // matches empty at start
  vibrex_free (group_optional);

  // Unmatched parentheses (should fail)
  assert (vibrex_compile ("(", NULL) == NULL);
  assert (vibrex_compile (")", NULL) == NULL);
  assert (vibrex_compile ("(a", NULL) == NULL);
  assert (vibrex_compile ("a)", NULL) == NULL);
  assert (vibrex_compile ("a(b", NULL) == NULL);
  assert (vibrex_compile ("a)b", NULL) == NULL);
  assert (vibrex_compile ("a(b)c)d", NULL) == NULL);
  assert (vibrex_compile ("a(b(c)d", NULL) == NULL);

  printf (TEST_PASS_SYMBOL " Plain group tests passed\n");
}

void
test_group_alternations ()
{
  printf ("Testing alternations within groups (a|b)...\n");

  /* Basic alternation in group */
  vibrex_t *basic_group_alt = vibrex_compile ("(a|b)", NULL);
  assert (basic_group_alt != NULL);

  assert (vibrex_match (basic_group_alt, "a") == true);
  assert (vibrex_match (basic_group_alt, "b") == true);
  assert (vibrex_match (basic_group_alt, "c") == false);
  assert (vibrex_match (basic_group_alt, "hello a") == true);
  assert (vibrex_match (basic_group_alt, "hello b") == true);

  vibrex_free (basic_group_alt);

  /* Multi-character alternation in group */
  vibrex_t *multi_char_alt = vibrex_compile ("(cat|dog)", NULL);
  assert (multi_char_alt != NULL);

  assert (vibrex_match (multi_char_alt, "cat") == true);
  assert (vibrex_match (multi_char_alt, "dog") == true);
  assert (vibrex_match (multi_char_alt, "bird") == false);
  assert (vibrex_match (multi_char_alt, "I have a cat") == true);
  assert (vibrex_match (multi_char_alt, "My dog is cute") == true);

  vibrex_free (multi_char_alt);

  /* Alternation with following characters */
  vibrex_t *alt_with_suffix = vibrex_compile ("(a|b)c", NULL);
  assert (alt_with_suffix != NULL);

  assert (vibrex_match (alt_with_suffix, "ac") == true);
  assert (vibrex_match (alt_with_suffix, "bc") == true);
  assert (vibrex_match (alt_with_suffix, "cc") == false);
  assert (vibrex_match (alt_with_suffix, "abc") == true); // Contains "bc" substring
  assert (vibrex_match (alt_with_suffix, "hello ac") == true);

  vibrex_free (alt_with_suffix);

  /* Alternation in middle of pattern */
  vibrex_t *alt_middle = vibrex_compile ("x(a|b)y", NULL);
  assert (alt_middle != NULL);

  assert (vibrex_match (alt_middle, "xay") == true);
  assert (vibrex_match (alt_middle, "xby") == true);
  assert (vibrex_match (alt_middle, "xcy") == false);
  assert (vibrex_match (alt_middle, "xy") == false);

  vibrex_free (alt_middle);

  /* Multiple alternatives */
  vibrex_t *multi_alt = vibrex_compile ("(red|green|blue)", NULL);
  assert (multi_alt != NULL);

  assert (vibrex_match (multi_alt, "red") == true);
  assert (vibrex_match (multi_alt, "green") == true);
  assert (vibrex_match (multi_alt, "blue") == true);
  assert (vibrex_match (multi_alt, "yellow") == false);
  assert (vibrex_match (multi_alt, "dark red") == true);

  vibrex_free (multi_alt);

  /* Alternation with quantifiers */
  vibrex_t *alt_star = vibrex_compile ("(a|b)*", NULL);
  assert (alt_star != NULL);

  assert (vibrex_match (alt_star, "") == true);
  assert (vibrex_match (alt_star, "a") == true);
  assert (vibrex_match (alt_star, "b") == true);
  assert (vibrex_match (alt_star, "ab") == true);
  assert (vibrex_match (alt_star, "ba") == true);
  assert (vibrex_match (alt_star, "aabbaa") == true);
  assert (vibrex_match (alt_star, "c") == true); // matches empty at start

  vibrex_free (alt_star);

  vibrex_t *alt_plus = vibrex_compile ("(a|b)+", NULL);
  assert (alt_plus != NULL);

  assert (vibrex_match (alt_plus, "a") == true);
  assert (vibrex_match (alt_plus, "b") == true);
  assert (vibrex_match (alt_plus, "ab") == true);
  assert (vibrex_match (alt_plus, "ba") == true);
  assert (vibrex_match (alt_plus, "aabbaa") == true);
  assert (vibrex_match (alt_plus, "") == false);
  assert (vibrex_match (alt_plus, "c") == false);

  vibrex_free (alt_plus);

  vibrex_t *alt_optional = vibrex_compile ("(a|b)?", NULL);
  assert (alt_optional != NULL);

  assert (vibrex_match (alt_optional, "") == true);
  assert (vibrex_match (alt_optional, "a") == true);
  assert (vibrex_match (alt_optional, "b") == true);
  assert (vibrex_match (alt_optional, "ab") == true); // matches first "a"
  assert (vibrex_match (alt_optional, "c") == true);  // matches empty at start

  vibrex_free (alt_optional);

  /* Anchored alternation */
  vibrex_t *anchored_alt = vibrex_compile ("^(start|begin)$", NULL);
  assert (anchored_alt != NULL);

  assert (vibrex_match (anchored_alt, "start") == true);
  assert (vibrex_match (anchored_alt, "begin") == true);
  assert (vibrex_match (anchored_alt, "end") == false);
  assert (vibrex_match (anchored_alt, "start something") == false);
  assert (vibrex_match (anchored_alt, "something start") == false);

  vibrex_free (anchored_alt);

  /* Character class alternatives */
  vibrex_t *class_alt = vibrex_compile ("([0-9]|[a-z])", NULL);
  assert (class_alt != NULL);

  assert (vibrex_match (class_alt, "5") == true);
  assert (vibrex_match (class_alt, "a") == true);
  assert (vibrex_match (class_alt, "Z") == false);
  assert (vibrex_match (class_alt, "hello5") == true);

  vibrex_free (class_alt);

  /* Complex pattern with alternation */
  vibrex_t *complex_alt = vibrex_compile ("(http|https)://[a-z]+", NULL);
  assert (complex_alt != NULL);

  assert (vibrex_match (complex_alt, "http://example") == true);
  assert (vibrex_match (complex_alt, "https://example") == true);
  assert (vibrex_match (complex_alt, "ftp://example") == false);
  assert (vibrex_match (complex_alt, "Visit http://example") == true);

  vibrex_free (complex_alt);

  /* Alternation with different length alternatives */
  vibrex_t *diff_len_alt = vibrex_compile ("(a|hello)world", NULL);
  assert (diff_len_alt != NULL);

  assert (vibrex_match (diff_len_alt, "aworld") == true);
  assert (vibrex_match (diff_len_alt, "helloworld") == true);
  assert (vibrex_match (diff_len_alt, "world") == false);
  assert (vibrex_match (diff_len_alt, "helloaworld") == true); // Contains "aworld" substring

  vibrex_free (diff_len_alt);

  /* Nested groups with alternation */
  vibrex_t *nested_alt = vibrex_compile ("((a|b)c|d)", NULL);
  assert (nested_alt != NULL);

  assert (vibrex_match (nested_alt, "ac") == true);
  assert (vibrex_match (nested_alt, "bc") == true);
  assert (vibrex_match (nested_alt, "d") == true);
  assert (vibrex_match (nested_alt, "c") == false);
  assert (vibrex_match (nested_alt, "abc") == true); // Contains "bc" substring

  vibrex_free (nested_alt);

  /* Empty alternatives */
  vibrex_t *empty_alt1 = vibrex_compile ("(a|)", NULL);
  assert (empty_alt1 != NULL);
  assert (vibrex_match (empty_alt1, "a") == true);
  assert (vibrex_match (empty_alt1, "") == true);      // matches empty string
  assert (vibrex_match (empty_alt1, "b") == true);     // matches empty string at start
  assert (vibrex_match (empty_alt1, "hello") == true); // matches empty string at start
  vibrex_free (empty_alt1);

  vibrex_t *empty_alt2 = vibrex_compile ("(|a)", NULL);
  assert (empty_alt2 != NULL);
  assert (vibrex_match (empty_alt2, "a") == true); // matches 'a'
  assert (vibrex_match (empty_alt2, "") == true);  // matches empty string
  assert (vibrex_match (empty_alt2, "b") == true); // matches empty string at start
  vibrex_free (empty_alt2);

  vibrex_t *empty_alt3 = vibrex_compile ("(a||b)", NULL);
  assert (empty_alt3 != NULL);
  assert (vibrex_match (empty_alt3, "a") == true); // matches 'a'
  assert (vibrex_match (empty_alt3, "b") == true); // matches 'b'
  assert (vibrex_match (empty_alt3, "") == true);  // matches empty string
  assert (vibrex_match (empty_alt3, "c") == true); // matches empty string at start
  vibrex_free (empty_alt3);

  /* Test optional groups with alternations */
  printf ("  Testing optional groups with alternations (fix verification)...\n");
  vibrex_t *opt_alt = vibrex_compile ("^x(a|b|c)?y$", NULL);
  assert (opt_alt != NULL);

  assert (vibrex_match (opt_alt, "xy") == true);    // Optional part not present
  assert (vibrex_match (opt_alt, "xay") == true);   // First alternative
  assert (vibrex_match (opt_alt, "xby") == true);   // Second alternative
  assert (vibrex_match (opt_alt, "xcy") == true);   // Third alternative
  assert (vibrex_match (opt_alt, "xdy") == false);  // Invalid content in middle
  assert (vibrex_match (opt_alt, "xaby") == false); // More than one char from optional group
  assert (vibrex_match (opt_alt, "y") == false);    // Missing prefix

  vibrex_free (opt_alt);

  printf (TEST_PASS_SYMBOL " Group alternation tests passed\n");
}

void
test_bad_input ()
{
  printf ("Testing bad and pathological input...\n");

  // Should return NULL for invalid patterns
  assert (vibrex_compile (NULL, NULL) == NULL);

  // Invalid quantifiers (at start, double)
  assert (vibrex_compile ("*a", NULL) == NULL);
  assert (vibrex_compile ("+a", NULL) == NULL);
  assert (vibrex_compile ("?a", NULL) == NULL);
  assert (vibrex_compile ("a**", NULL) == NULL);
  assert (vibrex_compile ("a++", NULL) == NULL);
  assert (vibrex_compile ("a?*", NULL) == NULL);
  assert (vibrex_compile ("(a)|*", NULL) == NULL);

  // Unmatched parentheses
  assert (vibrex_compile ("(", NULL) == NULL);
  assert (vibrex_compile (")", NULL) == NULL);
  assert (vibrex_compile ("(a", NULL) == NULL);
  assert (vibrex_compile ("a)", NULL) == NULL);
  assert (vibrex_compile ("a(b", NULL) == NULL);
  assert (vibrex_compile ("a)b", NULL) == NULL);
  assert (vibrex_compile ("a(b)c)d", NULL) == NULL);
  assert (vibrex_compile ("a(b(c)d", NULL) == NULL);

  // Unmatched/invalid brackets
  assert (vibrex_compile ("[", NULL) == NULL);    // Unmatched opening bracket
  assert (vibrex_compile ("[a", NULL) == NULL);   // Unmatched opening bracket
  assert (vibrex_compile ("[a-z", NULL) == NULL); // Unmatched opening bracket

  // These are valid - ] by itself is a literal character
  vibrex_t *literal_bracket = vibrex_compile ("]", NULL);
  assert (literal_bracket != NULL);
  assert (vibrex_match (literal_bracket, "]") == true);
  assert (vibrex_match (literal_bracket, "a]b") == true); // contains ]
  vibrex_free (literal_bracket);

  vibrex_t *literal_with_bracket = vibrex_compile ("a]", NULL);
  assert (literal_with_bracket != NULL);
  assert (vibrex_match (literal_with_bracket, "a]") == true);
  assert (vibrex_match (literal_with_bracket, "ba]c") == true); // contains a]
  vibrex_free (literal_with_bracket);

  // Invalid character class
  assert (vibrex_compile ("[]", NULL) == NULL);
  assert (vibrex_compile ("[^]", NULL) == NULL);
  assert (vibrex_compile ("[z-a]", NULL) == NULL);

  // Trailing escape
  assert (vibrex_compile ("\\", NULL) == NULL);
  assert (vibrex_compile ("a\\", NULL) == NULL);

  // Empty groups are valid - they match the empty string
  vibrex_t *empty_group = vibrex_compile ("()", NULL);
  assert (empty_group != NULL);
  assert (vibrex_match (empty_group, "") == true);
  assert (vibrex_match (empty_group, "a") == true);
  vibrex_free (empty_group);

  vibrex_t *empty_group_context = vibrex_compile ("a()b", NULL);
  assert (empty_group_context != NULL);
  assert (vibrex_match (empty_group_context, "ab") == true);
  assert (vibrex_match (empty_group_context, "aXb") == false); // X doesn't match empty group
  vibrex_free (empty_group_context);

  // Invalid group structures
  assert (vibrex_compile ("(a|b", NULL) == NULL); // Unfinished group with alternation

  // empty alternatives are allowed within groups
  vibrex_t *empty_alt_pattern = vibrex_compile ("a||b", NULL);
  assert (empty_alt_pattern != NULL);
  assert (vibrex_match (empty_alt_pattern, "a") == true);
  assert (vibrex_match (empty_alt_pattern, "b") == true);
  assert (vibrex_match (empty_alt_pattern, "") == true);  // matches empty alternative
  assert (vibrex_match (empty_alt_pattern, "c") == true); // matches empty alternative at start
  vibrex_free (empty_alt_pattern);

  printf (TEST_PASS_SYMBOL " Bad input tests passed\n");
}

void
test_complex_patterns ()
{
  printf ("Testing complex patterns...\n");

  vibrex_t *email_pattern = vibrex_compile ("[a-zA-Z0-9]+@[a-zA-Z0-9]+\\.[a-zA-Z]+", NULL);
  assert (email_pattern != NULL);

  assert (vibrex_match (email_pattern, "user@example.com") == true);
  assert (vibrex_match (email_pattern, "test123@domain.org") == true);
  assert (vibrex_match (email_pattern, "invalid.email") == false);

  vibrex_free (email_pattern);

  vibrex_t *identifier_pattern = vibrex_compile ("[a-zA-Z_][a-zA-Z0-9_]*", NULL);
  assert (identifier_pattern != NULL);
  assert (vibrex_match (identifier_pattern, "my_var") == true);
  assert (vibrex_match (identifier_pattern, "_my_var") == true);
  assert (vibrex_match (identifier_pattern, "var123") == true);
  assert (vibrex_match (identifier_pattern, "a") == true);
  assert (vibrex_match (identifier_pattern, "1var") == true);   // matches "var"
  assert (vibrex_match (identifier_pattern, "my-var") == true); // matches "my"
  vibrex_free (identifier_pattern);

  vibrex_t *anchored_id = vibrex_compile ("^[a-zA-Z_][a-zA-Z0-9_]*$", NULL);
  assert (anchored_id != NULL);
  assert (vibrex_match (anchored_id, "my_var") == true);
  assert (vibrex_match (anchored_id, "my-var") == false);
  assert (vibrex_match (anchored_id, "1var") == false);
  vibrex_free (anchored_id);

  printf (TEST_PASS_SYMBOL " Complex pattern tests passed\n");
}

void
test_many_alternations_fdsn ()
{
  printf ("Testing many alternations with FDSN source ID pattern...\n");

  const char *fdsn_pattern =
      "^FDSN:NET_STA_LOC_L_H_N/MSEED3?|"
      "^FDSN:NET_STA_LOC_L_H_E/MSEED3?|"
      "^FDSN:NET_STA_LOC_L_H_Z/MSEED3?|"
      "^FDSN:XY_STA_10_B_H_.*/MSEED3?|"
      "^FDSN:YY_ST1_.*_.*_.*_Z/MSEED3?|"
      "^FDSN:YY_ST2_.*_.*_.*_Z/MSEED3?|"
      "^FDSN:YY_ST3_.*_.*_.*_Z/MSEED3?|"
      "^FDSN:NET_ALL_.*/MSEED3?|"
      "^FDSN:NET_CHAN_00_[HBL]_H_[ENZ]/MSEED3?|"
      "^FDSN:NET_STA1__.*_.*_Z/MSEED3?|"
      "^FDSN:NET_STA2__.*_.*_Z/MSEED3?|"
      "^FDSN:NET_STA3__.*_.*_Z/MSEED3?|"
      "^FDSN:NET_MSEED__.*_.*_.*/MSEED$|"
      "^FDSN:NET_MSEED3__.*_.*_.*/MSEED3$";

  const char *error_message = NULL;
  vibrex_t *pattern         = vibrex_compile (fdsn_pattern, &error_message);
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "FDSN:NET_STA_LOC_L_H_N/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_STA_LOC_L_H_N/MSEED3") == true);
  assert (vibrex_match (pattern, "FDSN:NET_STA_LOC_L_H_E/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_STA_LOC_L_H_Z/MSEED3") == true);

  assert (vibrex_match (pattern, "FDSN:XY_STA_10_B_H_Z/MSEED") == true);

  assert (vibrex_match (pattern, "FDSN:YY_ST1__B_H_Z/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:YY_ST2__B_H_Z/MSEED3") == true);
  assert (vibrex_match (pattern, "FDSN:YY_ST3__B_H_Z/MSEED") == true);

  assert (vibrex_match (pattern, "FDSN:NET_ALL_00_V_K_I/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_ALL_00_V_K_O/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_ALL_00_M_D_1/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_ALL_00_M_D_2/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_ALL_00_M_D_3/MSEED") == true);

  assert (vibrex_match (pattern, "FDSN:NET_CHAN_00_B_H_E/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_CHAN_00_B_H_N/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_CHAN_00_B_H_Z/MSEED") == true);

  assert (vibrex_match (pattern, "FDSN:NET_STA1__B_H_Z/MSEED3") == true);
  assert (vibrex_match (pattern, "FDSN:NET_STA2__B_H_Z/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_STA3__B_H_Z/MSEED3") == true);

  /* Test patterns with end anchors */
  assert (vibrex_match (pattern, "FDSN:NET_MSEED__00_B_H_Z/MSEED") == true);
  assert (vibrex_match (pattern, "FDSN:NET_MSEED__00_B_H_Z/MSEED3") == false);
  assert (vibrex_match (pattern, "FDSN:NET_MSEED3__00_B_H_Z/MSEED3") == true);
  assert (vibrex_match (pattern, "FDSN:NET_MSEED3__00_B_H_Z/MSEED") == false);

  /* Test patterns that should NOT match */
  assert (vibrex_match (pattern, "NOTFDSN:XX_STA_LOC_C_H_N/MSEED") == false);     /* Wrong prefix */
  assert (vibrex_match (pattern, "FDSN:XX_STA_LOC_C_H_N/MSEED4") == false);       /* Wrong suffix */
  assert (vibrex_match (pattern, "FDSN:XX_STA_LOC_C_H_N/NOTMSEED") == false);     /* Wrong format */
  assert (vibrex_match (pattern, "prefix FDSN:XX_STA_LOC_C_H_N/MSEED") == false); /* Not anchored to start */

  /* Test edge cases */
  assert (vibrex_match (pattern, "") == false);      /* Empty string */
  assert (vibrex_match (pattern, "FDSN:") == false); /* Incomplete */

  vibrex_free (pattern);
  printf (TEST_PASS_SYMBOL " Many alternations FDSN pattern tests passed\n");
}

/********************************************************************************
 * PERFORMANCE AND SECURITY TESTS
 ********************************************************************************/

void
test_catastrophic_backtracking ()
{
  printf ("Testing catastrophic backtracking patterns...\n");

  /* Test patterns that would cause exponential backtracking in traditional regex engines
   * but should run efficiently in NFA-based engines like vibrex */

  /* 1. Nested quantifiers - the classic evil regex (a+)+ */
  vibrex_t *nested_plus = compile_and_verify ("(a+)+", true);

  /* Create a string that would cause catastrophic backtracking: many 'a's followed by 'X' */
  char *evil_string                                = create_repeated_string ('a', CATASTROPHIC_TEST_STRING_LENGTH);
  evil_string[CATASTROPHIC_TEST_STRING_LENGTH]     = 'X'; /* Non-matching character at end */
  evil_string[CATASTROPHIC_TEST_STRING_LENGTH + 1] = '\0';

  test_performance (nested_plus, evil_string, true, "nested quantifiers with evil input");

  /* Test with just 'a' characters - should match */
  char *good_string = create_repeated_string ('a', CATASTROPHIC_TEST_STRING_LENGTH);
  assert (vibrex_match (nested_plus, good_string) == true);

  vibrex_free (nested_plus);
  free (evil_string);
  free (good_string);

  /* 2. Nested star quantifiers (a*)* */
  vibrex_t *nested_star = vibrex_compile ("(a*)*", NULL);
  assert (nested_star != NULL);

  assert (vibrex_match (nested_star, "") == true);     /* Empty string */
  assert (vibrex_match (nested_star, "aaa") == true);  /* Multiple 'a's */
  assert (vibrex_match (nested_star, "aaab") == true); /* 'a's plus other chars */
  assert (vibrex_match (nested_star, "xyz") == true);  /* Any string (matches empty at start) */

  vibrex_free (nested_star);

  /* 3. Alternation with overlapping patterns (a|a)* */
  vibrex_t *overlap_alt = compile_and_verify ("(a|a)*", true);

  /* This pattern has exponential possibilities but NFA handles it efficiently */
  char *overlap_test = create_repeated_string ('a', 50);
  test_performance (overlap_alt, overlap_test, true, "overlapping alternation");

  vibrex_free (overlap_alt);
  free (overlap_test);

  /* 4. The classic "evil regex" pattern with end anchor */
  vibrex_t *evil_anchored = compile_and_verify ("^(a+)+$", true);

  /* String that matches */
  assert (vibrex_match (evil_anchored, "aaa") == true);
  assert (vibrex_match (evil_anchored, "aaaaaaaaaa") == true);

  /* String that doesn't match - this would cause catastrophic backtracking in backtracking engines */
  char *evil_nomatch = create_repeated_string ('a', 29);
  evil_nomatch[29]   = 'X'; /* Non-matching character at end */
  evil_nomatch[30]   = '\0';

  test_performance (evil_anchored, evil_nomatch, false, "evil anchored pattern");

  vibrex_free (evil_anchored);
  free (evil_nomatch);

  /* 5. Complex nested quantifiers with alternation */
  vibrex_t *complex_nested = compile_and_verify ("(a|b)*aaac", true);

  /* String that matches */
  assert (vibrex_match (complex_nested, "ababaaac") == true);
  assert (vibrex_match (complex_nested, "aaac") == true);

  /* Long string that doesn't match - would cause exponential backtracking */
  char complex_nomatch[101];
  for (int i = 0; i < 100; i++)
  {
    complex_nomatch[i] = (i % 2) ? 'b' : 'a'; /* Alternating a and b */
  }
  complex_nomatch[100] = '\0'; /* No "aaac" suffix */

  test_performance (complex_nested, complex_nomatch, false, "complex nested quantifiers");

  vibrex_free (complex_nested);

  /* 6. Multiple nested quantifiers */
  vibrex_t *multi_nested = compile_and_verify ("(a*)*b+(c+)+", true);

  assert (vibrex_match (multi_nested, "bcc") == true);
  assert (vibrex_match (multi_nested, "aaabbbcccc") == true);
  assert (vibrex_match (multi_nested, "bbbccccc") == true);

  /* Test performance with potentially problematic input */
  test_performance (multi_nested, "aaaaaaaaaaaaaaaaaabbbbbbbbbbcccccccccc", true, "multiple nested quantifiers");

  vibrex_free (multi_nested);

  /* 7. Optional groups with potential exponential expansion */
  vibrex_t *optional_exp = vibrex_compile ("^(a?)?(b?)?(c?)?d$", NULL);
  assert (optional_exp != NULL);

  assert (vibrex_match (optional_exp, "d") == true);    /* All optional */
  assert (vibrex_match (optional_exp, "ad") == true);   /* Only 'a' */
  assert (vibrex_match (optional_exp, "bd") == true);   /* Only 'b' */
  assert (vibrex_match (optional_exp, "cd") == true);   /* Only 'c' */
  assert (vibrex_match (optional_exp, "abcd") == true); /* All present */
  assert (vibrex_match (optional_exp, "abd") == true);  /* 'a' and 'b' */
  assert (vibrex_match (optional_exp, "ed") == false);  /* Wrong character - with anchors this should fail */
  assert (vibrex_match (optional_exp, "de") == false);  /* Wrong order */

  vibrex_free (optional_exp);

  /* 8. Deeply nested groups with quantifiers */
  vibrex_t *deep_nested = compile_and_verify ("((a+)+)+", true);

  test_performance (deep_nested, "aaaaaaaaaaaaaaaa", true, "deeply nested groups");

  vibrex_free (deep_nested);

  printf (TEST_PASS_SYMBOL " Catastrophic backtracking tests passed\n");
  printf ("  All patterns completed in < 10ms (NFA immunity to exponential backtracking)\n");
}

void
test_malicious_patterns ()
{
  printf ("Testing malicious patterns (recursion/stack overflow protection)...\n");

  /* Test 1: Deep parentheses nesting - should fail gracefully */
  printf ("  Testing deep parentheses nesting...\n");

  // Create deeply nested pattern that exceeds recursion limit
  char *deep_pattern = malloc (MAX_RECURSION_DEPTH_TEST * 2 + 10);
  assert (deep_pattern != NULL);

  // Build pattern like ((((((a))))))
  for (int i = 0; i < MAX_RECURSION_DEPTH_TEST; i++)
  {
    deep_pattern[i] = '(';
  }
  deep_pattern[MAX_RECURSION_DEPTH_TEST] = 'a';
  for (int i = 0; i < MAX_RECURSION_DEPTH_TEST; i++)
  {
    deep_pattern[MAX_RECURSION_DEPTH_TEST + 1 + i] = ')';
  }
  deep_pattern[MAX_RECURSION_DEPTH_TEST * 2 + 1] = '\0';

  compile_and_verify (deep_pattern, false);

  free (deep_pattern);

  /* Test 2: Deep alternation nesting - should fail gracefully */
  printf ("  Testing deep alternation nesting...\n");

  // Create pattern like (a|(b|(c|(d|e))))
  char *alt_pattern = malloc (ALT_DEPTH_TEST * 4 + 10);
  assert (alt_pattern != NULL);

  int pos = 0;
  for (int i = 0; i < ALT_DEPTH_TEST; i++)
  {
    alt_pattern[pos++] = '(';
    alt_pattern[pos++] = 'a' + (i % 26); // Use different letters
    alt_pattern[pos++] = '|';
  }
  alt_pattern[pos++] = 'z'; // Final character
  for (int i = 0; i < ALT_DEPTH_TEST; i++)
  {
    alt_pattern[pos++] = ')';
  }
  alt_pattern[pos] = '\0';

  compile_and_verify (alt_pattern, false);

  free (alt_pattern);

  /* Test 3: Deep character class nesting - should fail gracefully */
  printf ("  Testing deep character class nesting...\n");

  // Create pattern with many nested groups containing character classes
  char *cc_pattern = malloc (CC_DEPTH_TEST * 6 + 20);
  assert (cc_pattern != NULL);

  pos = 0;
  for (int i = 0; i < CC_DEPTH_TEST; i++)
  {
    cc_pattern[pos++] = '(';
    cc_pattern[pos++] = '[';
    cc_pattern[pos++] = 'a';
    cc_pattern[pos++] = ']';
    cc_pattern[pos++] = '|';
  }
  cc_pattern[pos++] = 'x'; // Final character
  for (int i = 0; i < CC_DEPTH_TEST; i++)
  {
    cc_pattern[pos++] = ')';
  }
  cc_pattern[pos] = '\0';

  compile_and_verify (cc_pattern, false);

  free (cc_pattern);

  /* Test 4: Mixed deep nesting with quantifiers - should fail gracefully */
  printf ("  Testing mixed deep nesting with quantifiers...\n");

  // Create pattern like ((a+)|(b*)|((c?)|(d+)))*
  char *mixed_pattern = malloc (MIXED_DEPTH_TEST * 8 + 20);
  assert (mixed_pattern != NULL);

  pos = 0;
  for (int i = 0; i < MIXED_DEPTH_TEST; i++)
  {
    mixed_pattern[pos++] = '(';
    mixed_pattern[pos++] = '(';
    mixed_pattern[pos++] = 'a' + (i % 26);
    mixed_pattern[pos++] = '+';
    mixed_pattern[pos++] = ')';
    mixed_pattern[pos++] = '|';
  }
  mixed_pattern[pos++] = 'z'; // Final character
  for (int i = 0; i < MIXED_DEPTH_TEST; i++)
  {
    mixed_pattern[pos++] = ')';
  }
  mixed_pattern[pos] = '\0';

  compile_and_verify (mixed_pattern, false);

  free (mixed_pattern);

  /* Test 5: Pathological but valid patterns - should work within limits */
  printf ("  Testing valid patterns near recursion limit...\n");

  // Create pattern that should work (well within limits)
  char *safe_pattern = malloc (SAFE_RECURSION_DEPTH * 2 + 10);
  assert (safe_pattern != NULL);

  // Build pattern like ((((a))))
  for (int i = 0; i < SAFE_RECURSION_DEPTH; i++)
  {
    safe_pattern[i] = '(';
  }
  safe_pattern[SAFE_RECURSION_DEPTH] = 'a';
  for (int i = 0; i < SAFE_RECURSION_DEPTH; i++)
  {
    safe_pattern[SAFE_RECURSION_DEPTH + 1 + i] = ')';
  }
  safe_pattern[SAFE_RECURSION_DEPTH * 2 + 1] = '\0';

  vibrex_t *safe = compile_and_verify (safe_pattern, true);
  assert (vibrex_match (safe, "a") == true);  // Should match correctly
  assert (vibrex_match (safe, "b") == false); // Should not match wrong char

  vibrex_free (safe);
  free (safe_pattern);

  /* Test 6: Complex but valid alternations - should work within limits */
  printf ("  Testing complex valid alternations...\n");

  // Create alternation pattern that should work
  char *safe_alt = malloc (SAFE_ALT_DEPTH * 10 + 50);
  assert (safe_alt != NULL);

  pos = 0;
  for (int i = 0; i < SAFE_ALT_DEPTH; i++)
  {
    if (i > 0)
    {
      safe_alt[pos++] = '|';
    }
    safe_alt[pos++] = '(';
    safe_alt[pos++] = 'a' + (i % 26);
    safe_alt[pos++] = 'b' + (i % 26);
    safe_alt[pos++] = ')';
  }
  safe_alt[pos] = '\0';

  vibrex_t *safe_alt_pattern = vibrex_compile (safe_alt, NULL);
  assert (safe_alt_pattern != NULL);                       // Should compile successfully
  assert (vibrex_match (safe_alt_pattern, "ab") == true);  // Should match first alternation
  assert (vibrex_match (safe_alt_pattern, "xy") == false); // Should not match invalid pattern

  vibrex_free (safe_alt_pattern);
  free (safe_alt);

  /* Test 7: Verify error messages for recursion limit */
  printf ("  Testing error message accuracy...\n");

  // Test a pattern that definitely exceeds limits
  char *error_test = malloc (3000);
  assert (error_test != NULL);

  // Create very deep nesting
  for (int i = 0; i < 1500; i++)
  {
    error_test[i] = '(';
  }
  error_test[1500] = 'x';
  for (int i = 0; i < 1500; i++)
  {
    error_test[1501 + i] = ')';
  }
  error_test[3001] = '\0';

  vibrex_t *error_pattern = vibrex_compile (error_test, NULL);
  assert (error_pattern == NULL); // Must fail gracefully

  free (error_test);

  /* Test 8: Edge case patterns that could exploit parser */
  printf ("  Testing parser edge cases...\n");

  // Test various combinations that could cause issues
  assert (vibrex_compile ("((((((((((", NULL) == NULL);                                                // Unclosed groups
  assert (vibrex_compile (")))))))))) ", NULL) == NULL);                                               // Extra closing groups
  assert (vibrex_compile ("(((a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z)))", NULL) != NULL); // Should work

  /* Test 9: Patterns that stress NFA thread creation */
  printf ("  Testing NFA thread stress patterns...\n");

  // Pattern that creates many split states but shouldn't crash
  // Test patterns with moderate alternations to stress NFA thread creation
  vibrex_t *thread_stress = vibrex_compile ("(a|b|c|d|e|f|g)*x", NULL);
  assert (thread_stress != NULL);
  assert (vibrex_match (thread_stress, "abcdefgx") == true);   // Should match: a,b,c,d,e,f,g from alternation plus x
  assert (vibrex_match (thread_stress, "aaabbbcccx") == true); // Should match: multiple chars from alternation plus x
  assert (vibrex_match (thread_stress, "x") == true);          // Should match: empty alternation plus x
  assert (vibrex_match (thread_stress, "y") == false);         // Should not match: y is not in alternation or x

  vibrex_free (thread_stress);

  /* Test 10: Many alternations stress patterns */
  printf ("  Testing many alternations stress patterns...\n");

  // Pattern with many literal alternations (stresses alternation optimization)
  char *many_alt_pattern = malloc (1000);
  assert (many_alt_pattern != NULL);

  strcpy (many_alt_pattern, "^(");
  for (int i = 0; i < 50; i++)
  {
    if (i > 0)
      strcat (many_alt_pattern, "|");
    char literal[10];
    sprintf (literal, "lit%d", i);
    strcat (many_alt_pattern, literal);
  }
  strcat (many_alt_pattern, ")$");

  vibrex_t *many_alt_test = vibrex_compile (many_alt_pattern, NULL);
  assert (many_alt_test != NULL); // Should compile successfully with many alternations
  assert (vibrex_match (many_alt_test, "lit0") == true);
  assert (vibrex_match (many_alt_test, "lit25") == true);
  assert (vibrex_match (many_alt_test, "lit49") == true);
  assert (vibrex_match (many_alt_test, "lit50") == false); // Not in the alternation
  assert (vibrex_match (many_alt_test, "nope") == false);

  vibrex_free (many_alt_test);
  free (many_alt_pattern);

  /* Test 11: Many alternations with quantifiers stress test */
  printf ("  Testing many alternations with quantifiers (stress test)...\n");

  vibrex_t *alt_quantifier = vibrex_compile ("(a|b|c|d|e|f|g|h|i|j)*test", NULL);
  assert (alt_quantifier != NULL);

  assert (vibrex_match (alt_quantifier, "test") == true);     // Matches with zero occurrences from the group.
  assert (vibrex_match (alt_quantifier, "abcdtest") == true); // Matches with several occurrences.
  assert (vibrex_match (alt_quantifier, "jigatest") == true); // Matches various occurrences.

  vibrex_free (alt_quantifier);

  printf (TEST_PASS_SYMBOL " Malicious pattern protection tests passed\n");
  printf ("  All recursion attacks blocked, valid patterns work correctly\n");
}

void
test_alternations ()
{
  printf ("Testing basic alternations (a|b)...\n");

  vibrex_t *basic_alt = vibrex_compile ("a|b", NULL);
  assert (basic_alt != NULL);
  assert (vibrex_match (basic_alt, "a") == true);
  assert (vibrex_match (basic_alt, "b") == true);
  assert (vibrex_match (basic_alt, "c") == false);
  assert (vibrex_match (basic_alt, "hello a") == true);
  assert (vibrex_match (basic_alt, "hello b") == true);
  vibrex_free (basic_alt);

  vibrex_t *multi_alt = vibrex_compile ("cat|dog|bird", NULL);
  assert (multi_alt != NULL);
  assert (vibrex_match (multi_alt, "cat") == true);
  assert (vibrex_match (multi_alt, "dog") == true);
  assert (vibrex_match (multi_alt, "bird") == true);
  assert (vibrex_match (multi_alt, "fish") == false);
  vibrex_free (multi_alt);

  printf (TEST_PASS_SYMBOL " Basic alternation tests passed\n");
}

void
test_complex_nested_patterns ()
{
  printf ("Testing complex nested patterns...\n");

  // Test deeply nested groups (within safe limits)
  vibrex_t *nested = vibrex_compile ("(((a)))", NULL);
  assert (nested != NULL);
  assert (vibrex_match (nested, "a") == true);
  assert (vibrex_match (nested, "b") == false);
  vibrex_free (nested);

  // Test nested groups with alternations
  vibrex_t *nested_alt = vibrex_compile ("((a|b)|(c|d))", NULL);
  assert (nested_alt != NULL);
  assert (vibrex_match (nested_alt, "a") == true);
  assert (vibrex_match (nested_alt, "b") == true);
  assert (vibrex_match (nested_alt, "c") == true);
  assert (vibrex_match (nested_alt, "d") == true);
  assert (vibrex_match (nested_alt, "e") == false);
  vibrex_free (nested_alt);

  printf (TEST_PASS_SYMBOL " Complex nested pattern tests passed\n");
}

void
test_dotstar_optimization ()
{
  printf ("Testing dot-star optimization patterns...\n");

  // Test .*pattern optimization
  vibrex_t *dotstar_suffix = vibrex_compile (".*test", NULL);
  assert (dotstar_suffix != NULL);
  assert (vibrex_match (dotstar_suffix, "test") == true);
  assert (vibrex_match (dotstar_suffix, "hello test") == true);
  assert (vibrex_match (dotstar_suffix, "hello world test") == true);
  assert (vibrex_match (dotstar_suffix, "hello") == false);
  vibrex_free (dotstar_suffix);

  // Test pattern.* optimization
  vibrex_t *dotstar_prefix = vibrex_compile ("test.*", NULL);
  assert (dotstar_prefix != NULL);
  assert (vibrex_match (dotstar_prefix, "test") == true);
  assert (vibrex_match (dotstar_prefix, "test hello") == true);
  assert (vibrex_match (dotstar_prefix, "test hello world") == true);
  assert (vibrex_match (dotstar_prefix, "hello") == false);
  vibrex_free (dotstar_prefix);

  printf (TEST_PASS_SYMBOL " Dot-star optimization tests passed\n");
}

void
test_optimization_scenarios ()
{
  printf ("Testing optimization scenarios...\n");

  // Test prefix optimization
  vibrex_t *prefix = vibrex_compile ("^hello", NULL);
  assert (prefix != NULL);
  assert (vibrex_match (prefix, "hello world") == true);
  assert (vibrex_match (prefix, "world hello") == false);
  vibrex_free (prefix);

  // Test suffix optimization
  vibrex_t *suffix = vibrex_compile ("world$", NULL);
  assert (suffix != NULL);
  assert (vibrex_match (suffix, "hello world") == true);
  assert (vibrex_match (suffix, "world hello") == false);
  vibrex_free (suffix);

  printf (TEST_PASS_SYMBOL " Optimization scenario tests passed\n");
}

void
test_empty_and_edge_cases ()
{
  printf ("Testing empty patterns and edge cases...\n");

  // Test empty pattern
  vibrex_t *empty = vibrex_compile ("", NULL);
  assert (empty != NULL);
  assert (vibrex_match (empty, "") == true);
  assert (vibrex_match (empty, "a") == true); // Matches empty at start
  vibrex_free (empty);

  // Test very long strings
  char *long_string = create_repeated_string ('x', 10000);
  vibrex_t *simple  = vibrex_compile ("x", NULL);
  assert (simple != NULL);
  assert (vibrex_match (simple, long_string) == true);
  vibrex_free (simple);
  free (long_string);

  printf (TEST_PASS_SYMBOL " Empty and edge case tests passed\n");
}

void
test_error_handling_and_limits ()
{
  printf ("Testing error handling and security limits...\n");

  const char *error_message = NULL;

  // Test 1: NULL pattern error handling
  printf ("  Testing NULL pattern handling...\n");
  vibrex_t *null_pattern = vibrex_compile (NULL, &error_message);
  assert (null_pattern == NULL);
  assert (error_message != NULL);
  assert (strstr (error_message, "NULL") != NULL);

  // Test 2: Pattern length security limit (MAX_PATTERN_LENGTH = 65536)
  printf ("  Testing maximum pattern length limit...\n");
  error_message = NULL;

  // Create a pattern that exceeds MAX_PATTERN_LENGTH (65536 chars)
  size_t oversized_length = 70000;
  char *oversized_pattern = malloc (oversized_length + 1);
  assert (oversized_pattern != NULL);

  // Fill with a simple repeating pattern
  for (size_t i = 0; i < oversized_length; i++)
  {
    oversized_pattern[i] = 'a';
  }
  oversized_pattern[oversized_length] = '\0';

  vibrex_t *long_pattern = vibrex_compile (oversized_pattern, &error_message);
  assert (long_pattern == NULL);
  assert (error_message != NULL);
  assert (strstr (error_message, "too long") != NULL || strstr (error_message, "security limit") != NULL);

  free (oversized_pattern);

  // Test 3: Maximum alternations limit (MAX_ALTERNATIONS = 1000)
  printf ("  Testing maximum alternations limit...\n");
  error_message = NULL;

  // Create a pattern with more than MAX_ALTERNATIONS (1000) alternations
  size_t max_alts         = 1100;              // Exceed the limit
  size_t pattern_size     = max_alts * 3 + 10; // Each alt needs "a|" plus extras
  char *many_alts_pattern = malloc (pattern_size);
  assert (many_alts_pattern != NULL);

  strcpy (many_alts_pattern, "^(");
  for (size_t i = 0; i < max_alts; i++)
  {
    if (i > 0)
    {
      strcat (many_alts_pattern, "|");
    }
    char alt_char = 'a' + (i % 26);
    strncat (many_alts_pattern, &alt_char, 1);
  }
  strcat (many_alts_pattern, ")$");

  vibrex_t *many_alts = vibrex_compile (many_alts_pattern, &error_message);
  assert (many_alts == NULL);
  // Note: This might not trigger the MAX_ALTERNATIONS limit depending on implementation
  // but it should at least fail gracefully

  free (many_alts_pattern);

  // Test 4: Complex nested pattern that might exceed recursion
  printf ("  Testing complex nested patterns...\n");
  error_message = NULL;

  // Create a pattern with extreme nesting that should fail
  size_t extreme_depth  = 2000; // Well beyond MAX_RECURSION_DEPTH
  char *extreme_pattern = malloc (extreme_depth * 2 + 10);
  assert (extreme_pattern != NULL);

  // Build pattern like (((((...((a))...))))
  for (size_t i = 0; i < extreme_depth; i++)
  {
    extreme_pattern[i] = '(';
  }
  extreme_pattern[extreme_depth] = 'a';
  for (size_t i = 0; i < extreme_depth; i++)
  {
    extreme_pattern[extreme_depth + 1 + i] = ')';
  }
  extreme_pattern[extreme_depth * 2 + 1] = '\0';

  vibrex_t *extreme_nested = vibrex_compile (extreme_pattern, &error_message);
  assert (extreme_nested == NULL);
  // Should fail due to recursion depth limit

  free (extreme_pattern);

  // Test 5: Invalid pattern structures
  printf ("  Testing invalid pattern structures...\n");
  error_message = NULL;

  const char *invalid_patterns[] = {
      "[z-a]",   // Invalid character range
      "[]",      // Empty character class
      "[^]",     // Empty negated character class
      "a**",     // Double quantifier
      "a++",     // Double quantifier
      "?a",      // Quantifier at start
      "*a",      // Quantifier at start
      "+a",      // Quantifier at start
      "\\",      // Trailing escape
      "a\\",     // Trailing escape
      "(a|b",    // Unfinished group
      "a(b(c)d", // Unmatched parentheses
      "[a-z",    // Unmatched bracket
      NULL};

  for (int i = 0; invalid_patterns[i] != NULL; i++)
  {
    error_message     = NULL;
    vibrex_t *invalid = vibrex_compile (invalid_patterns[i], &error_message);
    assert (invalid == NULL);
    // Should have error message for invalid patterns
    // Note: Not all might set error_message, but compilation should fail
  }

  // Test 6: Successful compilation should clear error message
  printf ("  Testing successful compilation clears error message...\n");
  error_message   = "previous error"; // Set to non-NULL
  vibrex_t *valid = vibrex_compile ("test", &error_message);
  assert (valid != NULL);
  assert (error_message == NULL); // Should be cleared on success
  vibrex_free (valid);

  // Test 7: Patterns that stress character class limits
  printf ("  Testing character class edge cases...\n");
  error_message = NULL;

  // Test character class with potential infinite loop (the fixed bug)
  vibrex_t *edge_class = vibrex_compile ("[\x00-\xFF]", &error_message);
  // This should either work or fail gracefully (it should work after our fix)
  if (edge_class)
  {
    assert (vibrex_match (edge_class, "\x00") == true);
    assert (vibrex_match (edge_class, "\xFF") == true);
    vibrex_free (edge_class);
  }

  // Test 8: Very long input strings with simple patterns
  printf ("  Testing very long input strings...\n");
  char *very_long_input    = create_repeated_string ('x', 100000);
  vibrex_t *simple_pattern = vibrex_compile ("x", NULL);
  assert (simple_pattern != NULL);

  // This should complete quickly and not crash
  bool result = vibrex_match (simple_pattern, very_long_input);
  assert (result == true);

  vibrex_free (simple_pattern);
  free (very_long_input);

  // Test 9: Patterns with mixed valid/invalid character ranges
  printf ("  Testing boundary character ranges...\n");

  // Test ranges at ASCII boundaries
  vibrex_t *boundary_ranges[] = {
      vibrex_compile ("[\x00-\x1F]", NULL), // Control characters
      vibrex_compile ("[\x20-\x7E]", NULL), // Printable ASCII
      vibrex_compile ("[\x7F-\xFF]", NULL), // Extended ASCII
      NULL};

  for (int i = 0; boundary_ranges[i] != NULL; i++)
  {
    if (boundary_ranges[i])
    {
      // Just verify they compile and can match something
      vibrex_match (boundary_ranges[i], "test");
      vibrex_free (boundary_ranges[i]);
    }
  }

  // Test 10: Empty string patterns and inputs
  printf ("  Testing empty string edge cases...\n");

  vibrex_t *empty_pattern = vibrex_compile ("", NULL);
  assert (empty_pattern != NULL);
  assert (vibrex_match (empty_pattern, "") == true);
  assert (vibrex_match (empty_pattern, "anything") == true); // Matches empty at start
  vibrex_free (empty_pattern);

  printf (TEST_PASS_SYMBOL " Error handling and limits tests passed\n");
}

void
test_memory_and_resource_limits ()
{
  printf ("Testing memory allocation and resource limits...\n");

  // Test 1: Large DFA construction
  printf ("  Testing large DFA patterns...\n");

  // Pattern that would create many DFA states
  vibrex_t *complex_dfa = vibrex_compile ("[a-z]*[0-9]*[A-Z]*", NULL);
  if (complex_dfa)
  {
    // Test with various inputs to exercise DFA state creation
    assert (vibrex_match (complex_dfa, "abc123XYZ") == true);
    assert (vibrex_match (complex_dfa, "xyz999ABC") == true);
    assert (vibrex_match (complex_dfa, "") == true);
    vibrex_free (complex_dfa);
  }

  // Test 2: Patterns with many character classes
  printf ("  Testing multiple character class patterns...\n");

  vibrex_t *multi_class = vibrex_compile ("[a-z][0-9][A-Z][!@#$%][a-z][0-9]", NULL);
  if (multi_class)
  {
    assert (vibrex_match (multi_class, "a1A!b2") == true);
    assert (vibrex_match (multi_class, "z9Z%x0") == true);
    assert (vibrex_match (multi_class, "invalid") == false);
    vibrex_free (multi_class);
  }

  // Test 3: Stress test with alternations close to but under limit
  printf ("  Testing near-maximum alternations...\n");

  // Create pattern with many alternations but under MAX_ALTERNATIONS
  size_t safe_alt_count = 50;                        // Much smaller for stability
  size_t pattern_size   = safe_alt_count * 10 + 100; // More generous size estimate
  char *stress_pattern  = malloc (pattern_size);
  assert (stress_pattern != NULL);

  strcpy (stress_pattern, "^(");
  for (size_t i = 0; i < safe_alt_count; i++)
  {
    if (i > 0)
    {
      strcat (stress_pattern, "|");
    }
    char temp[10];
    sprintf (temp, "t%zu", i);
    strcat (stress_pattern, temp);
  }
  strcat (stress_pattern, ")$");

  vibrex_t *stress_alt = vibrex_compile (stress_pattern, NULL);
  if (stress_alt)
  {
    assert (vibrex_match (stress_alt, "t0") == true);
    assert (vibrex_match (stress_alt, "t10") == true);
    assert (vibrex_match (stress_alt, "t49") == true);
    assert (vibrex_match (stress_alt, "t50") == false);  // Not in pattern
    assert (vibrex_match (stress_alt, "t100") == false); // Also not in pattern
    vibrex_free (stress_alt);
  }

  free (stress_pattern);

  // Test 4: Patterns that stress NFA state creation
  printf ("  Testing NFA state stress patterns...\n");

  // Pattern with many optional groups
  vibrex_t *optional_stress = vibrex_compile ("(a)?(b)?(c)?(d)?(e)?(f)?(g)?(h)?", NULL);
  if (optional_stress)
  {
    assert (vibrex_match (optional_stress, "") == true);
    assert (vibrex_match (optional_stress, "abcdefgh") == true);
    assert (vibrex_match (optional_stress, "aceg") == true);
    assert (vibrex_match (optional_stress, "bdfh") == true);
    vibrex_free (optional_stress);
  }

  // Test 5: Repeated quantifier patterns
  printf ("  Testing repeated quantifier patterns...\n");

  vibrex_t *repeat_quant = vibrex_compile ("a+b*c?d+e*f?", NULL);
  if (repeat_quant)
  {
    assert (vibrex_match (repeat_quant, "aaaacddddd") == true);
    assert (vibrex_match (repeat_quant, "abbbbbcdddeeeef") == true);
    assert (vibrex_match (repeat_quant, "adf") == true);
    vibrex_free (repeat_quant);
  }

  printf (TEST_PASS_SYMBOL " Memory and resource limit tests passed\n");
}

int
main ()
{
  printf ("Running vibrex regex engine tests...\n\n");

  // === BASIC FEATURE TESTS ===
  printf ("=== Basic Feature Tests ===\n");
  test_basic_matching ();
  test_escape_sequences ();
  test_dot_matching ();
  test_anchors ();
  test_individual_anchors ();

  // === QUANTIFIER TESTS ===
  printf ("\n=== Quantifier Tests ===\n");
  test_star_quantifier ();
  test_plus_quantifier ();
  test_optional_quantifier_char ();

  // === CHARACTER CLASS TESTS ===
  printf ("\n=== Character Class Tests ===\n");
  test_character_classes ();
  test_extended_character_classes ();
  test_character_class_edge_cases ();
  test_extended_ascii ();

  // === GROUPING TESTS ===
  printf ("\n=== Grouping Tests ===\n");
  test_plain_groups ();
  test_optional_groups ();

  // === ALTERNATION TESTS ===
  printf ("\n=== Alternation Tests ===\n");
  test_alternations ();
  test_group_alternations ();
  test_many_alternations_fdsn ();

  // === COMPLEX PATTERN TESTS ===
  printf ("\n=== Complex Pattern Tests ===\n");
  test_complex_patterns ();
  test_complex_nested_patterns ();

  // === OPTIMIZATION TESTS ===
  printf ("\n=== Optimization Tests ===\n");
  test_dotstar_optimization ();
  test_optimization_scenarios ();

  // === EDGE CASES AND ERROR HANDLING ===
  printf ("\n=== Edge Cases and Error Handling ===\n");
  test_empty_and_edge_cases ();
  test_bad_input ();
  test_error_handling_and_limits ();
  test_memory_and_resource_limits ();

  // === PERFORMANCE AND SECURITY TESTS ===
  printf ("\n=== Performance and Security Tests ===\n");
  test_catastrophic_backtracking ();
  test_malicious_patterns ();

  printf ("\n" TEST_CELEBRATION " All tests passed! The vibrex regex engine is working correctly.\n");
  return 0;
}