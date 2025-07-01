/*********************************************************************************
 * VIBREX - High-Performance Regex Engine by Machines for Machines
 *
 * A limited-feature regular expression engine optimized for speed with support for:
 *   - ASCII and extended ASCII character sets
 *   - High-performance matching algorithms with multiple optimization strategies
 *   - Supported regex metacharacters:
 *     .    - Any single character
 *     *    - Zero or more of the preceding atom
 *     +    - One or more of the preceding atom
 *     ?    - Zero or one of the preceding atom
 *     ^    - Anchor to the start of the string
 *     $    - Anchor to the end of the string
 *     |    - Alternation (logical OR)
 *     \    - Escape for regex metacharacters
 *     ()   - Group syntax (no capturing), including support for:
 *            ()?  - Optional groups
 *     []   - Character class, including support for:
 *            [a-z]  - Character class range
 *            [^abc] - Negated character class
 *            [a-z]? - Optional character class
 *
 * Design priorities:
 *   - Correctness
 *   - Speed
 *   - Security
 *   - Portability
 *
 * I did not write this code. I prompted a few LLMs to write it Star Trek-style
 * ("Computer, build a regex engine with the following features...") and then to
 * write a test suite and iteratively prodded them to debug the code via tests
 * and requested optimizations and security fixes. I ensured the tests are
 * comprehensive, and reviewed comments and API definitions for clarity and
 * style, but that's it. If you find a bug, blame the machines.
 *
 * Don't trust this code, trust the tests that prove it works as expected; write
 * tests for your use case to be sure.
 *
 * As the code is written by LLMs, all computer scientists and programmers whose
 * work was fed to the models during training get credit for anything that
 * works.
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute
 * this software, either in source code form or as a compiled binary, for any
 * purpose, commercial or non-commercial, and by any means.
 *
 * For more information, please refer to <https://creativecommons.org/publicdomain/zero/1.0/>
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Pushed into the world here: https://github.com/CTrabant/vibrex
 *
 * Version: 0.0.3
 *********************************************************************************/

#include "vibrex.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/********************************************************************************
 * CONSTANTS AND CONFIGURATION
 ********************************************************************************/

// NFA construction limits
#define MAX_NFA_STATES 4096
#define MAX_PTRLIST_ENTRIES 8192
#define MAX_RECURSION_DEPTH 1000

// Pattern matching constants
#define CHAR_CLASS_BYTES 32
#define TRANSITION_TABLE_SIZE 256
#define PATTERN_BUFFER_SIZE 4096

// Security limits to prevent DoS attacks
#define MAX_PATTERN_LENGTH 65536
#define MAX_ALTERNATIONS 1000

/********************************************************************************
 * TYPE DEFINITIONS
 ********************************************************************************/

// Forward declaration for recursive structures
struct vibrex_pattern;

// State types for NFA
typedef enum
{
  STATE_CHAR = 1,     // Match specific character
  STATE_ANY,          // Match any character (.)
  STATE_CLASS,        // Character class [abc]
  STATE_SPLIT,        // Epsilon split (for *, +, ?, |)
  STATE_MATCH,        // Accept state
  STATE_START_ANCHOR, // ^ anchor
  STATE_END_ANCHOR    // $ anchor
} StateType;

// NFA State
typedef struct State
{
  StateType type;
  union
  {
    unsigned char c;                        // Character to match
    unsigned char cclass[CHAR_CLASS_BYTES]; // Character class bitmap
  } data;
  struct State *out;  // Primary transition
  struct State *out1; // Secondary transition (for SPLIT)
  int lastlist;       // Used for tracking active states
} State;

// Pointer list for managing dangling arrows during NFA construction
typedef struct Ptrlist
{
  State **s;
  struct Ptrlist *next;
} Ptrlist;

// Fragment during NFA construction
typedef struct Frag
{
  State *start;
  Ptrlist *out; // List of dangling arrows
} Frag;

// Boyer-Moore bad character table for string search optimization
typedef struct
{
  int skip[TRANSITION_TABLE_SIZE];
  bool enabled;
} BadCharTable;

// Both anchors optimization for patterns like ^prefix.*suffix$
typedef struct
{
  char *prefix;
  char *suffix;
  size_t prefix_len;
  size_t suffix_len;
  bool enabled;
} BothAnchorsOpt;

// URL pattern optimization for patterns like https?://[char-class]+
typedef struct
{
  bool enabled;                  // Whether this optimization is active
  unsigned char char_table[256]; // Lookup table for allowed characters after ://
} UrlPatternOpt;

// Literal alternation optimization for patterns like cat|dog|bird|fish
typedef struct
{
  bool enabled;        // Whether this optimization is active
  char **alternatives; // Array of literal alternatives
  size_t *alt_lengths; // Lengths of each alternative
  size_t alt_count;    // Number of alternatives
} LiteralAltOpt;

// Pattern types for mixed dotstar optimization
typedef enum
{
  ALT_LITERAL,         // ^foo - literal pattern
  ALT_DOTSTAR_PREFIX,  // ^.*foo - dotstar prefix pattern
  ALT_DOTSTAR_SUFFIX,  // ^foo.* - dotstar suffix pattern
  ALT_DOTSTAR_WRAPPER, // ^.*foo.* - dotstar wrapper pattern
  ALT_REGEX            // ^fo+ - complex regex pattern
} AltPatternType;

// A single alternative in the advanced optimization
typedef struct
{
  char *literal_suffix;                // For pure literal alternatives
  struct vibrex_pattern *regex_suffix; // For pre-compiled regex alternatives
  AltPatternType pattern_type;         // Type of this alternative for mixed optimization
  char *core_pattern;                  // Core pattern (without dotstar wrappers)
} AltSuf;

// Advanced alternation optimization structures
typedef struct
{
  char *prefix;                          // Common prefix for all alternations
  size_t prefix_len;                     // Length of common prefix
  char *suffix;                          // Common suffix for all alternations
  size_t suffix_len;                     // Length of common suffix
  struct vibrex_pattern *suffix_pattern; // Compiled suffix pattern if it contains regex metacharacters
  AltSuf *suffixes;                      // Array of alternatives
  size_t alt_count;                      // Number of alternatives

  // Dotstar optimization fields
  bool has_dotstar_prefix;  // All alternatives start with .*
  bool has_dotstar_suffix;  // All alternatives end with .*
  bool has_dotstar_wrapper; // All alternatives are wrapped with .*...*
  bool has_mixed_dotstar;   // Mixed pattern types (some dotstar, some not)
} AlternationOpt;

// DFA state for compiled patterns
typedef struct DFAState
{
  int id;
  bool is_final;                                       // True if this is an accepting state
  struct DFAState *transitions[TRANSITION_TABLE_SIZE]; // Direct character transitions
  bool has_transitions;                                // Whether transitions are populated
} DFAState;

// DFA for fast linear-time matching
typedef struct
{
  DFAState *start_state; // Initial DFA state
  DFAState *states;      // Array of all DFA states
  int num_states;        // Number of DFA states
  int max_states;        // Maximum allocated states
  bool enabled;          // Whether DFA is active
  bool anchored_start;   // Whether pattern is anchored at start
  bool anchored_end;     // Whether pattern is anchored at end
} DFA;

// Complete compiled pattern
struct vibrex_pattern
{
  State *start;
  int nstate;
  State *states;
  bool anchored_end;
  // Performance optimizations
  bool has_dotstar_unanchored; // Pattern is .* without anchors
  State **state_list1;         // Pre-allocated state list 1
  State **state_list2;         // Pre-allocated state list 2

  // Advanced optimizations
  unsigned char first_char; // First required character (if any)
  bool has_first_char;      // Whether first_char is valid
  char *literal_prefix;     // Literal prefix string (if any)
  int prefix_len;           // Length of literal prefix

  // Boyer-Moore bad character table
  BadCharTable bad_char;

  // Both anchors optimization for ^prefix.*suffix$ patterns
  BothAnchorsOpt both_anchors; // Both anchors optimization data

  // URL pattern optimization for https?://[char-class]+ patterns
  UrlPatternOpt url_pattern; // URL pattern optimization data

  // Literal alternation optimization for literal1|literal2|... patterns
  LiteralAltOpt literal_alt; // Literal alternation optimization data

  // DFA optimization for linear-time matching
  DFA dfa; // Compiled DFA for fast matching

  // Advanced alternation optimization
  AlternationOpt alt_opt;    // Advanced alternation optimization data
  bool has_advanced_alt_opt; // Whether advanced alternation optimization is active
};

// Globals for tracking during NFA construction
static int listid = 1;
static State *states;
static int nstate;

// Create a new state
static State *
state (StateType type, State *out, State *out1)
{
  State *s    = &states[nstate++];
  s->type     = type;
  s->out      = out;
  s->out1     = out1;
  s->lastlist = 0;
  return s;
}

static Ptrlist *ptrlist_pool;
static int nptrlist;

// Get a pointer list
static Ptrlist *
list1 (State **outp)
{
  Ptrlist *l = &ptrlist_pool[nptrlist++];
  l->s       = outp;
  l->next    = NULL;
  return l;
}

// Append pointer lists
static Ptrlist *
append (Ptrlist *l1, Ptrlist *l2)
{
  if (l1 == NULL)
    return l2;
  Ptrlist *oldl1 = l1;
  while (l1->next)
    l1 = l1->next;
  l1->next = l2;
  return oldl1;
}

// Patch pointer list to point to state
static void
patch (Ptrlist *l, State *s)
{
  while (l)
  {
    *(l->s) = s;
    l       = l->next;
  }
}

// Parsing context
typedef struct
{
  const char *re;
  int pos;
  int depth;     // Current recursion depth
  int max_depth; // Maximum allowed recursion depth
} ParseContext;

/********************************************************************************
 * FORWARD DECLARATIONS
 ********************************************************************************/

// NFA construction functions
static Frag parsealt (ParseContext *ctx);
static Frag parsecat (ParseContext *ctx);
static Frag parsepiece (ParseContext *ctx);
static Frag parseatom (ParseContext *ctx);

// DFA optimization functions
static bool can_compile_to_dfa (const char *pattern);
static bool compile_literal_to_dfa (struct vibrex_pattern *compiled, const char *pattern);
static bool compile_alternation_to_dfa (struct vibrex_pattern *compiled, const char *pattern);
static DFAState *create_dfa_state (DFA *dfa, bool is_final);
static bool dfa_match (const DFA *dfa, const char *text);
static void free_dfa (DFA *dfa);

// Both anchors optimization functions
static bool can_use_both_anchors_opt (const char *pattern);
static bool compile_both_anchors_opt (struct vibrex_pattern *compiled, const char *pattern);
static bool match_with_both_anchors_opt (const struct vibrex_pattern *pattern, const char *text);
static void free_both_anchors_opt (BothAnchorsOpt *both_anchors);

// URL pattern optimization functions
static bool can_use_url_pattern_opt (const char *pattern);
static bool compile_url_pattern_opt (struct vibrex_pattern *compiled, const char *pattern);
static bool match_with_url_pattern_opt (const struct vibrex_pattern *pattern, const char *text);
static void free_url_pattern_opt (UrlPatternOpt *url_pattern);

// Literal alternation optimization functions
static bool can_use_literal_alt_opt (const char *pattern);
static bool compile_literal_alt_opt (struct vibrex_pattern *compiled, const char *pattern);
static bool match_with_literal_alt_opt (const struct vibrex_pattern *pattern, const char *text);
static void free_literal_alt_opt (LiteralAltOpt *literal_alt);

// Advanced alternation optimization functions
static bool can_use_advanced_alternation_opt (const char *pattern);
static bool compile_advanced_alternation_opt (struct vibrex_pattern *compiled, const char *pattern);
static bool match_with_advanced_alternation_opt (const struct vibrex_pattern *pattern, const char *text);
static void free_alternation_opt (AlternationOpt *alt_opt);

// Alternation optimization helper functions
static AltPatternType classify_alternative_pattern (const char *alt, size_t len);
static bool parse_alternatives (const char *pattern, const char ***alternatives, size_t **alt_lengths, size_t *alt_count);
static bool check_dotstar_consistency (const char **alternatives, size_t *alt_lengths, size_t alt_count, AlternationOpt *alt_opt);
static bool extract_core_pattern (const char *alt, size_t alt_len, AltPatternType pattern_type, char **core_pattern, size_t *core_len);
static bool compile_dotstar_optimization (AlternationOpt *alt_opt, const char **alternatives, size_t *alt_lengths);
static bool find_common_prefix_suffix (const char *pattern, const char **alternatives, size_t *alt_lengths, size_t alt_count, AlternationOpt *alt_opt);
static bool compile_suffix_pattern (AlternationOpt *alt_opt);
static bool compile_middle_parts (AlternationOpt *alt_opt, const char **alternatives, size_t *alt_lengths);

// Alternation matching helper functions
static bool match_single_alternative (const AltSuf *alt_suffix, const char *text, size_t text_len);
static bool match_dotstar_patterns (const AlternationOpt *alt_opt, const char *text, size_t text_len);
static bool match_suffix_pattern (const AlternationOpt *alt_opt, const char *text, size_t text_len, const char **match_end);
static bool match_alternatives (const AlternationOpt *alt_opt, const char *middle_text, size_t middle_len);

// Utility functions
static const char *boyer_moore_search (const char *text, int text_len, const struct vibrex_pattern *pattern);

/********************************************************************************
 * NFA CONSTRUCTION FUNCTIONS
 ********************************************************************************/

// Parse alternation (|)
static Frag
parsealt (ParseContext *ctx)
{
  // Check recursion depth
  if (ctx->depth >= ctx->max_depth)
  {
    return (Frag){NULL, NULL};
  }
  ctx->depth++;

  Frag e1 = parsecat (ctx);
  if (ctx->re[ctx->pos] != '|')
  {
    ctx->depth--;
    return e1;
  }

  ctx->pos++; // skip '|'
  Frag e2 = parsealt (ctx);

  if (!e1.start && !e2.start)
  {
    State *s = state (STATE_MATCH, NULL, NULL);
    ctx->depth--;
    return (Frag){s, NULL};
  }
  else if (!e1.start)
  {
    State *match = state (STATE_MATCH, NULL, NULL);
    State *s     = state (STATE_SPLIT, match, e2.start);
    ctx->depth--;
    return (Frag){s, e2.out};
  }
  else if (!e2.start)
  {
    State *match = state (STATE_MATCH, NULL, NULL);
    State *s     = state (STATE_SPLIT, e1.start, match);
    ctx->depth--;
    return (Frag){s, e1.out};
  }

  State *s = state (STATE_SPLIT, e1.start, e2.start);
  ctx->depth--;
  return (Frag){s, append (e1.out, e2.out)};
}

// Parse concatenation
static Frag
parsecat (ParseContext *ctx)
{
  if (ctx->depth >= ctx->max_depth)
  {
    return (Frag){NULL, NULL};
  }
  ctx->depth++;

  if (!ctx->re[ctx->pos] || ctx->re[ctx->pos] == ')' || ctx->re[ctx->pos] == '|')
  {
    State *s = state (STATE_SPLIT, NULL, NULL);
    ctx->depth--;
    return (Frag){s, list1 (&s->out)};
  }

  Frag e1 = parsepiece (ctx);

  if (!e1.start)
  {
    ctx->depth--;
    return e1;
  }

  while (ctx->re[ctx->pos] && ctx->re[ctx->pos] != ')' && ctx->re[ctx->pos] != '|')
  {
    Frag e2 = parsepiece (ctx);

    if (!e2.start)
    {
      ctx->depth--;
      return e2;
    }

    patch (e1.out, e2.start);
    e1.out = e2.out;
  }
  ctx->depth--;
  return e1;
}

// Parse piece (atom with quantifier)
static Frag
parsepiece (ParseContext *ctx)
{
  Frag e = parseatom (ctx);

  if (!e.start)
  {
    return e;
  }

  char op = ctx->re[ctx->pos];
  if (op != '*' && op != '+' && op != '?')
    return e;

  ctx->pos++;

  State *s;
  switch (op)
  {
  case '*':
    s = state (STATE_SPLIT, e.start, NULL);
    patch (e.out, s);
    return (Frag){s, list1 (&s->out1)};

  case '+':
    s = state (STATE_SPLIT, e.start, NULL);
    patch (e.out, s);
    return (Frag){e.start, list1 (&s->out1)};

  case '?':
    s = state (STATE_SPLIT, e.start, NULL);
    return (Frag){s, append (e.out, list1 (&s->out1))};
  }
  return e;
}

// Parse atom
static Frag
parseatom (ParseContext *ctx)
{
  char c = ctx->re[ctx->pos];

  if (c == '.')
  {
    ctx->pos++;
    State *s = state (STATE_ANY, NULL, NULL);
    return (Frag){s, list1 (&s->out)};
  }

  if (c == '^')
  {
    ctx->pos++;
    State *s = state (STATE_START_ANCHOR, NULL, NULL);
    return (Frag){s, list1 (&s->out)};
  }

  if (c == '$')
  {
    ctx->pos++;
    State *s = state (STATE_END_ANCHOR, NULL, NULL);
    return (Frag){s, list1 (&s->out)};
  }

  if (c == '(')
  {
    if (ctx->depth >= ctx->max_depth)
    {
      return (Frag){NULL, NULL};
    }
    ctx->depth++;

    ctx->pos++;

    Frag e = parsealt (ctx);

    if (ctx->re[ctx->pos] != ')')
    {
      ctx->depth--;
      return (Frag){NULL, NULL};
    }
    ctx->pos++;
    ctx->depth--;
    return e;
  }

  if (c == '[')
  {
    ctx->pos++;
    State *s = state (STATE_CLASS, NULL, NULL);
    memset (s->data.cclass, 0, CHAR_CLASS_BYTES);

    bool negated = false;
    if (ctx->re[ctx->pos] == '^')
    {
      negated = true;
      ctx->pos++;
    }

    if (ctx->re[ctx->pos] == ']')
    {
      return (Frag){NULL, NULL};
    }

    while (ctx->re[ctx->pos] && ctx->re[ctx->pos] != ']')
    {
      unsigned char start = ctx->re[ctx->pos++];
      unsigned char end   = start;

      if (ctx->re[ctx->pos] == '-' && ctx->re[ctx->pos + 1] && ctx->re[ctx->pos + 1] != ']')
      {
        ctx->pos++;
        end = ctx->re[ctx->pos++];

        if (end < start)
        {
          return (Frag){NULL, NULL};
        }
      }

      for (unsigned char ch = start; ch <= end; ch++)
      {
        // Security check: Explicit bounds verification for character class array
        unsigned int byte_idx = ch / 8;
        if (byte_idx >= CHAR_CLASS_BYTES)
        {
          return (Frag){NULL, NULL};
        }
        s->data.cclass[byte_idx] |= 1 << (ch % 8);

        // Prevent infinite loop when end is 255 (0xFF)
        if (ch == 255)
          break;
      }
    }

    if (ctx->re[ctx->pos] != ']')
    {
      return (Frag){NULL, NULL};
    }
    ctx->pos++;

    if (negated)
    {
      for (int i = 0; i < CHAR_CLASS_BYTES; i++)
      {
        s->data.cclass[i] = ~s->data.cclass[i];
      }
    }

    return (Frag){s, list1 (&s->out)};
  }

  if (c == '\\')
  {
    if (!ctx->re[ctx->pos + 1])
    {
      return (Frag){NULL, NULL};
    }
    ctx->pos++;
    c         = ctx->re[ctx->pos++];
    State *s  = state (STATE_CHAR, NULL, NULL);
    s->data.c = c;
    return (Frag){s, list1 (&s->out)};
  }

  if (c == ')')
  {
    return (Frag){NULL, NULL};
  }

  if (c && c != '*' && c != '+' && c != '?' && c != '|' && c != ')')
  {
    ctx->pos++;
    State *s  = state (STATE_CHAR, NULL, NULL);
    s->data.c = c;
    return (Frag){s, list1 (&s->out)};
  }

  return (Frag){NULL, NULL};
}

/********************************************************************************
 * PUBLIC API FUNCTIONS
 ********************************************************************************/

// Compile regex to NFA
struct vibrex_pattern *
vibrex_compile (const char *pattern, const char **error_message)
{
  if (!pattern)
  {
    if (error_message)
      *error_message = "NULL pattern";
    return NULL;
  }

  // Security check: Prevent DoS via extremely long patterns
  size_t pattern_len = strlen (pattern);
  if (pattern_len > MAX_PATTERN_LENGTH)
  {
    if (error_message)
      *error_message = "Pattern too long (exceeds security limit)";
    return NULL;
  }

  struct vibrex_pattern *compiled = calloc (1, sizeof (struct vibrex_pattern));
  if (!compiled)
  {
    if (error_message)
      *error_message = "Out of memory";
    return NULL;
  }

  // Try both anchors optimization first (^prefix.*suffix$)
  if (compile_both_anchors_opt (compiled, pattern))
  {
    if (error_message)
      *error_message = NULL;
    return compiled;
  }

  // Try URL pattern optimization (https?://[char-class]+)
  if (compile_url_pattern_opt (compiled, pattern))
  {
    if (error_message)
      *error_message = NULL;
    return compiled;
  }

  // Try literal alternation optimization (literal1|literal2|...)
  if (compile_literal_alt_opt (compiled, pattern))
  {
    if (error_message)
      *error_message = NULL;
    return compiled;
  }

  if (compile_advanced_alternation_opt (compiled, pattern))
  {
    if (error_message)
      *error_message = NULL;
    return compiled;
  }

  if (can_compile_to_dfa (pattern))
  {
    bool dfa_compiled      = false;
    bool has_top_level_alt = (strchr (pattern, '|') != NULL);

    if (has_top_level_alt)
    {
      dfa_compiled = compile_alternation_to_dfa (compiled, pattern);
    }
    else
    {
      dfa_compiled = compile_literal_to_dfa (compiled, pattern);
    }

    if (dfa_compiled)
    {
      if (error_message)
        *error_message = NULL;
      return compiled;
    }
  }

  states       = malloc (MAX_NFA_STATES * sizeof (State));
  ptrlist_pool = malloc (MAX_PTRLIST_ENTRIES * sizeof (Ptrlist));
  if (!states || !ptrlist_pool)
  {
    free (states);
    free (ptrlist_pool);
    vibrex_free (compiled);
    if (error_message)
      *error_message = "Out of memory";
    return NULL;
  }

  nstate   = 0;
  nptrlist = 0;

  ParseContext ctx = {pattern, 0, 0, MAX_RECURSION_DEPTH};
  Frag e           = parsealt (&ctx);
  if (!e.start)
  {
    free (states);
    free (ptrlist_pool);
    vibrex_free (compiled);
    if (error_message)
      *error_message = "Parse error: Invalid pattern structure";
    return NULL;
  }

  if (ctx.pos < (int)strlen (pattern))
  {
    free (states);
    free (ptrlist_pool);
    vibrex_free (compiled);
    if (error_message)
      *error_message = "Parse error: Unexpected characters at end of pattern";
    return NULL;
  }

  State *match = state (STATE_MATCH, NULL, NULL);
  patch (e.out, match);

  compiled->start  = e.start;
  compiled->nstate = nstate;
  compiled->states = states;

  int pat_len = strlen (pattern);
  if (pat_len > 0 && pattern[pat_len - 1] == '$')
  {
    if (pat_len == 1 || pattern[pat_len - 2] != '\\')
    {
      compiled->anchored_end = true;
    }
  }

  compiled->state_list1 = malloc (nstate * sizeof (State *));
  compiled->state_list2 = malloc (nstate * sizeof (State *));
  if (!compiled->state_list1 || !compiled->state_list2)
  {
    vibrex_free (compiled);
    free (ptrlist_pool);
    if (error_message)
      *error_message = "Out of memory";
    return NULL;
  }

  compiled->has_dotstar_unanchored = (strcmp (pattern, ".*") == 0);

  // Check for top-level alternations, which make simple prefix analysis unsafe
  bool has_top_level_alt = false;
  if (!compiled->dfa.enabled && !compiled->has_advanced_alt_opt)
  {
    int paren_depth       = 0;
    const char *alt_check = pattern;
    while (*alt_check)
    {
      if (*alt_check == '(')
        paren_depth++;
      else if (*alt_check == ')')
        paren_depth--;
      else if (*alt_check == '|' && paren_depth == 0)
      {
        has_top_level_alt = true;
        break;
      }
      alt_check++;
    }
  }

  if (!has_top_level_alt && !compiled->dfa.enabled && !compiled->has_advanced_alt_opt)
  {
    const char *p          = pattern;
    bool is_start_anchored = (*p == '^');
    if (is_start_anchored)
      p++;

    char prefix_buf[256];
    int prefix_idx            = 0;
    const char *end_of_prefix = p;

    while (*end_of_prefix && prefix_idx < 255)
    {
      if (*end_of_prefix == '\\' && *(end_of_prefix + 1))
      {
        if (strchr (".?*+[]()|^$", *(end_of_prefix + 1)))
          break;
        prefix_buf[prefix_idx++] = *(end_of_prefix + 1);
        end_of_prefix += 2;
      }
      else if (strchr (".?*+[]()|^$", *end_of_prefix))
      {
        break;
      }
      else
      {
        prefix_buf[prefix_idx++] = *end_of_prefix++;
      }
    }

    if (prefix_idx > 0 && (*end_of_prefix == '*' || *end_of_prefix == '?'))
    {
      prefix_idx = 0;
    }

    if (prefix_idx > 0)
    {
      compiled->has_first_char = true;
      compiled->first_char     = (unsigned char)prefix_buf[0];

      if (!is_start_anchored && prefix_idx >= 3 && !compiled->anchored_end)
      {
        compiled->literal_prefix = malloc (prefix_idx + 1);
        if (compiled->literal_prefix)
        {
          memcpy (compiled->literal_prefix, prefix_buf, prefix_idx);
          compiled->literal_prefix[prefix_idx] = '\0';
          compiled->prefix_len                 = prefix_idx;

          for (int i = 0; i < TRANSITION_TABLE_SIZE; i++)
          {
            compiled->bad_char.skip[i] = prefix_idx;
          }
          for (int i = 0; i < prefix_idx - 1; i++)
          {
            compiled->bad_char.skip[(unsigned char)prefix_buf[i]] = prefix_idx - 1 - i;
          }
          compiled->bad_char.enabled = true;
        }
      }
    }
  }

  free (ptrlist_pool);
  if (error_message)
    *error_message = NULL;
  return compiled;
}

/********************************************************************************
 * NFA MATCHING ENGINE
 ********************************************************************************/

// List of states for simulation
typedef struct List
{
  State **s;
  int n;
} List;

// Global state lists for matching (reused across calls)
static List l1, l2;

// Add state to list with epsilon closure (position-aware)
static void
addstate_pos (List *l, State *s, int pos)
{
  if (s == NULL || s->lastlist == listid)
    return;
  s->lastlist = listid;

  if (s->type == STATE_SPLIT)
  {
    addstate_pos (l, s->out, pos);
    addstate_pos (l, s->out1, pos);
    return;
  }

  if (s->type == STATE_START_ANCHOR)
  {
    if (pos == 0)
    {
      addstate_pos (l, s->out, pos);
    }
    return;
  }

  l->s[l->n++] = s;
}

// Step simulation with one character
static void
step (List *clist, unsigned char c, List *nlist)
{
  listid++;
  nlist->n = 0;

  for (int i = 0; i < clist->n; i++)
  {
    State *s = clist->s[i];

    switch (s->type)
    {
    case STATE_CHAR:
      if (s->data.c == c)
        addstate_pos (nlist, s->out, -1);
      break;

    case STATE_ANY:
      addstate_pos (nlist, s->out, -1);
      break;

    case STATE_CLASS:
      if (s->data.cclass[c / 8] & (1 << (c % 8)))
        addstate_pos (nlist, s->out, -1);
      break;



    default:
      break;
    }
  }
}

// Check if list contains match state
static bool
ismatch (List *l)
{
  for (int i = 0; i < l->n; i++)
  {
    if (l->s[i]->type == STATE_MATCH)
      return true;
  }
  return false;
}

static bool
is_end_match (List *l, bool at_end_of_text)
{
  for (int i = 0; i < l->n; i++)
  {
    State *s = l->s[i];
    if (s->type == STATE_MATCH)
      return true;

    if (s->type == STATE_END_ANCHOR && at_end_of_text)
    {
      List temp_list;
      State *temp_states[16];
      temp_list.s = temp_states;
      temp_list.n = 0;
      listid++;
      addstate_pos (&temp_list, s->out, -1);
      if (ismatch (&temp_list))
        return true;
    }
  }
  return false;
}

// Match text against compiled pattern
bool
vibrex_match (const struct vibrex_pattern *pattern, const char *text)
{
  if (!pattern || !text)
    return false;

  // Check both anchors optimization first (fastest)
  if (pattern->both_anchors.enabled)
  {
    return match_with_both_anchors_opt (pattern, text);
  }

  // Check URL pattern optimization
  if (pattern->url_pattern.enabled)
  {
    return match_with_url_pattern_opt (pattern, text);
  }

  // Check literal alternation optimization
  if (pattern->literal_alt.enabled)
  {
    return match_with_literal_alt_opt (pattern, text);
  }

  if (pattern->has_advanced_alt_opt)
  {
    return match_with_advanced_alternation_opt (pattern, text);
  }
  if (pattern->dfa.enabled)
  {
    return dfa_match (&pattern->dfa, text);
  }
  if (pattern->has_dotstar_unanchored)
  {
    return true;
  }

  l1.s                    = pattern->state_list1;
  l2.s                    = pattern->state_list2;
  int textlen             = strlen (text);
  const char *text_end    = text + textlen;
  const char *current_pos = text;
  bool is_start_anchored  = (pattern->start && pattern->start->type == STATE_START_ANCHOR);

  if (pattern->bad_char.enabled)
  {
    while ((current_pos = boyer_moore_search (current_pos, text_end - current_pos, pattern)) != NULL)
    {
      listid++;
      l1.n = 0;
      addstate_pos (&l1, pattern->start, current_pos - text);

      List *clist = &l1, *nlist = &l2, *tmp;

      // Check for match after initialization
      if (!pattern->anchored_end && ismatch (clist))
        return true;

      // Simulate consuming the prefix that Boyer-Moore already matched
      for (int i = 0; i < pattern->prefix_len; i++)
      {
        step (clist, current_pos[i], nlist);
        tmp   = clist;
        clist = nlist;
        nlist = tmp;
        if (clist->n == 0)
          break;
        // Check for match after each character in prefix
        if (!pattern->anchored_end && ismatch (clist))
          return true;
      }

      // Now continue with the rest of the pattern
      for (const char *p = current_pos + pattern->prefix_len; p < text_end; ++p)
      {
        step (clist, *p, nlist);
        tmp   = clist;
        clist = nlist;
        nlist = tmp;
        if (clist->n == 0)
          break;
        // Check for match after each character
        if (!pattern->anchored_end && ismatch (clist))
          return true;
      }
      if (is_end_match (clist, true))
        return true;
      current_pos++;
    }
    return false;
  }

  if (!is_start_anchored && pattern->has_first_char)
  {
    while ((current_pos = strchr (current_pos, pattern->first_char)) != NULL)
    {
      listid++;
      l1.n = 0;
      addstate_pos (&l1, pattern->start, current_pos - text);
      List *clist = &l1, *nlist = &l2, *tmp;
      if (!pattern->anchored_end && ismatch (clist))
        return true;

      for (const char *p = current_pos; p < text_end; ++p)
      {
        step (clist, *p, nlist);
        tmp   = clist;
        clist = nlist;
        nlist = tmp;
        if (clist->n == 0)
          break;
        if (!pattern->anchored_end && ismatch (clist))
          return true;
      }
      if (is_end_match (clist, true))
        return true;
      current_pos++;
    }
    return false;
  }

  int max_start_pos = is_start_anchored ? 0 : textlen;
  for (int i = 0; i <= max_start_pos; i++)
  {
    listid++;
    l1.n = 0;
    addstate_pos (&l1, pattern->start, i);

    List *clist = &l1, *nlist = &l2, *tmp;

    if (is_end_match (clist, i == textlen))
    {
      if (!pattern->anchored_end || i == textlen)
      {
        return true;
      }
    }

    for (int j = i; j < textlen; j++)
    {
      step (clist, text[j], nlist);
      tmp   = clist;
      clist = nlist;
      nlist = tmp;
      if (clist->n == 0)
        break;

      if (!pattern->anchored_end && ismatch (clist))
      {
        return true;
      }
    }

    // Check for end match only if we've consumed all characters
    if (is_end_match (clist, true))
    {
      return true;
    }

    if (is_start_anchored)
      break;
  }

  return false;
}

// Free compiled pattern
void
vibrex_free (struct vibrex_pattern *pattern)
{
  if (pattern)
  {
    free (pattern->states);
    free (pattern->state_list1);
    free (pattern->state_list2);
    free (pattern->literal_prefix);

    free_both_anchors_opt (&pattern->both_anchors);
    free_url_pattern_opt (&pattern->url_pattern);
    free_literal_alt_opt (&pattern->literal_alt);
    free_dfa (&pattern->dfa);

    if (pattern->has_advanced_alt_opt)
    {
      free_alternation_opt (&pattern->alt_opt);
    }

    free (pattern);
  }
}

/********************************************************************************
 * DFA OPTIMIZATION ENGINE
 ********************************************************************************/

static bool
can_compile_to_dfa (const char *pattern)
{
  if (!pattern)
    return false;

  const char *p = pattern;
  if (*p == '^')
    p++;

  int paren_depth = 0;
  while (*p)
  {
    if (*p == '$' && *(p + 1) == '\0')
      break;

    char c = *p;
    if (c == '\\')
    {
      p++;
      if (!*p)
        return false;
    }
    else if (strchr ("*+?.[()", c))
    {
      return false;
    }
    else if (c == '|')
    {
      if (paren_depth != 0)
        return false;
    }
    p++;
  }

  return paren_depth == 0;
}

static DFAState *
create_dfa_state (DFA *dfa, bool is_final)
{
  if (dfa->num_states >= dfa->max_states)
  {
    int new_max          = dfa->max_states > 0 ? dfa->max_states * 2 : 16;
    DFAState *new_states = realloc (dfa->states, new_max * sizeof (DFAState));
    if (!new_states)
      return NULL;
    dfa->states     = new_states;
    dfa->max_states = new_max;
  }

  DFAState *state = &dfa->states[dfa->num_states];
  state->id       = dfa->num_states;
  dfa->num_states++;

  state->is_final        = is_final;
  state->has_transitions = false;
  memset (state->transitions, 0, sizeof (state->transitions));

  return state;
}

static bool
compile_literal_to_dfa (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!pattern)
    return false;

  compiled->dfa.max_states = strlen (pattern) + 2;
  compiled->dfa.states     = calloc (compiled->dfa.max_states, sizeof (DFAState));
  if (!compiled->dfa.states)
    return false;
  compiled->dfa.num_states = 0;

  const char *p                = pattern;
  compiled->dfa.anchored_start = (*p == '^');
  if (compiled->dfa.anchored_start)
    p++;

  int len                    = strlen (p);
  compiled->dfa.anchored_end = (len > 0 && p[len - 1] == '$' && (len == 1 || p[len - 2] != '\\'));
  if (compiled->dfa.anchored_end)
    len--;

  if (len < 0)
    len = 0;

  DFAState *current = create_dfa_state (&compiled->dfa, len == 0);
  if (!current)
  {
    free_dfa (&compiled->dfa);
    return false;
  }
  compiled->dfa.start_state = current;

  for (int i = 0; i < len; i++)
  {
    unsigned char c = (unsigned char)p[i];
    if (c == '\\' && i + 1 < len)
    {
      i++;
      c = (unsigned char)p[i];
    }

    DFAState *next = create_dfa_state (&compiled->dfa, i == len - 1);
    if (!next)
    {
      free_dfa (&compiled->dfa);
      return false;
    }

    current->transitions[c]  = next;
    current->has_transitions = true;
    current                  = next;
  }

  compiled->dfa.enabled = true;
  return true;
}

static bool
compile_alternation_to_dfa (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!pattern)
    return false;

  compiled->dfa.max_states = 256;
  compiled->dfa.states     = calloc (compiled->dfa.max_states, sizeof (DFAState));
  if (!compiled->dfa.states)
    return false;
  compiled->dfa.num_states = 0;

  const char *p                = pattern;
  compiled->dfa.anchored_start = (*p == '^');
  if (compiled->dfa.anchored_start)
    p++;

  int pat_len                = strlen (p);
  compiled->dfa.anchored_end = (pat_len > 0 && p[pat_len - 1] == '$' && (pat_len == 1 || p[pat_len - 2] != '\\'));
  if (compiled->dfa.anchored_end)
    pat_len--;

  DFAState *start_state = create_dfa_state (&compiled->dfa, false);
  if (!start_state)
  {
    free_dfa (&compiled->dfa);
    return false;
  }
  compiled->dfa.start_state = start_state;

  // Security check: Count alternations to prevent DoS
  int alt_count       = 0;
  const char *count_p = p;
  while ((count_p = strchr (count_p, '|')) != NULL && count_p < p + pat_len)
  {
    alt_count++;
    count_p++;
    if (alt_count > MAX_ALTERNATIONS)
    {
      free_dfa (&compiled->dfa);
      return false;
    }
  }

  const char *alt_start = p;
  while (true)
  {
    const char *alt_end = strchr (alt_start, '|');
    if (alt_end == NULL || alt_end > p + pat_len)
      alt_end = p + pat_len;

    DFAState *current_state = start_state;
    int alt_len             = alt_end - alt_start;

    if (alt_len == 0)
    {
      start_state->is_final = true;
    }
    else
    {
      for (int i = 0; i < alt_len; ++i)
      {
        unsigned char c = alt_start[i];
        if (c == '\\' && i + 1 < alt_len)
        {
          i++;
          c = alt_start[i];
        }

        if (!current_state->transitions[c])
        {
          bool is_final        = (i == alt_len - 1);
          DFAState *next_state = create_dfa_state (&compiled->dfa, is_final);
          if (!next_state)
          {
            free_dfa (&compiled->dfa);
            return false;
          }
          current_state->transitions[c]  = next_state;
          current_state->has_transitions = true;
        }
        else if (i == alt_len - 1)
        {
          current_state->transitions[c]->is_final = true;
        }
        current_state = current_state->transitions[c];
      }
    }

    if (alt_end < p + pat_len && *alt_end == '|')
    {
      alt_start = alt_end + 1;
    }
    else
    {
      break;
    }
  }

  compiled->dfa.enabled = true;
  return true;
}

static bool
dfa_match (const DFA *dfa, const char *text)
{
  if (!dfa->enabled || !text)
    return false;
  int text_len = strlen (text);

  if (dfa->anchored_start)
  {
    DFAState *current = dfa->start_state;
    if (current->is_final && !dfa->anchored_end)
      return true;
    for (int i = 0; i < text_len; i++)
    {
      current = current->transitions[(unsigned char)text[i]];
      if (!current)
      {
        return false;
      }
      if (current->is_final && !dfa->anchored_end)
      {
        return true;
      }
    }
    return current->is_final;
  }
  else
  {
    for (int i = 0; i <= text_len; i++)
    {
      DFAState *current = dfa->start_state;
      if (current->is_final && (!dfa->anchored_end || i == text_len))
      {
        return true;
      }
      for (int j = i; j < text_len; j++)
      {
        current = current->transitions[(unsigned char)text[j]];
        if (!current)
          break;
        if (current->is_final)
        {
          if (!dfa->anchored_end || (j == text_len - 1))
          {
            return true;
          }
        }
      }
    }
  }

  return false;
}

static void
free_dfa (DFA *dfa)
{
  if (dfa && dfa->states)
  {
    free (dfa->states);
    dfa->states     = NULL;
    dfa->num_states = 0;
    dfa->enabled    = false;
  }
}

/********************************************************************************
 * BOTH ANCHORS OPTIMIZATION ENGINE
 ********************************************************************************/

// Check if pattern is of the form ^prefix.*suffix$
static bool
can_use_both_anchors_opt (const char *pattern)
{
  if (!pattern)
    return false;

  size_t len = strlen (pattern);

  // Must start with ^ and end with $
  if (len < 3 || pattern[0] != '^' || pattern[len - 1] != '$')
    return false;

  // Find .* in the middle
  const char *dotstar = strstr (pattern + 1, ".*");
  if (!dotstar)
    return false;

  // Check that .* appears only once and is followed by suffix then $
  const char *second_dotstar = strstr (dotstar + 2, ".*");
  if (second_dotstar)
    return false;

  // Must have some prefix before .* and some suffix after .*
  size_t prefix_len = dotstar - (pattern + 1);
  size_t suffix_len = (pattern + len - 1) - (dotstar + 2);

  // Both prefix and suffix must be non-empty for meaningful optimization
  if (prefix_len == 0 || suffix_len == 0)
    return false;

  // Check that prefix and suffix contain only literal characters (no regex metacharacters)
  for (size_t i = 1; i <= prefix_len; i++)
  {
    if (strchr (".?*+[]()|\\", pattern[i]))
      return false;
  }

  for (size_t i = dotstar + 2 - pattern; i < len - 1; i++)
  {
    if (strchr (".?*+[]()|\\", pattern[i]))
      return false;
  }

  return true;
}

// Compile both anchors optimization
static bool
compile_both_anchors_opt (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!can_use_both_anchors_opt (pattern))
    return false;

  size_t len          = strlen (pattern);
  const char *dotstar = strstr (pattern + 1, ".*");

  size_t prefix_len = dotstar - (pattern + 1);
  size_t suffix_len = (pattern + len - 1) - (dotstar + 2);

  // Allocate and copy prefix
  compiled->both_anchors.prefix = malloc (prefix_len + 1);
  if (!compiled->both_anchors.prefix)
    return false;

  memcpy (compiled->both_anchors.prefix, pattern + 1, prefix_len);
  compiled->both_anchors.prefix[prefix_len] = '\0';
  compiled->both_anchors.prefix_len         = prefix_len;

  // Allocate and copy suffix
  compiled->both_anchors.suffix = malloc (suffix_len + 1);
  if (!compiled->both_anchors.suffix)
  {
    free (compiled->both_anchors.prefix);
    return false;
  }

  memcpy (compiled->both_anchors.suffix, dotstar + 2, suffix_len);
  compiled->both_anchors.suffix[suffix_len] = '\0';
  compiled->both_anchors.suffix_len         = suffix_len;

  compiled->both_anchors.enabled = true;
  return true;
}

// Match using both anchors optimization - ultra-simple approach!
static bool
match_with_both_anchors_opt (const struct vibrex_pattern *pattern, const char *text)
{
  if (!pattern->both_anchors.enabled || !text)
    return false;

  const BothAnchorsOpt *opt = &pattern->both_anchors;

  // Check prefix at start first (this is very fast)
  if (strncmp (text, opt->prefix, opt->prefix_len) != 0)
    return false;

  // For suffix check: find the end once and check suffix directly
  if (opt->suffix_len == 0)
    return true;

  // Find the end of the string (starting from after prefix)
  const char *p = text + opt->prefix_len;
  while (*p)
    p++;

  // Check if string is long enough to contain both prefix and suffix
  size_t total_len = p - text;
  if (total_len < opt->prefix_len + opt->suffix_len)
    return false;

  // Check if the string ends with our suffix
  const char *suffix_start = p - opt->suffix_len;
  return (strncmp (suffix_start, opt->suffix, opt->suffix_len) == 0);
}

// Free both anchors optimization data
static void
free_both_anchors_opt (BothAnchorsOpt *both_anchors)
{
  if (both_anchors)
  {
    free (both_anchors->prefix);
    free (both_anchors->suffix);
    both_anchors->prefix  = NULL;
    both_anchors->suffix  = NULL;
    both_anchors->enabled = false;
  }
}

/********************************************************************************
 * URL PATTERN OPTIMIZATION ENGINE
 ********************************************************************************/

// Check if pattern is of the form https?://[char-class]+
static bool
can_use_url_pattern_opt (const char *pattern)
{
  if (!pattern)
    return false;

  // Must start with "http"
  if (strncmp (pattern, "http", 4) != 0)
    return false;

  const char *p = pattern + 4;

  // Check for optional 's'
  if (*p == 's')
  {
    // Must be followed by '?'
    if (*(p + 1) != '?')
      return false;
    p += 2;
  }

  // Must be followed by "://"
  if (strncmp (p, "://", 3) != 0)
    return false;
  p += 3;

  // Must be followed by character class
  if (*p != '[')
    return false;
  p++;

  // Skip to end of character class
  while (*p && *p != ']')
    p++;

  if (*p != ']')
    return false;
  p++;

  // Must be followed by '+'
  if (*p != '+')
    return false;
  p++;

  // Must be end of pattern
  return (*p == '\0');
}

// Compile URL pattern optimization
static bool
compile_url_pattern_opt (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!can_use_url_pattern_opt (pattern))
    return false;

  // Initialize character lookup table to all false
  memset (compiled->url_pattern.char_table, 0, sizeof (compiled->url_pattern.char_table));

  // Find the character class
  const char *class_start = strchr (pattern, '[');
  if (!class_start)
    return false;
  class_start++; // skip '['

  const char *class_end = strchr (class_start, ']');
  if (!class_end)
    return false;

  // Parse character class and populate lookup table
  const char *p = class_start;
  while (p < class_end)
  {
    unsigned char start = *p++;
    unsigned char end   = start;

    // Check for range (e.g., a-z)
    if (p < class_end - 1 && *p == '-')
    {
      p++; // skip '-'
      end = *p++;
      if (end < start)
        return false;
    }

    // Set bits for this range
    for (unsigned char ch = start; ch <= end; ch++)
    {
      compiled->url_pattern.char_table[ch] = 1;
      if (ch == 255)
        break; // Prevent overflow
    }
  }

  compiled->url_pattern.enabled = true;
  return true;
}

// Match using URL pattern optimization - very fast!
static bool
match_with_url_pattern_opt (const struct vibrex_pattern *pattern, const char *text)
{
  if (!pattern->url_pattern.enabled || !text)
    return false;

  const char *p                   = text;
  const unsigned char *char_table = pattern->url_pattern.char_table;

  // Search for "http" in the text
  while ((p = strstr (p, "http")) != NULL)
  {
    const char *url_start = p;
    p += 4; // skip "http"

    // Check for optional 's'
    if (*p == 's')
      p++;

    // Must be followed by "://"
    if (strncmp (p, "://", 3) != 0)
    {
      p = url_start + 1; // Try next position
      continue;
    }
    p += 3; // skip "://"

    // Must have at least one character from the allowed class
    if (*p == '\0' || !char_table[(unsigned char)*p])
    {
      p = url_start + 1; // Try next position
      continue;
    }

    // Scan characters from the allowed class
    while (*p && char_table[(unsigned char)*p])
      p++;

    // Found a match!
    return true;
  }

  return false;
}

// Free URL pattern optimization data
static void
free_url_pattern_opt (UrlPatternOpt *url_pattern)
{
  if (url_pattern)
  {
    url_pattern->enabled = false;
  }
}

/********************************************************************************
 * LITERAL ALTERNATION OPTIMIZATION ENGINE
 ********************************************************************************/

// Check if pattern is pure literal alternation like cat|dog|bird|fish, (cat|dog), or (cat|dog)|(bird|fish)
// Must be ONLY alternations of literals with no additional regex structure
static bool
can_use_literal_alt_opt (const char *pattern)
{
  if (!pattern)
    return false;

  const char *p           = pattern;
  bool has_alternation    = false;
  int paren_depth         = 0;
  bool in_top_level_group = false;

  // Check if the pattern is wrapped in a single outer group like (a|b)
  if (*p == '(' && pattern[strlen (pattern) - 1] == ')')
  {
    // Check if this is a complete wrapper - no additional content outside
    const char *check_p = p + 1;
    int check_depth     = 0;

    while (*check_p && check_p < pattern + strlen (pattern) - 1)
    {
      if (*check_p == '(')
        check_depth++;
      else if (*check_p == ')')
      {
        check_depth--;
        if (check_depth < 0)
          break; // Found the closing paren of outer group
      }
      check_p++;
    }

    // If we reached the end-1 position with balanced parens, it's a wrapper
    if (check_p == pattern + strlen (pattern) - 1 && check_depth == 0)
      in_top_level_group = true;
  }

  // The pattern should be one of these forms:
  // 1. a|b|c (simple alternation)
  // 2. (a|b) (grouped alternation)
  // 3. (a|b)|(c|d) (multiple grouped alternations)

  // Scan through the pattern to validate structure
  while (*p)
  {
    char c = *p;

    if (c == '(')
    {
      paren_depth++;
    }
    else if (c == ')')
    {
      paren_depth--;
      if (paren_depth < 0)
        return false; // Unbalanced parentheses

      // After closing a group, we should only see | or end of pattern
      if (*(p + 1) != '\0' && *(p + 1) != '|')
      {
        if (!in_top_level_group || paren_depth > 0)
          return false; // Additional content after group
      }
    }
    else if (c == '|')
    {
      has_alternation = true;
    }
    else if (c == '\\' && *(p + 1))
    {
      // Skip escaped character
      p++;
    }
    else if (strchr (".?*+[]^$", c))
    {
      // Contains regex metacharacters
      return false;
    }
    // Regular literal characters are allowed

    p++;
  }

  // Must have balanced parentheses and at least one alternation
  return has_alternation && paren_depth == 0;
}

// Parse literal alternatives from pattern - handles patterns like (cat|dog)|(bird|fish)
static bool
parse_literal_alternatives (const char *pattern, char ***alternatives, size_t **alt_lengths, size_t *alt_count)
{
  // Count how many alternatives we'll have
  *alt_count      = 0;
  const char *p   = pattern;
  int paren_depth = 0;

  // First pass: count alternatives, accounting for nested groups
  while (*p)
  {
    if (*p == '(')
      paren_depth++;
    else if (*p == ')')
      paren_depth--;
    else if (*p == '|')
    {
      if (paren_depth == 0)
        (*alt_count)++; // Top-level alternation
      else
        (*alt_count)++; // Nested alternation will become separate alternatives
    }
    p++;
  }
  (*alt_count)++; // Add one for the final alternative

  // Allocate arrays with extra space for nested alternatives
  size_t capacity = (*alt_count) * 2; // Allow for expansion
  *alternatives   = malloc (capacity * sizeof (char *));
  *alt_lengths    = malloc (capacity * sizeof (size_t));
  if (!*alternatives || !*alt_lengths)
  {
    free (*alternatives);
    free (*alt_lengths);
    return false;
  }

  // Second pass: extract alternatives
  p                     = pattern;
  const char *alt_start = p;
  paren_depth           = 0;
  size_t alt_idx        = 0;

  while (*p)
  {
    if (*p == '(')
      paren_depth++;
    else if (*p == ')')
      paren_depth--;
    else if (*p == '|' && paren_depth == 0)
    {
      // Top-level alternation - process current alternative
      size_t alt_len = p - alt_start;

      // Check if this is a grouped alternative like (cat|dog)
      if (alt_start[0] == '(' && alt_start[alt_len - 1] == ')')
      {
        // Extract alternatives from inside the group
        const char *inner           = alt_start + 1;
        const char *inner_end       = alt_start + alt_len - 1;
        const char *inner_alt_start = inner;

        while (inner < inner_end)
        {
          if (*inner == '|')
          {
            // Found inner alternation
            size_t inner_len         = inner - inner_alt_start;
            (*alternatives)[alt_idx] = malloc (inner_len + 1);
            if (!(*alternatives)[alt_idx])
              return false;
            memcpy ((*alternatives)[alt_idx], inner_alt_start, inner_len);
            (*alternatives)[alt_idx][inner_len] = '\0';
            (*alt_lengths)[alt_idx]             = inner_len;
            alt_idx++;

            inner_alt_start = inner + 1;
          }
          inner++;
        }
        // Add final inner alternative
        size_t final_inner_len   = inner_end - inner_alt_start;
        (*alternatives)[alt_idx] = malloc (final_inner_len + 1);
        if (!(*alternatives)[alt_idx])
          return false;
        memcpy ((*alternatives)[alt_idx], inner_alt_start, final_inner_len);
        (*alternatives)[alt_idx][final_inner_len] = '\0';
        (*alt_lengths)[alt_idx]                   = final_inner_len;
        alt_idx++;
      }
      else
      {
        // Simple literal alternative
        (*alternatives)[alt_idx] = malloc (alt_len + 1);
        if (!(*alternatives)[alt_idx])
          return false;
        memcpy ((*alternatives)[alt_idx], alt_start, alt_len);
        (*alternatives)[alt_idx][alt_len] = '\0';
        (*alt_lengths)[alt_idx]           = alt_len;
        alt_idx++;
      }

      alt_start = p + 1;
    }
    p++;
  }

  // Process final alternative
  size_t final_len = p - alt_start;
  if (alt_start[0] == '(' && alt_start[final_len - 1] == ')')
  {
    // Extract alternatives from inside the final group
    const char *inner           = alt_start + 1;
    const char *inner_end       = alt_start + final_len - 1;
    const char *inner_alt_start = inner;

    while (inner < inner_end)
    {
      if (*inner == '|')
      {
        size_t inner_len         = inner - inner_alt_start;
        (*alternatives)[alt_idx] = malloc (inner_len + 1);
        if (!(*alternatives)[alt_idx])
          return false;
        memcpy ((*alternatives)[alt_idx], inner_alt_start, inner_len);
        (*alternatives)[alt_idx][inner_len] = '\0';
        (*alt_lengths)[alt_idx]             = inner_len;
        alt_idx++;

        inner_alt_start = inner + 1;
      }
      inner++;
    }
    // Add final inner alternative
    size_t final_inner_len   = inner_end - inner_alt_start;
    (*alternatives)[alt_idx] = malloc (final_inner_len + 1);
    if (!(*alternatives)[alt_idx])
      return false;
    memcpy ((*alternatives)[alt_idx], inner_alt_start, final_inner_len);
    (*alternatives)[alt_idx][final_inner_len] = '\0';
    (*alt_lengths)[alt_idx]                   = final_inner_len;
    alt_idx++;
  }
  else
  {
    // Simple final alternative
    (*alternatives)[alt_idx] = malloc (final_len + 1);
    if (!(*alternatives)[alt_idx])
      return false;
    memcpy ((*alternatives)[alt_idx], alt_start, final_len);
    (*alternatives)[alt_idx][final_len] = '\0';
    (*alt_lengths)[alt_idx]             = final_len;
    alt_idx++;
  }

  *alt_count = alt_idx;
  return alt_idx > 0;
}

// Compile literal alternation optimization
static bool
compile_literal_alt_opt (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!can_use_literal_alt_opt (pattern))
    return false;

  char **alternatives;
  size_t *alt_lengths;
  size_t alt_count;

  if (!parse_literal_alternatives (pattern, &alternatives, &alt_lengths, &alt_count))
    return false;

  compiled->literal_alt.alternatives = alternatives;
  compiled->literal_alt.alt_lengths  = alt_lengths;
  compiled->literal_alt.alt_count    = alt_count;
  compiled->literal_alt.enabled      = true;

  return true;
}

// Match using literal alternation optimization - very fast multi-string search!
static bool
match_with_literal_alt_opt (const struct vibrex_pattern *pattern, const char *text)
{
  if (!pattern->literal_alt.enabled || !text)
    return false;

  const LiteralAltOpt *opt = &pattern->literal_alt;

  // Try each alternative - return true as soon as any is found
  for (size_t i = 0; i < opt->alt_count; i++)
  {
    if (strstr (text, opt->alternatives[i]) != NULL)
      return true;
  }

  return false;
}

// Free literal alternation optimization data
static void
free_literal_alt_opt (LiteralAltOpt *literal_alt)
{
  if (literal_alt && literal_alt->enabled)
  {
    if (literal_alt->alternatives)
    {
      for (size_t i = 0; i < literal_alt->alt_count; i++)
      {
        free (literal_alt->alternatives[i]);
      }
      free (literal_alt->alternatives);
    }
    free (literal_alt->alt_lengths);
    literal_alt->alternatives = NULL;
    literal_alt->alt_lengths  = NULL;
    literal_alt->enabled      = false;
  }
}

/********************************************************************************
 * ADVANCED ALTERNATION OPTIMIZATION ENGINE
 ********************************************************************************/

static bool
can_use_advanced_alternation_opt (const char *pattern)
{
  if (!pattern)
    return false;

  // Count alternations
  int alt_count = 0;
  const char *p = pattern;
  while ((p = strchr (p, '|')) != NULL)
  {
    alt_count++;
    p++;

    // Security check: Prevent DoS via excessive alternations
    if (alt_count > MAX_ALTERNATIONS)
    {
      return false;
    }
  }

  if (alt_count == 0)
    return false;

  // Check for consistent dotstar patterns in all alternations
  bool all_have_dotstar_prefix = true;
  bool all_have_dotstar_suffix = true;
  bool any_have_dotstar        = false;

  p                     = pattern;
  const char *alt_start = p;

  for (int i = 0; i <= alt_count; i++)
  {
    const char *alt_end = strchr (alt_start, '|');
    if (!alt_end)
      alt_end = pattern + strlen (pattern);

    size_t alt_len  = alt_end - alt_start;
    const char *alt = alt_start;

    // Skip ^ anchor if present
    if (alt_len > 0 && alt[0] == '^')
    {
      alt++;
      alt_len--;
    }

    // Skip $ anchor at end
    if (alt_len > 0 && alt[alt_len - 1] == '$')
    {
      alt_len--;
    }

    // Check for .* prefix
    bool has_prefix = (alt_len >= 2 && alt[0] == '.' && alt[1] == '*');
    if (!has_prefix)
      all_have_dotstar_prefix = false;

    // Check for .* suffix
    bool has_suffix = (alt_len >= 2 && alt[alt_len - 2] == '.' && alt[alt_len - 1] == '*');
    if (!has_suffix)
      all_have_dotstar_suffix = false;

    // Check if this alternative has any dotstar
    if (has_prefix || has_suffix)
      any_have_dotstar = true;

    if (!alt_end || *alt_end == '\0')
      break;
    alt_start = alt_end + 1;
  }

  // Check for mixed patterns by analyzing pattern types
  bool has_mixed_dotstar    = false;
  AltPatternType first_type = ALT_LITERAL;
  bool first_set            = false;

  const char *alt_pos = pattern;

  for (int i = 0; i <= alt_count; i++)
  {
    const char *alt_end = strchr (alt_pos, '|');
    if (!alt_end)
      alt_end = pattern + strlen (pattern);

    size_t alt_len              = alt_end - alt_pos;
    AltPatternType pattern_type = classify_alternative_pattern (alt_pos, alt_len);

    if (!first_set)
    {
      first_type = pattern_type;
      first_set  = true;
    }
    else if (first_type != pattern_type)
    {
      has_mixed_dotstar = true;
      break;
    }

    if (!alt_end || *alt_end == '\0')
      break;
    alt_pos = alt_end + 1;
  }

  // Use optimization if any of these conditions are met:
  // 1. ALL alternatives have consistent dotstar patterns (all prefix, all suffix, or all wrapper)
  // 2. No alternatives have dotstar AND we have enough alternations for prefix/suffix optimization
  // 3. Mixed dotstar patterns (some with dotstar, some without - new optimization!)
  bool consistent_dotstar         = (all_have_dotstar_prefix || all_have_dotstar_suffix);
  bool sufficient_alts_no_dotstar = (!any_have_dotstar && alt_count >= 2 &&
                                     (pattern[0] == '^' || alt_count >= 3));
  bool mixed_dotstar_enabled      = has_mixed_dotstar && (pattern[0] == '^'); // Only for anchored patterns

  return consistent_dotstar || sufficient_alts_no_dotstar || mixed_dotstar_enabled;
}

static bool
compile_advanced_alternation_opt (struct vibrex_pattern *compiled, const char *pattern)
{
  if (!can_use_advanced_alternation_opt (pattern))
    return false;

  AlternationOpt *alt_opt = &compiled->alt_opt;
  memset (alt_opt, 0, sizeof (AlternationOpt));

  // Parse alternatives from pattern
  const char **alternatives;
  size_t *alt_lengths;
  if (!parse_alternatives (pattern, &alternatives, &alt_lengths, &alt_opt->alt_count))
    return false;

  // Check for dotstar patterns and handle them specially
  if (check_dotstar_consistency (alternatives, alt_lengths, alt_opt->alt_count, alt_opt))
  {
    if (!compile_dotstar_optimization (alt_opt, alternatives, alt_lengths))
    {
      free (alternatives);
      free (alt_lengths);
      free_alternation_opt (alt_opt);
      return false;
    }

    free (alternatives);
    free (alt_lengths);
    compiled->has_advanced_alt_opt = true;
    return true;
  }

  // Standard prefix/suffix optimization for anchored patterns
  if (!find_common_prefix_suffix (pattern, alternatives, alt_lengths, alt_opt->alt_count, alt_opt))
  {
    free (alternatives);
    free (alt_lengths);
    return false;
  }

  // Compile suffix pattern if it contains regex metacharacters
  if (!compile_suffix_pattern (alt_opt))
  {
    free (alternatives);
    free (alt_lengths);
    free_alternation_opt (alt_opt);
    return false;
  }

  // Compile middle parts of alternatives
  if (!compile_middle_parts (alt_opt, alternatives, alt_lengths))
  {
    free (alternatives);
    free (alt_lengths);
    free_alternation_opt (alt_opt);
    return false;
  }

  free (alternatives);
  free (alt_lengths);
  compiled->has_advanced_alt_opt = true;
  return true;
}

// Match a single alternative based on its classified pattern type
// Handles different matching strategies for literal, dotstar prefix/suffix/wrapper, and regex patterns
static bool
match_single_alternative (const AltSuf *alt_suffix, const char *text, size_t text_len)
{
  const char *core = alt_suffix->core_pattern;

  if (!core || strlen (core) == 0)
  {
    // Empty core pattern handling
    switch (alt_suffix->pattern_type)
    {
    case ALT_LITERAL:
      // Empty literal (^$) matches only empty string
      return (text_len == 0);
    case ALT_DOTSTAR_PREFIX:
    case ALT_DOTSTAR_SUFFIX:
    case ALT_DOTSTAR_WRAPPER:
      // ^.*, ^.*.*, .* - matches everything
      return true;
    case ALT_REGEX:
      return false;
    }
  }

  size_t core_len = strlen (core);

  switch (alt_suffix->pattern_type)
  {
  case ALT_LITERAL:
    // ^foo - exact match
    return (text_len == core_len && strcmp (text, core) == 0);

  case ALT_DOTSTAR_PREFIX:
    // ^.*foo - text must contain core (foo can appear anywhere, not necessarily at end)
    return (strstr (text, core) != NULL);

  case ALT_DOTSTAR_SUFFIX:
    // ^foo.* - text must start with core
    if (text_len >= core_len)
    {
      return (strncmp (text, core, core_len) == 0);
    }
    return false;

  case ALT_DOTSTAR_WRAPPER:
    // ^.*foo.* - core can appear anywhere
    return (strstr (text, core) != NULL);

  case ALT_REGEX:
    // Complex regex - use compiled pattern
    if (alt_suffix->regex_suffix)
    {
      return vibrex_match (alt_suffix->regex_suffix, text);
    }
    return false;
  }

  return false;
}

// Helper function to match dotstar patterns
static bool
match_dotstar_patterns (const AlternationOpt *alt_opt, const char *text, size_t text_len)
{
  // Handle mixed patterns
  if (alt_opt->has_mixed_dotstar)
  {
    for (size_t i = 0; i < alt_opt->alt_count; i++)
    {
      if (match_single_alternative (&alt_opt->suffixes[i], text, text_len))
        return true;
    }
    return false;
  }

  // Handle consistent patterns (original logic)
  if (alt_opt->has_dotstar_wrapper)
  {
    // Pattern: .*core.* - core can appear anywhere
    for (size_t i = 0; i < alt_opt->alt_count; i++)
    {
      if (alt_opt->suffixes[i].literal_suffix)
      {
        if (strstr (text, alt_opt->suffixes[i].literal_suffix) != NULL)
          return true;
      }
      else if (!alt_opt->suffixes[i].literal_suffix && !alt_opt->suffixes[i].regex_suffix)
      {
        // Empty core - .*.* matches everything
        return true;
      }
    }
  }
  else if (alt_opt->has_dotstar_prefix)
  {
    // Pattern: .*core - core must be at end
    for (size_t i = 0; i < alt_opt->alt_count; i++)
    {
      if (alt_opt->suffixes[i].literal_suffix)
      {
        size_t core_len = strlen (alt_opt->suffixes[i].literal_suffix);
        if (text_len >= core_len)
        {
          const char *end_match = text + text_len - core_len;
          if (strcmp (end_match, alt_opt->suffixes[i].literal_suffix) == 0)
            return true;
        }
      }
      else if (!alt_opt->suffixes[i].literal_suffix && !alt_opt->suffixes[i].regex_suffix)
      {
        // Empty core - .* matches everything
        return true;
      }
    }
  }
  else if (alt_opt->has_dotstar_suffix)
  {
    // Pattern: core.* - core must be at start
    for (size_t i = 0; i < alt_opt->alt_count; i++)
    {
      if (alt_opt->suffixes[i].literal_suffix)
      {
        size_t core_len = strlen (alt_opt->suffixes[i].literal_suffix);
        if (text_len >= core_len)
        {
          if (strncmp (text, alt_opt->suffixes[i].literal_suffix, core_len) == 0)
            return true;
        }
      }
      else if (!alt_opt->suffixes[i].literal_suffix && !alt_opt->suffixes[i].regex_suffix)
      {
        // Empty core - .* matches everything
        return true;
      }
    }
  }

  return false;
}

// Helper function to match suffix pattern
static bool
match_suffix_pattern (const AlternationOpt *alt_opt, const char *text, size_t text_len, const char **match_end)
{
  if (alt_opt->suffix_len == 0)
  {
    *match_end = text + text_len;
    return true;
  }

  if (alt_opt->suffix_pattern)
  {
    // Use compiled regex pattern for suffix matching
    // We need to find the longest suffix that matches the pattern
    for (size_t i = 0; i <= text_len && i <= alt_opt->suffix_len + 10; i++)
    {
      const char *suffix_text = text + text_len - i;
      if (vibrex_match (alt_opt->suffix_pattern, suffix_text))
      {
        *match_end = text + text_len - i;
        return true;
      }
    }
    return false;
  }
  else
  {
    // Use literal string comparison for suffix
    if (text_len < alt_opt->suffix_len ||
        strcmp (text + text_len - alt_opt->suffix_len, alt_opt->suffix) != 0)
    {
      return false;
    }
    *match_end = text + text_len - alt_opt->suffix_len;
    return true;
  }
}

// Helper function to match alternatives
static bool
match_alternatives (const AlternationOpt *alt_opt, const char *middle_text, size_t middle_len)
{
  for (size_t i = 0; i < alt_opt->alt_count; i++)
  {
    bool matches = false;

    if (alt_opt->suffixes[i].literal_suffix)
    {
      if (middle_text && strcmp (middle_text, alt_opt->suffixes[i].literal_suffix) == 0)
      {
        matches = true;
      }
      else if (!middle_text && strlen (alt_opt->suffixes[i].literal_suffix) == 0)
      {
        matches = true;
      }
    }
    else if (alt_opt->suffixes[i].regex_suffix)
    {
      if (middle_text && vibrex_match (alt_opt->suffixes[i].regex_suffix, middle_text))
      {
        matches = true;
      }
    }
    else
    {
      // Empty middle part
      if (!middle_text || middle_len == 0)
      {
        matches = true;
      }
    }

    if (matches)
      return true;
  }

  return false;
}

static bool
match_with_advanced_alternation_opt (const struct vibrex_pattern *pattern, const char *text)
{
  if (!pattern->has_advanced_alt_opt || !text)
    return false;

  const AlternationOpt *alt_opt = &pattern->alt_opt;
  size_t text_len               = strlen (text);

  // Handle dotstar optimizations (consistent or mixed)
  if (alt_opt->has_dotstar_prefix || alt_opt->has_dotstar_suffix || alt_opt->has_mixed_dotstar)
  {
    return match_dotstar_patterns (alt_opt, text, text_len);
  }

  // Handle standard prefix/suffix optimization
  const char *match_start = text;
  const char *match_end;

  // Check prefix
  if (alt_opt->prefix_len > 0)
  {
    if (text_len < alt_opt->prefix_len ||
        strncmp (text, alt_opt->prefix, alt_opt->prefix_len) != 0)
    {
      return false;
    }
    match_start += alt_opt->prefix_len;
  }

  // Check suffix
  if (!match_suffix_pattern (alt_opt, text, text_len, &match_end))
  {
    return false;
  }

  // Extract middle part
  size_t middle_len = match_end - match_start;
  char *middle_text = NULL;
  if (middle_len > 0)
  {
    middle_text = malloc (middle_len + 1);
    if (!middle_text)
      return false;
    memcpy (middle_text, match_start, middle_len);
    middle_text[middle_len] = '\0';
  }

  // Check alternatives
  bool result = match_alternatives (alt_opt, middle_text, middle_len);
  free (middle_text);
  return result;
}

static void
free_alternation_opt (AlternationOpt *alt_opt)
{
  if (!alt_opt)
    return;

  free (alt_opt->prefix);
  alt_opt->prefix = NULL;

  free (alt_opt->suffix);
  alt_opt->suffix = NULL;

  vibrex_free (alt_opt->suffix_pattern);
  alt_opt->suffix_pattern = NULL;

  if (alt_opt->suffixes)
  {
    for (size_t i = 0; i < alt_opt->alt_count; i++)
    {
      free (alt_opt->suffixes[i].literal_suffix);
      free (alt_opt->suffixes[i].core_pattern);
      vibrex_free (alt_opt->suffixes[i].regex_suffix);
    }
    free (alt_opt->suffixes);
    alt_opt->suffixes = NULL;
  }
}

// Helper function to parse alternatives from pattern
static bool
parse_alternatives (const char *pattern, const char ***alternatives, size_t **alt_lengths, size_t *alt_count)
{
  // Count alternations
  *alt_count          = 1;
  const char *counter = pattern;
  while ((counter = strchr (counter, '|')) != NULL)
  {
    (*alt_count)++;
    counter++;

    // Security check: Prevent DoS via excessive alternations
    if (*alt_count > MAX_ALTERNATIONS)
    {
      return false;
    }
  }

  // Allocate arrays
  *alternatives = malloc (*alt_count * sizeof (char *));
  *alt_lengths  = malloc (*alt_count * sizeof (size_t));
  if (!*alternatives || !*alt_lengths)
  {
    free (*alternatives);
    free (*alt_lengths);
    return false;
  }

  // Parse alternatives
  const char *alt_start = pattern;
  for (size_t i = 0; i < *alt_count; i++)
  {
    const char *alt_end = strchr (alt_start, '|');
    if (!alt_end)
      alt_end = pattern + strlen (pattern);

    (*alternatives)[i] = alt_start;
    (*alt_lengths)[i]  = alt_end - alt_start;

    if (!alt_end || *alt_end == '\0')
      break;
    alt_start = alt_end + 1;
  }

  return true;
}

// Classify the type of an alternative pattern for optimization purposes
// Determines whether the pattern is literal, has dotstar prefix/suffix/wrapper, or is complex regex
static AltPatternType
classify_alternative_pattern (const char *alt, size_t len)
{
  // Skip ^ anchor if present
  if (len > 0 && alt[0] == '^')
  {
    alt++;
    len--;
  }

  // Skip $ anchor at end
  if (len > 0 && alt[len - 1] == '$')
  {
    len--;
  }

  if (len < 2)
    return ALT_LITERAL;

  bool has_prefix = (alt[0] == '.' && alt[1] == '*');
  bool has_suffix = (len >= 2 && alt[len - 2] == '.' && alt[len - 1] == '*');

  if (has_prefix && has_suffix)
    return ALT_DOTSTAR_WRAPPER;
  else if (has_prefix)
    return ALT_DOTSTAR_PREFIX;
  else if (has_suffix)
    return ALT_DOTSTAR_SUFFIX;
  else
  {
    // Check if it's a simple literal pattern or complex regex
    for (size_t i = 0; i < len; i++)
    {
      if (strchr (".?*+[]()|\\", alt[i]))
        return ALT_REGEX;
    }
    return ALT_LITERAL;
  }
}

// Helper function to check dotstar consistency across alternatives
static bool
check_dotstar_consistency (const char **alternatives, size_t *alt_lengths, size_t alt_count, AlternationOpt *alt_opt)
{
  bool all_have_dotstar_prefix = true;
  bool all_have_dotstar_suffix = true;
  bool has_mixed_patterns      = false;
  AltPatternType first_type    = ALT_LITERAL;
  bool first_set               = false;

  for (size_t i = 0; i < alt_count; i++)
  {
    const char *alt = alternatives[i];
    size_t len      = alt_lengths[i];

    // Skip ^ anchor if present
    if (len > 0 && alt[0] == '^')
    {
      alt++;
      len--;
    }

    // Skip $ anchor at end
    if (len > 0 && alt[len - 1] == '$')
    {
      len--;
    }

    // Check for .* prefix
    bool has_prefix = (len >= 2 && alt[0] == '.' && alt[1] == '*');
    if (!has_prefix)
      all_have_dotstar_prefix = false;

    // Check for .* suffix
    bool has_suffix = (len >= 2 && alt[len - 2] == '.' && alt[len - 1] == '*');
    if (!has_suffix)
      all_have_dotstar_suffix = false;

    // Classify pattern type for mixed optimization
    AltPatternType pattern_type = classify_alternative_pattern (alternatives[i], alt_lengths[i]);

    if (!first_set)
    {
      first_type = pattern_type;
      first_set  = true;
    }
    else if (first_type != pattern_type)
    {
      has_mixed_patterns = true;
    }
  }

  alt_opt->has_dotstar_prefix  = all_have_dotstar_prefix;
  alt_opt->has_dotstar_suffix  = all_have_dotstar_suffix;
  alt_opt->has_dotstar_wrapper = all_have_dotstar_prefix && all_have_dotstar_suffix;
  alt_opt->has_mixed_dotstar   = has_mixed_patterns;

  // Return true if we should use dotstar optimization (consistent OR mixed)
  return all_have_dotstar_prefix || all_have_dotstar_suffix || has_mixed_patterns;
}

// Extract the core pattern from an alternative by removing dotstar wrappers
// For example: "^.*foo.*" -> "foo", "^bar.*" -> "bar", "^baz" -> "baz"
static bool
extract_core_pattern (const char *alt, size_t alt_len, AltPatternType pattern_type, char **core_pattern, size_t *core_len)
{
  const char *start = alt;
  size_t len        = alt_len;

  // Skip ^ anchor
  if (len > 0 && start[0] == '^')
  {
    start++;
    len--;
  }

  // Skip $ anchor at end
  if (len > 0 && start[len - 1] == '$')
  {
    len--;
  }

  // Extract core based on pattern type
  switch (pattern_type)
  {
  case ALT_DOTSTAR_PREFIX:
    // ^.*foo -> foo
    if (len >= 2)
    {
      start += 2; // skip .*
      len -= 2;
    }
    break;

  case ALT_DOTSTAR_SUFFIX:
    // ^foo.* -> foo
    if (len >= 2)
    {
      len -= 2; // remove .*
    }
    break;

  case ALT_DOTSTAR_WRAPPER:
    // ^.*foo.* -> foo
    if (len >= 4)
    {
      start += 2; // skip .*
      len -= 4;   // remove .* from both ends
    }
    else if (len == 2)
    {
      // Just ^.* - empty core
      len = 0;
    }
    break;

  case ALT_LITERAL:
  case ALT_REGEX:
    // Use as-is
    break;
  }

  // Store the core pattern
  if (len > 0)
  {
    *core_pattern = malloc (len + 1);
    if (!*core_pattern)
      return false;
    memcpy (*core_pattern, start, len);
    (*core_pattern)[len] = '\0';
    *core_len            = len;
  }
  else
  {
    *core_pattern = NULL;
    *core_len     = 0;
  }

  return true;
}

// Helper function to compile dotstar optimization
static bool
compile_dotstar_optimization (AlternationOpt *alt_opt, const char **alternatives, size_t *alt_lengths)
{
  alt_opt->suffixes = calloc (alt_opt->alt_count, sizeof (AltSuf));
  if (!alt_opt->suffixes)
    return false;

  // Store the core patterns and pattern types
  for (size_t i = 0; i < alt_opt->alt_count; i++)
  {
    const char *alt = alternatives[i];
    size_t len      = alt_lengths[i];

    // Classify the pattern type
    AltPatternType pattern_type       = classify_alternative_pattern (alt, len);
    alt_opt->suffixes[i].pattern_type = pattern_type;

    // Extract core pattern
    char *core_pattern;
    size_t core_len;
    if (!extract_core_pattern (alt, len, pattern_type, &core_pattern, &core_len))
      return false;

    alt_opt->suffixes[i].core_pattern = core_pattern;

    // For mixed patterns, we store both literal_suffix (for simple cases)
    // and core_pattern (for type-specific matching)
    if (pattern_type == ALT_LITERAL || pattern_type == ALT_DOTSTAR_PREFIX ||
        pattern_type == ALT_DOTSTAR_SUFFIX || pattern_type == ALT_DOTSTAR_WRAPPER)
    {
      alt_opt->suffixes[i].literal_suffix = core_pattern ? strdup (core_pattern) : NULL;
    }
    else if (pattern_type == ALT_REGEX && core_pattern)
    {
      // Compile regex patterns
      char regex_pattern[PATTERN_BUFFER_SIZE];
      int offset = sprintf (regex_pattern, "^");
      strcpy (regex_pattern + offset, core_pattern);
      strcat (regex_pattern, "$");

      alt_opt->suffixes[i].regex_suffix = vibrex_compile (regex_pattern, NULL);
      if (!alt_opt->suffixes[i].regex_suffix)
        return false;
    }
  }

  return true;
}

// Helper function to find common prefix and suffix
static bool
find_common_prefix_suffix (const char *pattern, const char **alternatives, size_t *alt_lengths, size_t alt_count, AlternationOpt *alt_opt)
{
  const char *p    = pattern;
  bool is_anchored = (p[0] == '^');
  if (is_anchored)
    p++;

  const char *first_pipe = strchr (p, '|');
  if (!first_pipe)
    return false;

  // Find common prefix
  size_t max_prefix_len = first_pipe - p;
  size_t prefix_len     = 0;
  for (prefix_len = 0; prefix_len < max_prefix_len; prefix_len++)
  {
    char c         = p[prefix_len];
    bool all_match = true;

    for (size_t i = 1; i < alt_count; i++)
    {
      const char *alt = alternatives[i];
      if (alt[0] == '^')
        alt++;

      if (prefix_len >= alt_lengths[i] || alt[prefix_len] != c)
      {
        all_match = false;
        break;
      }
    }
    if (!all_match)
      break;
  }

  // Find common suffix
  size_t suffix_len = 0;
  if (alt_count > 1)
  {
    size_t min_len = alt_lengths[0];
    if (alternatives[0][0] == '^')
      min_len--;
    if (min_len > 0 && alternatives[0][alt_lengths[0] - 1] == '$')
      min_len--;

    for (size_t i = 1; i < alt_count; i++)
    {
      size_t len = alt_lengths[i];
      if (alternatives[i][0] == '^')
        len--;
      if (len > 0 && alternatives[i][alt_lengths[i] - 1] == '$')
        len--;

      if (len < min_len)
        min_len = len;
    }

    for (suffix_len = 1; suffix_len <= min_len - prefix_len; suffix_len++)
    {
      bool all_match = true;

      // Get the character from the first alternative's suffix position to compare against
      const char *first_alt = alternatives[0];
      size_t first_len      = alt_lengths[0];
      if (first_alt[0] == '^')
      {
        first_alt++;
        first_len--;
      }
      if (first_len > 0 && first_alt[first_len - 1] == '$')
      {
        first_len--;
      }

      if (first_len < prefix_len + suffix_len)
      {
        suffix_len--;
        break;
      }

      char expected_c = first_alt[first_len - suffix_len];

      for (size_t i = 0; i < alt_count; i++)
      {
        const char *alt = alternatives[i];
        size_t len      = alt_lengths[i];

        if (alt[0] == '^')
        {
          alt++;
          len--;
        }
        if (len > 0 && alt[len - 1] == '$')
        {
          len--;
        }

        if (len < prefix_len + suffix_len)
        {
          all_match = false;
          break;
        }

        if (alt[len - suffix_len] != expected_c)
        {
          all_match = false;
          break;
        }
      }

      if (!all_match)
      {
        suffix_len--;
        break;
      }
    }

    if (suffix_len > min_len - prefix_len)
      suffix_len = min_len - prefix_len;
  }

  // Only proceed if we have meaningful optimization
  if (prefix_len < 3 && suffix_len < 3)
    return false;

  // Store prefix
  if (prefix_len > 0)
  {
    alt_opt->prefix = malloc (prefix_len + 1);
    if (!alt_opt->prefix)
      return false;
    memcpy (alt_opt->prefix, p, prefix_len);
    alt_opt->prefix[prefix_len] = '\0';
    alt_opt->prefix_len         = prefix_len;
  }

  // Store suffix
  if (suffix_len > 0)
  {
    alt_opt->suffix = malloc (suffix_len + 1);
    if (!alt_opt->suffix)
      return false;

    // Get suffix from the first alternative
    const char *first_alt = alternatives[0];
    size_t first_len      = alt_lengths[0];
    if (first_alt[0] == '^')
    {
      first_alt++;
      first_len--;
    }
    if (first_len > 0 && first_alt[first_len - 1] == '$')
    {
      first_len--;
    }

    const char *suffix_start = first_alt + first_len - suffix_len;
    memcpy (alt_opt->suffix, suffix_start, suffix_len);
    alt_opt->suffix[suffix_len] = '\0';
    alt_opt->suffix_len         = suffix_len;
  }

  return true;
}

// Helper function to compile suffix pattern if it contains regex metacharacters
static bool
compile_suffix_pattern (AlternationOpt *alt_opt)
{
  if (alt_opt->suffix_len == 0)
    return true;

  // Check if suffix contains regex metacharacters
  bool suffix_is_literal = true;
  for (size_t i = 0; i < alt_opt->suffix_len; i++)
  {
    if (strchr (".?*+[]()|\\", alt_opt->suffix[i]))
    {
      suffix_is_literal = false;
      break;
    }
  }

  // If suffix contains regex metacharacters, compile it as a pattern
  if (!suffix_is_literal)
  {
    char suffix_pattern_str[PATTERN_BUFFER_SIZE];
    int offset = sprintf (suffix_pattern_str, "^");
    memcpy (suffix_pattern_str + offset, alt_opt->suffix, alt_opt->suffix_len);
    offset += alt_opt->suffix_len;
    suffix_pattern_str[offset++] = '$';
    suffix_pattern_str[offset]   = '\0';

    alt_opt->suffix_pattern = vibrex_compile (suffix_pattern_str, NULL);
    if (!alt_opt->suffix_pattern)
      return false;
  }

  return true;
}

// Helper function to compile middle parts of alternatives
static bool
compile_middle_parts (AlternationOpt *alt_opt, const char **alternatives, size_t *alt_lengths)
{
  alt_opt->suffixes = calloc (alt_opt->alt_count, sizeof (AltSuf));
  if (!alt_opt->suffixes)
    return false;

  // Store middle parts of alternatives
  for (size_t i = 0; i < alt_opt->alt_count; i++)
  {
    const char *alt = alternatives[i];
    size_t len      = alt_lengths[i];

    if (alt[0] == '^')
    {
      alt++;
      len--;
    }
    if (len > 0 && alt[len - 1] == '$')
    {
      len--;
    }

    const char *middle_start = alt + alt_opt->prefix_len;
    size_t middle_len        = len - alt_opt->prefix_len - alt_opt->suffix_len;

    if (middle_len > 0)
    {
      bool is_literal = true;
      for (size_t j = 0; j < middle_len; j++)
      {
        if (strchr (".?*+[]()|\\", middle_start[j]))
        {
          is_literal = false;
          break;
        }
      }

      if (is_literal)
      {
        alt_opt->suffixes[i].literal_suffix = malloc (middle_len + 1);
        if (!alt_opt->suffixes[i].literal_suffix)
          return false;
        memcpy (alt_opt->suffixes[i].literal_suffix, middle_start, middle_len);
        alt_opt->suffixes[i].literal_suffix[middle_len] = '\0';
      }
      else
      {
        char sub_pattern_str[PATTERN_BUFFER_SIZE];
        int offset = sprintf (sub_pattern_str, "^");
        memcpy (sub_pattern_str + offset, middle_start, middle_len);
        offset += middle_len;
        sub_pattern_str[offset++] = '$';
        sub_pattern_str[offset]   = '\0';

        alt_opt->suffixes[i].regex_suffix = vibrex_compile (sub_pattern_str, NULL);
        if (!alt_opt->suffixes[i].regex_suffix)
          return false;
      }
    }
  }

  return true;
}

static const char *
boyer_moore_search (const char *text, int text_len, const struct vibrex_pattern *pattern)
{
  if (!pattern->bad_char.enabled || pattern->prefix_len > text_len)
    return NULL;

  const char *pattern_str = pattern->literal_prefix;
  int pattern_len         = pattern->prefix_len;
  const int *skip_table   = pattern->bad_char.skip;

  int skip = 0;
  while (skip <= text_len - pattern_len)
  {
    int j = pattern_len - 1;
    while (j >= 0 && pattern_str[j] == text[skip + j])
      j--;

    if (j < 0)
    {
      return text + skip;
    }
    else
    {
      int char_skip = skip_table[(unsigned char)text[skip + j]];
      skip += (char_skip > 0 ? char_skip : 1);
    }
  }
  return NULL;
}
