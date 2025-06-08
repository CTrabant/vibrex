#include "vibrex.h"
#include <assert.h>
#include <stdio.h>

void
test_basic_matching ()
{
  printf ("Testing basic character matching...\n");

  vibrex_t *pattern = vibrex_compile ("hello");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, "hello world") == true);
  assert (vibrex_match (pattern, "say hello") == true);
  assert (vibrex_match (pattern, "hi") == false);

  vibrex_free (pattern);
  printf ("✓ Basic matching tests passed\n");
}

void
test_dot_matching ()
{
  printf ("Testing dot (.) matching...\n");

  vibrex_t *pattern = vibrex_compile ("h.llo");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, "hallo") == true);
  assert (vibrex_match (pattern, "hxllo") == true);
  assert (vibrex_match (pattern, "h@llo") == true);
  assert (vibrex_match (pattern, "hllo") == false);

  vibrex_free (pattern);

  pattern = vibrex_compile (".ello");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, " ello") == true);
  assert (vibrex_match (pattern, "ello") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("hell.");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "hello") == true);
  assert (vibrex_match (pattern, "hell!") == true);
  assert (vibrex_match (pattern, "hell") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("a.+c");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "axbyc") == true);
  assert (vibrex_match (pattern, "ac") == false);
  vibrex_free (pattern);

  printf ("✓ Dot matching tests passed\n");
}

void
test_star_quantifier ()
{
  printf ("Testing star (*) quantifier...\n");

  vibrex_t *pattern = vibrex_compile ("ab*c");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "ac") == true);
  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "abbc") == true);
  assert (vibrex_match (pattern, "abbbbbc") == true);
  assert (vibrex_match (pattern, "axc") == false);
  assert (vibrex_match (pattern, "xacx") == true); // matches 'ac'

  vibrex_free (pattern);

  pattern = vibrex_compile ("^a*b");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aaab") == true);
  assert (vibrex_match (pattern, "cab") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("ab*$");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "a") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "abbb") == true);
  assert (vibrex_match (pattern, "abc") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("a*b*c");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "c") == true);
  assert (vibrex_match (pattern, "ac") == true);
  assert (vibrex_match (pattern, "bc") == true);
  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "aaabbc") == true);
  assert (vibrex_match (pattern, "aabbbc") == true);
  vibrex_free (pattern);

  pattern = vibrex_compile ("a.*b");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "axbybzc") == true); // Should match "axbyb"
  vibrex_free (pattern);

  printf ("✓ Star quantifier tests passed\n");
}

void
test_plus_quantifier ()
{
  printf ("Testing plus (+) quantifier...\n");

  vibrex_t *pattern = vibrex_compile ("ab+c");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "abbc") == true);
  assert (vibrex_match (pattern, "abbbbbc") == true);
  assert (vibrex_match (pattern, "ac") == false);
  assert (vibrex_match (pattern, "xabcy") == true);

  vibrex_free (pattern);

  pattern = vibrex_compile ("^a+b");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aaab") == true);
  assert (vibrex_match (pattern, "b") == false);
  assert (vibrex_match (pattern, "cab") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("ab+$");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "abbb") == true);
  assert (vibrex_match (pattern, "a") == false);
  assert (vibrex_match (pattern, "abc") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("a+b+c");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "aabbc") == true);
  assert (vibrex_match (pattern, "c") == false);
  assert (vibrex_match (pattern, "ac") == false);
  assert (vibrex_match (pattern, "bc") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("a.+b");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "axbybzc") == true); // Should match "axbyb"
  assert (vibrex_match (pattern, "ab") == false);
  vibrex_free (pattern);

  printf ("✓ Plus quantifier tests passed\n");
}

void
test_non_capturing_groups ()
{
  printf ("Testing non-capturing groups (?:...)...\n");

  /* Basic group without quantifiers */
  vibrex_t *basic_group = vibrex_compile ("a(?:bc)d");
  assert (basic_group != NULL);

  assert (vibrex_match (basic_group, "abcd") == true);
  assert (vibrex_match (basic_group, "abd") == false);
  assert (vibrex_match (basic_group, "acd") == false);

  vibrex_free (basic_group);

  /* Multiple groups */
  vibrex_t *multi_group = vibrex_compile ("(?:ab)(?:cd)");
  assert (multi_group != NULL);

  assert (vibrex_match (multi_group, "abcd") == true);
  assert (vibrex_match (multi_group, "ab") == false);
  assert (vibrex_match (multi_group, "cd") == false);
  assert (vibrex_match (multi_group, "ac") == false);

  vibrex_free (multi_group);

  /* Groups with character classes */
  vibrex_t *class_group = vibrex_compile ("(?:[0-9])");
  assert (class_group != NULL);

  assert (vibrex_match (class_group, "5") == true);
  assert (vibrex_match (class_group, "abc") == false);

  vibrex_free (class_group);

  /* Nested non-capturing groups */
  vibrex_t *nested_group = vibrex_compile ("a(?:b(?:c))d");
  assert (nested_group != NULL);
  assert (vibrex_match (nested_group, "abcd") == true);
  assert (vibrex_match (nested_group, "abd") == false);
  vibrex_free (nested_group);

  /* Groups at start/end */
  vibrex_t *start_group = vibrex_compile ("^(?:ab)c");
  assert (start_group != NULL);
  assert (vibrex_match (start_group, "abc") == true);
  assert (vibrex_match (start_group, "xabc") == false);
  vibrex_free (start_group);

  vibrex_t *end_group = vibrex_compile ("a(?:bc)$");
  assert (end_group != NULL);
  assert (vibrex_match (end_group, "abc") == true);
  assert (vibrex_match (end_group, "abcd") == false);
  vibrex_free (end_group);

  printf ("✓ Non-capturing group tests passed\n");
}

void
test_optional_groups ()
{
  printf ("Testing optional groups (?:...)?...\n");

  /* Basic optional group */
  vibrex_t *optional_group = vibrex_compile ("a(?:bc)?d");
  assert (optional_group != NULL);

  assert (vibrex_match (optional_group, "abcd") == true); // with optional part
  assert (vibrex_match (optional_group, "ad") == true);   // without optional part
  assert (vibrex_match (optional_group, "abd") == false); // partial match
  assert (vibrex_match (optional_group, "acd") == false); // wrong content

  vibrex_free (optional_group);

  /* Multiple optional groups */
  vibrex_t *multi_optional = vibrex_compile ("(?:ab)?(?:cd)?");
  assert (multi_optional != NULL);

  assert (vibrex_match (multi_optional, "abcd") == true); // both present
  assert (vibrex_match (multi_optional, "ab") == true);   // first only
  assert (vibrex_match (multi_optional, "cd") == true);   // second only
  assert (vibrex_match (multi_optional, "") == true);     // neither present
  assert (vibrex_match (multi_optional, "ac") == true);   // matches empty at start

  vibrex_free (multi_optional);

  /* Multiple optional groups with anchors */
  vibrex_t *anchored_optional = vibrex_compile ("^(?:ab)?(?:cd)?$");
  assert (anchored_optional != NULL);

  assert (vibrex_match (anchored_optional, "abcd") == true); // both present
  assert (vibrex_match (anchored_optional, "ab") == true);   // first only
  assert (vibrex_match (anchored_optional, "cd") == true);   // second only
  assert (vibrex_match (anchored_optional, "") == true);     // neither present
  assert (vibrex_match (anchored_optional, "ac") == false);  // wrong content - must match entire string

  vibrex_free (anchored_optional);

  /* Optional groups with quantifiers */
  vibrex_t *optional_with_quant = vibrex_compile ("a(?:b+)?c");
  assert (optional_with_quant != NULL);

  assert (vibrex_match (optional_with_quant, "abc") == true);   // with single b
  assert (vibrex_match (optional_with_quant, "abbbc") == true); // with multiple b's
  assert (vibrex_match (optional_with_quant, "ac") == true);    // without optional part
  assert (vibrex_match (optional_with_quant, "axc") == false);  // wrong content

  vibrex_free (optional_with_quant);

  /* Optional groups with character classes */
  vibrex_t *optional_class = vibrex_compile ("(?:[0-9]+)?[a-z]");
  assert (optional_class != NULL);

  assert (vibrex_match (optional_class, "123a") == true); // with digits
  assert (vibrex_match (optional_class, "a") == true);    // without digits
  assert (vibrex_match (optional_class, "123") == false); // missing letter
  assert (vibrex_match (optional_class, "A") == false);   // wrong case

  vibrex_free (optional_class);

  /* Nested optional groups */
  vibrex_t *nested_optional = vibrex_compile ("a(?:b(?:c)?)?d");
  assert (nested_optional != NULL);

  assert (vibrex_match (nested_optional, "abcd") == true); // all parts
  assert (vibrex_match (nested_optional, "abd") == true);  // outer group, no inner
  assert (vibrex_match (nested_optional, "ad") == true);   // no optional parts
  assert (vibrex_match (nested_optional, "acd") == false); // wrong structure

  vibrex_free (nested_optional);

  /* Note: Optional groups with alternations are not yet implemented */
  printf ("  (Note: Optional groups with alternations (?:a|b)? are not yet implemented)\n");

  printf ("✓ Optional group tests passed\n");
}

void
test_optional_quantifier_char ()
{
  printf ("Testing optional (?) quantifier on characters...\n");

  vibrex_t *pattern = vibrex_compile ("ab?c");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "ac") == true);
  assert (vibrex_match (pattern, "abc") == true);
  assert (vibrex_match (pattern, "abbc") == false);
  vibrex_free (pattern);

  pattern = vibrex_compile ("^a?b$");
  assert (pattern != NULL);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "ab") == true);
  assert (vibrex_match (pattern, "aab") == false);
  vibrex_free (pattern);

  printf ("✓ Optional (?) on characters tests passed\n");
}

void
test_anchors ()
{
  printf ("Testing anchors (^ and $)...\n");

  vibrex_t *start_pattern = vibrex_compile ("^hello");
  assert (start_pattern != NULL);

  assert (vibrex_match (start_pattern, "hello world") == true);
  assert (vibrex_match (start_pattern, "say hello") == false);
  assert (vibrex_match (start_pattern, "hello") == true);

  vibrex_free (start_pattern);

  vibrex_t *end_pattern = vibrex_compile ("world$");
  assert (end_pattern != NULL);

  assert (vibrex_match (end_pattern, "hello world") == true);
  assert (vibrex_match (end_pattern, "world peace") == false);
  assert (vibrex_match (end_pattern, "world") == true);

  vibrex_free (end_pattern);

  vibrex_t *both_pattern = vibrex_compile ("^hello$");
  assert (both_pattern != NULL);

  assert (vibrex_match (both_pattern, "hello") == true);
  assert (vibrex_match (both_pattern, "hello world") == false);
  assert (vibrex_match (both_pattern, "say hello") == false);

  vibrex_free (both_pattern);

  /* Anchors with quantifiers */
  vibrex_t *anchor_quant = vibrex_compile ("^a*$");
  assert (anchor_quant != NULL);
  assert (vibrex_match (anchor_quant, "aaa") == true);
  assert (vibrex_match (anchor_quant, "") == true);
  assert (vibrex_match (anchor_quant, "b") == false);
  assert (vibrex_match (anchor_quant, "aaab") == false);
  vibrex_free (anchor_quant);

  /* Anchor on empty string */
  vibrex_t *empty_anchor = vibrex_compile ("^$");
  assert (empty_anchor != NULL);
  assert (vibrex_match (empty_anchor, "") == true);
  assert (vibrex_match (empty_anchor, "a") == false);
  vibrex_free (empty_anchor);

  printf ("✓ Anchor tests passed\n");
}

void
test_character_classes ()
{
  printf ("Testing character classes...\n");

  vibrex_t *pattern = vibrex_compile ("[abc]");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "a") == true);
  assert (vibrex_match (pattern, "b") == true);
  assert (vibrex_match (pattern, "c") == true);
  assert (vibrex_match (pattern, "d") == false);

  vibrex_free (pattern);

  vibrex_t *range_pattern = vibrex_compile ("[a-z]");
  assert (range_pattern != NULL);

  assert (vibrex_match (range_pattern, "hello") == true);
  assert (vibrex_match (range_pattern, "HELLO") == false);

  vibrex_free (range_pattern);

  vibrex_t *negated_pattern = vibrex_compile ("[^0-9]");
  assert (negated_pattern != NULL);

  assert (vibrex_match (negated_pattern, "hello") == true);
  assert (vibrex_match (negated_pattern, "123") == false);

  vibrex_free (negated_pattern);
  printf ("✓ Character class tests passed\n");
}

void
test_extended_character_classes ()
{
  printf ("Testing extended character class features...\n");

  /* Multiple ranges in one class */
  vibrex_t *multi_range = vibrex_compile ("[a-zA-Z0-9]");
  assert (multi_range != NULL);

  assert (vibrex_match (multi_range, "a") == true);
  assert (vibrex_match (multi_range, "Z") == true);
  assert (vibrex_match (multi_range, "5") == true);
  assert (vibrex_match (multi_range, "@") == false);
  assert (vibrex_match (multi_range, " ") == false);

  vibrex_free (multi_range);

  /* Mixed literals and ranges */
  vibrex_t *mixed_class = vibrex_compile ("[a-z.@_0-9]");
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
  vibrex_t *class_plus = vibrex_compile ("[0-9]+");
  assert (class_plus != NULL);

  assert (vibrex_match (class_plus, "123") == true);
  assert (vibrex_match (class_plus, "0") == true);
  assert (vibrex_match (class_plus, "abc123") == true); // contains digits
  assert (vibrex_match (class_plus, "abc") == false);   // no digits

  vibrex_free (class_plus);

  vibrex_t *class_star = vibrex_compile ("[a-z]*");
  assert (class_star != NULL);

  assert (vibrex_match (class_star, "hello") == true);
  assert (vibrex_match (class_star, "") == true);         // zero matches
  assert (vibrex_match (class_star, "123hello") == true); // matches empty at start
  assert (vibrex_match (class_star, "HELLO") == true);    // matches empty at start

  vibrex_free (class_star);

  vibrex_t *class_optional = vibrex_compile ("[0-9]?");
  assert (class_optional != NULL);

  assert (vibrex_match (class_optional, "5") == true);
  assert (vibrex_match (class_optional, "") == true);   // zero matches
  assert (vibrex_match (class_optional, "a5") == true); // matches '5'
  assert (vibrex_match (class_optional, "a") == true);  // matches empty string at start
  assert (vibrex_match (class_optional, "55") == true); // matches first digit

  vibrex_free (class_optional);

  /* Complex negated character classes */
  vibrex_t *complex_negated = vibrex_compile ("[^a-zA-Z0-9]");
  assert (complex_negated != NULL);

  assert (vibrex_match (complex_negated, "@") == true);
  assert (vibrex_match (complex_negated, " ") == true);
  assert (vibrex_match (complex_negated, "!") == true);
  assert (vibrex_match (complex_negated, "a") == false);
  assert (vibrex_match (complex_negated, "Z") == false);
  assert (vibrex_match (complex_negated, "5") == false);

  vibrex_free (complex_negated);

  /* Hyphen as a literal */
  vibrex_t *hyphen_literal = vibrex_compile ("[-az]"); // at the start
  assert (hyphen_literal != NULL);
  assert (vibrex_match (hyphen_literal, "-") == true);
  assert (vibrex_match (hyphen_literal, "a") == true);
  assert (vibrex_match (hyphen_literal, "z") == true);
  assert (vibrex_match (hyphen_literal, "b") == false);
  vibrex_free (hyphen_literal);

  hyphen_literal = vibrex_compile ("[az-]"); // at the end
  assert (hyphen_literal != NULL);
  assert (vibrex_match (hyphen_literal, "-") == true);
  assert (vibrex_match (hyphen_literal, "a") == true);
  assert (vibrex_match (hyphen_literal, "z") == true);
  assert (vibrex_match (hyphen_literal, "b") == false);
  vibrex_free (hyphen_literal);

  /* Caret as a literal */
  vibrex_t *caret_literal = vibrex_compile ("[a^c]"); // not at the start
  assert (caret_literal != NULL);
  assert (vibrex_match (caret_literal, "a") == true);
  assert (vibrex_match (caret_literal, "^") == true);
  assert (vibrex_match (caret_literal, "c") == true);
  assert (vibrex_match (caret_literal, "b") == false);
  vibrex_free (caret_literal);

  /* Note: Escaped characters in character classes are not yet implemented */
  printf ("  (Note: Escaped characters in character classes [\\[\\]] are not yet implemented)\n");

  /* Character classes in complex patterns */
  vibrex_t *complex_pattern = vibrex_compile ("[a-z]+@[a-z]+\\.[a-z]+");
  assert (complex_pattern != NULL);

  assert (vibrex_match (complex_pattern, "user@example.com") == true);
  assert (vibrex_match (complex_pattern, "test@domain.org") == true);
  assert (vibrex_match (complex_pattern, "User@Example.Com") == false); // uppercase
  assert (vibrex_match (complex_pattern, "user.example.com") == false); // missing @

  vibrex_free (complex_pattern);

  /* Character class ranges with special characters */
  vibrex_t *special_range = vibrex_compile ("[!-/]"); // ASCII range from ! to /
  assert (special_range != NULL);

  assert (vibrex_match (special_range, "!") == true);
  assert (vibrex_match (special_range, "#") == true);
  assert (vibrex_match (special_range, "/") == true);
  assert (vibrex_match (special_range, "0") == false);
  assert (vibrex_match (special_range, " ") == false);

  vibrex_free (special_range);

  printf ("✓ Extended character class tests passed\n");
}

void
test_dotstar_optimization ()
{
  printf ("Testing .* optimization...\n");

  vibrex_t *pattern = vibrex_compile (".*");
  assert (pattern != NULL);

  assert (vibrex_match (pattern, "") == true);
  assert (vibrex_match (pattern, "anything") == true);
  assert (vibrex_match (pattern, "hello world 123!@#") == true);

  vibrex_free (pattern);
  printf ("✓ .* optimization tests passed\n");
}

void
test_alternations ()
{
  printf ("Testing alternations (|)...\n");

  vibrex_t *simple_alt = vibrex_compile ("cat|dog");
  assert (simple_alt != NULL);

  assert (vibrex_match (simple_alt, "cat") == true);
  assert (vibrex_match (simple_alt, "dog") == true);
  assert (vibrex_match (simple_alt, "bird") == false);
  assert (vibrex_match (simple_alt, "I have a cat") == true);
  assert (vibrex_match (simple_alt, "My dog is cute") == true);

  vibrex_free (simple_alt);

  vibrex_t *multi_alt = vibrex_compile ("red|green|blue");
  assert (multi_alt != NULL);

  assert (vibrex_match (multi_alt, "red") == true);
  assert (vibrex_match (multi_alt, "green") == true);
  assert (vibrex_match (multi_alt, "blue") == true);
  assert (vibrex_match (multi_alt, "yellow") == false);
  assert (vibrex_match (multi_alt, "dark red") == true);

  vibrex_free (multi_alt);

  vibrex_t *mixed_alt = vibrex_compile ("a+|b*|c");
  assert (mixed_alt != NULL);

  assert (vibrex_match (mixed_alt, "a") == true);
  assert (vibrex_match (mixed_alt, "aaa") == true);
  assert (vibrex_match (mixed_alt, "b") == true);
  assert (vibrex_match (mixed_alt, "bbb") == true);
  assert (vibrex_match (mixed_alt, "c") == true);
  assert (vibrex_match (mixed_alt, "") == true);  // matches b* (zero b's)
  assert (vibrex_match (mixed_alt, "d") == true); // matches b* (zero b's) at start

  vibrex_t *no_star_alt = vibrex_compile ("a+|b+|c");
  assert (no_star_alt != NULL);

  assert (vibrex_match (no_star_alt, "a") == true);
  assert (vibrex_match (no_star_alt, "bbb") == true);
  assert (vibrex_match (no_star_alt, "c") == true);
  assert (vibrex_match (no_star_alt, "d") == false); // no alternative matches

  vibrex_free (no_star_alt);

  vibrex_free (mixed_alt);

  vibrex_t *anchor_alt = vibrex_compile ("^start|end$");
  assert (anchor_alt != NULL);

  assert (vibrex_match (anchor_alt, "start") == true);
  assert (vibrex_match (anchor_alt, "start something") == true);
  assert (vibrex_match (anchor_alt, "something end") == true);
  assert (vibrex_match (anchor_alt, "middle start") == false);
  assert (vibrex_match (anchor_alt, "end middle") == false);

  vibrex_free (anchor_alt);

  vibrex_t *char_alt = vibrex_compile ("[0-9]|[a-z]");
  assert (char_alt != NULL);

  assert (vibrex_match (char_alt, "5") == true);
  assert (vibrex_match (char_alt, "a") == true);
  assert (vibrex_match (char_alt, "Z") == false);
  assert (vibrex_match (char_alt, "hello5") == true);

  vibrex_free (char_alt);

  printf ("✓ Alternation tests passed\n");
}

void
test_bad_input ()
{
  printf ("Testing bad and pathological input...\n");

  // Should return NULL for invalid patterns
  assert (vibrex_compile (NULL) == NULL);

  // Invalid quantifiers (at start, double)
  assert (vibrex_compile ("*a") == NULL);
  assert (vibrex_compile ("+a") == NULL);
  assert (vibrex_compile ("?a") == NULL);
  assert (vibrex_compile ("a**") == NULL);
  assert (vibrex_compile ("a++") == NULL);
  assert (vibrex_compile ("a?*") == NULL);
  assert (vibrex_compile ("(?:a)|*") == NULL);

  // Unmatched parentheses
  assert (vibrex_compile ("(") == NULL);
  assert (vibrex_compile (")") == NULL);
  assert (vibrex_compile ("(a") == NULL);
  assert (vibrex_compile ("a)") == NULL);
  assert (vibrex_compile ("a(b") == NULL);
  assert (vibrex_compile ("a)b") == NULL);
  assert (vibrex_compile ("a(b)c)d") == NULL);
  assert (vibrex_compile ("a(b(c)d") == NULL);

  // Unmatched brackets
  assert (vibrex_compile ("[") == NULL);
  assert (vibrex_compile ("]") == NULL);
  assert (vibrex_compile ("[a") == NULL);
  assert (vibrex_compile ("a]") == NULL);
  assert (vibrex_compile ("[a-z") == NULL);

  // Invalid character class
  assert (vibrex_compile ("[]") == NULL);
  assert (vibrex_compile ("[^]") == NULL);
  assert (vibrex_compile ("[z-a]") == NULL);

  // Trailing escape
  assert (vibrex_compile ("\\") == NULL);
  assert (vibrex_compile ("a\\") == NULL);

  // Invalid group structures
  assert (vibrex_compile ("(?:)") == NULL);   // Empty non-capturing group
  assert (vibrex_compile ("(?:a|b") == NULL); // Unfinished group with alternation

  // Invalid alternations
  assert (vibrex_compile ("|a") == NULL);
  assert (vibrex_compile ("ab|") == NULL);
  assert (vibrex_compile ("a||b") == NULL);

  printf ("✓ Bad input tests passed\n");
}

void
test_complex_patterns ()
{
  printf ("Testing complex patterns...\n");

  vibrex_t *email_pattern = vibrex_compile ("[a-zA-Z0-9]+@[a-zA-Z0-9]+\\.[a-zA-Z]+");
  assert (email_pattern != NULL);

  assert (vibrex_match (email_pattern, "user@example.com") == true);
  assert (vibrex_match (email_pattern, "test123@domain.org") == true);
  assert (vibrex_match (email_pattern, "invalid.email") == false);

  vibrex_free (email_pattern);

  vibrex_t *identifier_pattern = vibrex_compile ("[a-zA-Z_][a-zA-Z0-9_]*");
  assert (identifier_pattern != NULL);
  assert (vibrex_match (identifier_pattern, "my_var") == true);
  assert (vibrex_match (identifier_pattern, "_my_var") == true);
  assert (vibrex_match (identifier_pattern, "var123") == true);
  assert (vibrex_match (identifier_pattern, "a") == true);
  assert (vibrex_match (identifier_pattern, "1var") == true);   // matches "var"
  assert (vibrex_match (identifier_pattern, "my-var") == true); // matches "my"
  vibrex_free (identifier_pattern);

  vibrex_t *anchored_id = vibrex_compile ("^[a-zA-Z_][a-zA-Z0-9_]*$");
  assert (anchored_id != NULL);
  assert (vibrex_match (anchored_id, "my_var") == true);
  assert (vibrex_match (anchored_id, "my-var") == false);
  assert (vibrex_match (anchored_id, "1var") == false);
  vibrex_free (anchored_id);

  printf ("✓ Complex pattern tests passed\n");
}

void
test_plain_groups ()
{
  printf ("Testing plain (non-capturing) groups '()'...\n");

  // Basic group
  vibrex_t *group = vibrex_compile ("a(bc)d");
  assert (group != NULL);
  assert (vibrex_match (group, "abcd") == true);
  assert (vibrex_match (group, "abd") == false);
  assert (vibrex_match (group, "acd") == false);
  vibrex_free (group);

  // Nested groups
  vibrex_t *nested = vibrex_compile ("a(b(c))d");
  assert (nested != NULL);
  assert (vibrex_match (nested, "abcd") == true);
  assert (vibrex_match (nested, "abd") == false);
  assert (vibrex_match (nested, "acd") == false);
  vibrex_free (nested);

  // Group at start/end
  vibrex_t *start_group = vibrex_compile ("^(ab)c");
  assert (start_group != NULL);
  assert (vibrex_match (start_group, "abc") == true);
  assert (vibrex_match (start_group, "xabc") == false);
  vibrex_free (start_group);

  vibrex_t *end_group = vibrex_compile ("ab(c)$");
  assert (end_group != NULL);
  assert (vibrex_match (end_group, "abc") == true);
  assert (vibrex_match (end_group, "abcd") == false);
  vibrex_free (end_group);

  // Group with quantifiers (should be supported)
  vibrex_t *group_star = vibrex_compile ("(ab)*");
  assert (group_star != NULL);
  assert (vibrex_match (group_star, "") == true);
  assert (vibrex_match (group_star, "ab") == true);
  assert (vibrex_match (group_star, "abab") == true);
  assert (vibrex_match (group_star, "aba") == true); // matches "ab" at start
  vibrex_free (group_star);

  vibrex_t *group_plus = vibrex_compile ("(ab)+");
  assert (group_plus != NULL);
  assert (vibrex_match (group_plus, "ab") == true);
  assert (vibrex_match (group_plus, "abab") == true);
  assert (vibrex_match (group_plus, "") == false);
  assert (vibrex_match (group_plus, "aba") == true); // matches "ab" at start
  vibrex_free (group_plus);

  vibrex_t *group_optional = vibrex_compile ("(ab)?");
  assert (group_optional != NULL);
  assert (vibrex_match (group_optional, "") == true);
  assert (vibrex_match (group_optional, "ab") == true);
  assert (vibrex_match (group_optional, "abab") == true); // matches first "ab"
  assert (vibrex_match (group_optional, "a") == true);    // matches empty at start
  vibrex_free (group_optional);

  // Unmatched parentheses (should fail)
  assert (vibrex_compile ("(") == NULL);
  assert (vibrex_compile (")") == NULL);
  assert (vibrex_compile ("(a") == NULL);
  assert (vibrex_compile ("a)") == NULL);
  assert (vibrex_compile ("a(b") == NULL);
  assert (vibrex_compile ("a)b") == NULL);
  assert (vibrex_compile ("a(b)c)d") == NULL);
  assert (vibrex_compile ("a(b(c)d") == NULL);

  printf ("\u2713 Plain group tests passed\n");
}

int
main ()
{
  printf ("Running vibrex regex engine tests...\n\n");

  test_basic_matching ();
  test_dot_matching ();
  test_star_quantifier ();
  test_plus_quantifier ();
  test_non_capturing_groups ();
  test_optional_groups ();
  test_optional_quantifier_char ();
  test_anchors ();
  test_character_classes ();
  test_extended_character_classes ();
  test_dotstar_optimization ();
  test_alternations ();
  test_bad_input ();
  test_complex_patterns ();
  test_plain_groups ();

  printf ("\n🎉 All tests passed! The vibrex regex engine is working correctly.\n");
  return 0;
}