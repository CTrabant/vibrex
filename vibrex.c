/*********************************************************************************
 * A limited regular expression engine, boldly going where no regex has gone
 * before (maybe).
 *
 * This regex engine is designed to be faster than a warp 11 and more
 * memory-efficient than a tribble in a transporter buffer. Features include:
 *   - ASCII and extended ASCII support (Klingon not included)
 *   - Optimized for speed, memory, and avoiding red-shirt fates
 *   - Alternation matching so fast, even Data would raise an eyebrow
 *   - Limited regex features:
 *     .      - Any single character (except Q, he's everywhere)
 *     *      - Zero or more of the preceding atom (like tribbles)
 *     +      - One or more of the preceding atom (like tribbles, but more)
 *     ?      - Zero or one of the preceding atom (Schrödinger's atom)
 *     (?:)   - Non-capturing groups (because some things are best left unremembered)
 *     ()     - Group syntax but without capturing (like a holodeck program with no save file)
 *     ()?    - Optional groups (for when you can't decide if you want to beam down)
 *     ^      - Anchor to the start of the string (make it so)
 *     $      - Anchor to the end of the string (engage!)
 *     |      - Alternation (logical OR, not the Vulcan kind)
 *     [abc]  - Character class (choose your away team)
 *     [a-z]  - Character class range (alphabet soup, Starfleet style)
 *     [^abc] - Negated character class (no red shirts allowed)
 *     [a-z]? - Optional character class (maybe a red shirt sneaks in)
 *
 *
 * I did not write this code. I prompted a few LLMs to write it Star Trek style
 * ("Computer, build a regex engine with the following features...") and then to
 * write a test suite and iteratively goaded them to debug the code via tests. I
 * ensured the tests are comprehensive, and reviewed comments and API
 * definitions for clarity and style, but that's it. If you find a bug, blame
 * the holodeck safety protocols.
 *
 * Don't trust this code, trust the tests that prove it works as expected.
 *
 * As the code (and cringe jokes) are written by LLMs, all coders whose code was
 * fed to the LLMs get credit for anything that works. If you find a bug, blame
 * the LLMs, but if you see something clever, thank the collective wisdom of the
 * open source galaxy.
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
 * Version: 0.0.2
 *********************************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "vibrex.h"

#define VIBREX_MAX_RECURSION_DEPTH 32

/* Instruction types for the compiled pattern */
typedef enum
{
  OP_CHAR,        /* Match specific character */
  OP_DOT,         /* Match any character */
  OP_STAR,        /* Zero or more repetitions */
  OP_PLUS,        /* One or more repetitions */
  OP_OPTIONAL,    /* Optional group */
  OP_START,       /* Start of string anchor */
  OP_END,         /* End of string anchor */
  OP_BRANCH,      /* Branch for alternation */
  OP_JUMP,        /* Jump instruction */
  OP_MATCH,       /* Successful match */
  OP_CHARCLASS,   /* Character class */
  OP_GROUP_START, /* Start of non-capturing group */
  OP_GROUP_END    /* End of non-capturing group */
} opcode_t;

/* Instruction structure */
typedef struct
{
  opcode_t op;
  union
  {
    unsigned char ch; /* For OP_CHAR */
    struct
    {                           /* For OP_CHARCLASS */
      unsigned char bitmap[32]; /* 256 bits for ASCII */
    } charclass;
    int offset; /* For OP_JUMP, OP_BRANCH, OP_GROUP_START, OP_GROUP_END */
  } data;
} instruction_t;

/* Pre-compiled alternative for optimized alternation matching */
typedef struct
{
  vibrex_t *compiled;
  char *pattern_text;
  size_t pattern_len;
} alternative_t;

/* Optimized alternation dispatch table for single character alternatives */
typedef struct
{
  bool is_single_char_dispatch;
  bool char_table[256];     /* Direct lookup table for single characters */
  unsigned char *char_list; /* List of characters for iteration */
  size_t char_count;
} char_dispatch_t;

/* Compiled pattern structure */
struct vibrex_pattern
{
  instruction_t *code;
  size_t code_size;
  size_t code_capacity;
  bool anchored_start;
  bool anchored_end;
  bool has_dotstar;       /* Optimization flag for .* patterns */
  char *original_pattern; /* Store original pattern for alternation handling */

  /* Optimized alternation support */
  bool has_alternation;
  alternative_t *alternatives;
  size_t alt_count;
  size_t alt_capacity;
  char_dispatch_t char_dispatch; /* Fast dispatch for single characters */

  /* Common prefix/suffix optimization */
  char *common_prefix;
  size_t common_prefix_len;
  char *common_suffix;
  size_t common_suffix_len;
  vibrex_t *compiled_suffix;

  /* First character optimization */
  char first_char;
  bool is_first_char_literal;

  /* Dot-star-literal optimization */
  char *dotstar_literal;
  size_t dotstar_literal_len;
  bool dotstar_anchored_end;
};

/* Forward declarations */
static vibrex_t *vibrex_compile_recursive (const char *pattern, int depth);
static bool compile_pattern (vibrex_t *pattern, const char **regex, int depth);
static bool compile_atom (vibrex_t *pattern, const char **regex, int depth);
static bool compile_charclass (vibrex_t *pattern, const char **regex);
static bool compile_group (vibrex_t *pattern, const char **regex, int depth);
static bool add_instruction (vibrex_t *pattern, instruction_t instr);
static const char *match_here (const instruction_t *code, const char *text, int pc);
static const char *match_star (const instruction_t *code, const char *text, int pc);
static const char *match_plus (const instruction_t *code, const char *text, int pc);
static const char *match_optional (const instruction_t *code, const char *text, int pc, unsigned char ch);
static const char *match_optional_group (const instruction_t *code, const char *text, int pc);
static inline bool char_matches_atom (const instruction_t *atom, unsigned char c);
static const char *match_group_star (const instruction_t *code, const char *text, int pc);
static const char *match_group_plus (const instruction_t *code, const char *text, int pc);

/* Alternation optimization functions */
static bool analyze_alternations (vibrex_t *pattern, int depth);
static bool build_char_dispatch (vibrex_t *pattern);
static size_t find_common_prefix (vibrex_t *pattern);
static size_t find_common_suffix (vibrex_t *pattern);
static bool precompile_alternatives (vibrex_t *pattern, int depth);
static vibrex_t *compile_simple_regex (const char *pattern_str, bool anchored_end, int depth);
static void free_alternatives (vibrex_t *pattern);
static bool match_alternations_optimized (const vibrex_t *pattern, const char *text);
static bool is_single_char_pattern (const char *pattern);
static char get_single_char_from_pattern (const char *pattern);

vibrex_t *
vibrex_compile (const char *pattern)
{
  return vibrex_compile_recursive (pattern, 0);
}

static vibrex_t *
vibrex_compile_recursive (const char *pattern, int depth)
{
  if (depth > VIBREX_MAX_RECURSION_DEPTH)
  {
    return NULL; /* Max recursion depth exceeded */
  }
  if (!pattern)
    return NULL;

  vibrex_t *compiled = malloc (sizeof (vibrex_t));
  if (!compiled)
    return NULL;

  compiled->code_capacity = 64;
  compiled->code          = malloc (compiled->code_capacity * sizeof (instruction_t));
  if (!compiled->code)
  {
    free (compiled);
    return NULL;
  }

  compiled->code_size      = 0;
  compiled->anchored_start = false;
  compiled->anchored_end   = false;
  compiled->has_dotstar    = false;

  /* Initialize alternation optimization fields */
  compiled->has_alternation                       = false;
  compiled->alternatives                          = NULL;
  compiled->alt_count                             = 0;
  compiled->alt_capacity                          = 0;
  compiled->char_dispatch.is_single_char_dispatch = false;
  compiled->char_dispatch.char_list               = NULL;
  compiled->char_dispatch.char_count              = 0;
  compiled->common_prefix                         = NULL;
  compiled->common_prefix_len                     = 0;
  compiled->common_suffix                         = NULL;
  compiled->common_suffix_len                     = 0;
  compiled->compiled_suffix                       = NULL;
  memset (compiled->char_dispatch.char_table, 0, sizeof (compiled->char_dispatch.char_table));
  compiled->first_char            = '\0';
  compiled->is_first_char_literal = false;
  compiled->dotstar_literal       = NULL;
  compiled->dotstar_literal_len   = 0;
  compiled->dotstar_anchored_end  = false;

  /* Store original pattern for alternation handling */
  compiled->original_pattern = malloc (strlen (pattern) + 1);
  if (!compiled->original_pattern)
  {
    free (compiled->code);
    free (compiled);
    return NULL;
  }
  strcpy (compiled->original_pattern, pattern);

  /* Check if pattern has alternations and optimize if needed */
  if (strchr (compiled->original_pattern, '|'))
  {
    compiled->has_alternation = true;
    if (!analyze_alternations (compiled, depth))
    {
      vibrex_free (compiled);
      return NULL;
    }
    /* For alternation patterns, we rely on the optimized matching */
    instruction_t match_instr = {.op = OP_MATCH};
    if (!add_instruction (compiled, match_instr))
    {
      vibrex_free (compiled);
      return NULL;
    }
    return compiled;
  }

  const char *p = pattern;

  /* Check for start anchor */
  if (*p == '^')
  {
    compiled->anchored_start = true;
    p++;
  }

  /* Compile the main pattern */
  if (!compile_pattern (compiled, &p, depth))
  {
    vibrex_free (compiled);
    return NULL;
  }

  /* If we stopped at an unmatched closing parenthesis, that's a syntax error */
  if (*p == ')')
  {
    vibrex_free (compiled);
    return NULL;
  }

  /* Check for end anchor */
  if (*p == '$')
  {
    compiled->anchored_end  = true;
    instruction_t end_instr = {.op = OP_END};
    if (!add_instruction (compiled, end_instr))
    {
      vibrex_free (compiled);
      return NULL;
    }
    p++;
  }

  /* Add final match instruction */
  instruction_t match_instr = {.op = OP_MATCH};
  if (!add_instruction (compiled, match_instr))
  {
    vibrex_free (compiled);
    return NULL;
  }

  /* Detect .* optimization */
  if (compiled->code_size >= 3 &&
      compiled->code[0].op == OP_DOT &&
      compiled->code[1].op == OP_STAR)
  {
    compiled->has_dotstar = true;
    // Extended: check for .*[literal] or .*[literal]$
    size_t i             = 2;
    size_t literal_start = i;
    while (i < compiled->code_size - 1 && compiled->code[i].op == OP_CHAR)
    {
      i++;
    }
    bool ends_with_end = (i < compiled->code_size - 1 && compiled->code[i].op == OP_END);
    if ((i > literal_start) && (i == compiled->code_size - 1 || (ends_with_end && i == compiled->code_size - 2)))
    {
      // Found .*[literal] or .*[literal]$
      size_t literal_len        = i - literal_start;
      compiled->dotstar_literal = malloc (literal_len + 1);
      if (compiled->dotstar_literal)
      {
        for (size_t j = 0; j < literal_len; ++j)
        {
          compiled->dotstar_literal[j] = compiled->code[literal_start + j].data.ch;
        }
        compiled->dotstar_literal[literal_len] = '\0';
        compiled->dotstar_literal_len          = literal_len;
        compiled->dotstar_anchored_end         = ends_with_end;
      }
    }
  }

  /* First character optimization: check if first instruction is a literal char */
  if (!compiled->anchored_start && compiled->code_size > 0 && compiled->code[0].op == OP_CHAR)
  {
    /* Optimization is only valid if the first char is NOT optional or part of a star quantifier */
    if (compiled->code_size < 2 || (compiled->code[1].op != OP_STAR && compiled->code[1].op != OP_OPTIONAL))
    {
      compiled->first_char            = compiled->code[0].data.ch;
      compiled->is_first_char_literal = true;
    }
  }

  return compiled;
}

static bool
compile_pattern (vibrex_t *pattern, const char **regex, int depth)
{
  const char *p = *regex;

  while (*p && *p != ')' && *p != '$')
  {
    /* Skip alternation markers - they're handled at match time */
    if (*p == '|')
    {
      break; /* Stop at alternation - let match-time handler deal with it */
    }

    if (!compile_atom (pattern, &p, depth))
      return false;
  }

  *regex = p;
  return true;
}

static bool
compile_atom (vibrex_t *pattern, const char **regex, int depth)
{
  const char *p = *regex;
  instruction_t instr;

  switch (*p)
  {
  case '.':
    instr.op = OP_DOT;
    p++;
    break;

  case '[':
    p++;
    if (!compile_charclass (pattern, &p))
      return false;
    goto check_quantifier;

  case '(':
    /* Check for non-capturing group syntax (?:...) */
    if (p[1] && p[2] && p[1] == '?' && p[2] == ':')
    {
      p += 3; /* Skip (?: */
      if (!compile_group (pattern, &p, depth))
        return false;
      if (*p != ')')
        return false; /* Expect closing ) */
      p++;
      goto check_quantifier;
    }
    else
    {
      /* Plain group '()' */
      p++; /* Skip '(' */
      if (!compile_group (pattern, &p, depth))
        return false;
      if (*p != ')')
        return false; /* Expect closing ) */
      p++;
      goto check_quantifier;
    }

  case '\\':
    p++;
    if (!*p)
      return false;
    instr.op      = OP_CHAR;
    instr.data.ch = *p;
    p++;
    break;

  default:
    if (*p == '*' || *p == '+' || *p == '?' || *p == '|' ||
        *p == ')' || *p == '^' || *p == '$' || *p == ']')
    {
      return false; /* Invalid atom */
    }
    instr.op      = OP_CHAR;
    instr.data.ch = *p;
    p++;
    break;
  }

  if (!add_instruction (pattern, instr))
    return false;

check_quantifier:
  /* Handle quantifiers */
  if (*p == '*')
  {
    instruction_t star = {.op = OP_STAR};
    if (!add_instruction (pattern, star))
      return false;
    p++;
  }
  else if (*p == '+')
  {
    instruction_t plus = {.op = OP_PLUS};
    if (!add_instruction (pattern, plus))
      return false;
    p++;
  }
  else if (*p == '?')
  {
    instruction_t optional = {.op = OP_OPTIONAL};
    if (!add_instruction (pattern, optional))
      return false;
    p++;
  }

  *regex = p;
  return true;
}

static bool
compile_charclass (vibrex_t *pattern, const char **regex)
{
  const char *p = *regex;
  instruction_t instr;
  instr.op = OP_CHARCLASS;
  memset (instr.data.charclass.bitmap, 0, sizeof (instr.data.charclass.bitmap));

  bool negated = false;
  if (*p == '^')
  {
    negated = true;
    p++;
  }

  /* If the first character is ']' or end of string, it's a syntax error */
  if (*p == ']' || *p == '\0')
    return false;

  while (*p && *p != ']')
  {
    unsigned char start = *p++;
    unsigned char end   = start;

    /* Handle ranges */
    if (*p == '-' && p[1] && p[1] != ']')
    {
      p++;
      end = *p++;
      if (end < start)
        return false; // Invalid range
    }

    /* Set bits for the range */
    for (unsigned char ch = start; ch <= end; ch++)
    {
      instr.data.charclass.bitmap[ch / 8] |= (1 << (ch % 8));
    }
  }

  if (*p != ']')
    return false;
  p++;

  /* Apply negation */
  if (negated)
  {
    for (int i = 0; i < 32; i++)
    {
      instr.data.charclass.bitmap[i] = ~instr.data.charclass.bitmap[i];
    }
  }

  *regex = p;
  return add_instruction (pattern, instr);
}

static bool
compile_group (vibrex_t *pattern, const char **regex, int depth)
{
  const char *p = *regex;

  /* Check if this group contains alternations */
  const char *scan     = p;
  bool has_alternation = false;
  int paren_depth      = 0;
  while (*scan && (*scan != ')' || paren_depth > 0))
  {
    if (*scan == '(' && scan[1] && scan[2] && scan[1] == '?' && scan[2] == ':')
    {
      paren_depth++;
      scan += 3;
    }
    else if (*scan == ')' && paren_depth > 0)
    {
      paren_depth--;
      scan++;
    }
    else if (*scan == '|' && paren_depth == 0)
    {
      has_alternation = true;
      break;
    }
    else
    {
      scan++;
    }
  }

  if (has_alternation)
  {
    /* Group contains alternation - treat as a mini alternation pattern */
    while (*scan && *scan != ')')
      scan++; /* Find end of group */
    if (*scan != ')')
      return false;

    size_t total_len    = scan - p;
    char *group_pattern = malloc (total_len + 1);
    if (!group_pattern)
      return false;

    strncpy (group_pattern, p, total_len);
    group_pattern[total_len] = '\0';

    /* Create a sub-pattern and compile it with alternation support */
    vibrex_t *sub_pattern = vibrex_compile_recursive (group_pattern, depth + 1);
    free (group_pattern);

    if (!sub_pattern)
      return false;

    /* Add group start instruction */
    instruction_t group_start = {.op = OP_GROUP_START};
    size_t group_start_pos    = pattern->code_size;
    group_start.data.offset   = 0; /* Will be updated later */
    if (!add_instruction (pattern, group_start))
    {
      vibrex_free (sub_pattern);
      return false;
    }

    /* Copy the sub-pattern instructions (except the final MATCH) */
    for (size_t i = 0; i < sub_pattern->code_size - 1; i++)
    {
      if (!add_instruction (pattern, sub_pattern->code[i]))
      {
        vibrex_free (sub_pattern);
        return false;
      }
    }

    /* Add group end instruction */
    instruction_t group_end = {.op = OP_GROUP_END};
    group_end.data.offset   = group_start_pos;
    if (!add_instruction (pattern, group_end))
    {
      vibrex_free (sub_pattern);
      return false;
    }

    /* Update group start instruction with group length */
    pattern->code[group_start_pos].data.offset = pattern->code_size - group_start_pos - 1;

    vibrex_free (sub_pattern);
    *regex = scan; /* Move to the closing ) */
    return true;
  }

  /* No alternation - use simple group compilation */
  /* Add group start instruction */
  instruction_t group_start = {.op = OP_GROUP_START};
  size_t group_start_pos    = pattern->code_size;
  group_start.data.offset   = 0; /* Will be updated later with group length */
  if (!add_instruction (pattern, group_start))
    return false;

  /* Compile the content inside the group */
  size_t before = pattern->code_size;
  if (!compile_pattern (pattern, &p, depth))
    return false;
  size_t after = pattern->code_size;
  if (after == before)
    return false; /* Empty group is a syntax error */

  /* Add group end instruction */
  instruction_t group_end = {.op = OP_GROUP_END};
  group_end.data.offset   = group_start_pos; /* Point back to group start */
  if (!add_instruction (pattern, group_end))
    return false;

  /* Update group start instruction with group length */
  pattern->code[group_start_pos].data.offset = pattern->code_size - group_start_pos - 1;

  *regex = p;
  return true;
}

static bool
add_instruction (vibrex_t *pattern, instruction_t instr)
{
  if (pattern->code_size >= pattern->code_capacity)
  {
    size_t new_capacity     = pattern->code_capacity * 2;
    instruction_t *new_code = realloc (pattern->code, new_capacity * sizeof (instruction_t));
    if (!new_code)
      return false;
    pattern->code          = new_code;
    pattern->code_capacity = new_capacity;
  }

  pattern->code[pattern->code_size++] = instr;
  return true;
}

bool
vibrex_match (const vibrex_t *compiled_pattern, const char *text)
{
  if (!compiled_pattern || !text)
    return false;

  /* Use optimized alternation matching if available */
  if (compiled_pattern->has_alternation)
  {
    return match_alternations_optimized (compiled_pattern, text);
  }

  /* Optimization for .* patterns */
  if (compiled_pattern->has_dotstar && !compiled_pattern->anchored_start && !compiled_pattern->anchored_end)
  {
    // Extended: check for .*[literal] or .*[literal]$
    if (compiled_pattern->dotstar_literal && compiled_pattern->dotstar_literal_len > 0)
    {
      // Use memcmp instead of strstr for performance
      const char *haystack = text;
      size_t haystack_len  = 0;
      if (text)
      {
        const char *t = text;
        while (*t++)
          ++haystack_len;
      }
      size_t literal_len = compiled_pattern->dotstar_literal_len;
      if (haystack_len < literal_len)
        return false;
      size_t max_pos = haystack_len - literal_len + 1;
      for (size_t i = 0; i < max_pos; ++i)
      {
        if (memcmp (haystack + i, compiled_pattern->dotstar_literal, literal_len) == 0)
        {
          if (compiled_pattern->dotstar_anchored_end)
          {
            // Must be at end
            if (i + literal_len == haystack_len)
            {
              return true;
            }
          }
          else
          {
            return true;
          }
        }
      }
      return false;
    }
    return true;
  }

  /* If anchored at start, only try matching from beginning */
  if (compiled_pattern->anchored_start)
  {
    return match_here (compiled_pattern->code, text, 0) != NULL;
  }

  /* First character optimization */
  if (compiled_pattern->is_first_char_literal)
  {
    const char *p = text;
    while ((p = strchr (p, compiled_pattern->first_char)) != NULL)
    {
      if (match_here (compiled_pattern->code, p, 0))
      {
        return true;
      }
      p++;
    }
    return false;
  }

  /* Try matching at each position */
  const char *p = text;
  do
  {
    if (match_here (compiled_pattern->code, p, 0))
    {
      return true;
    }
  } while (*p++);

  return false;
}

static const char *
match_here (const instruction_t *code, const char *text, int pc)
{
  while (true)
  {
    switch (code[pc].op)
    {
    case OP_MATCH:
      return text;

    case OP_CHAR:
      /* Check if next instruction is a quantifier */
      if (code[pc + 1].op == OP_STAR)
      {
        return match_star (code, text, pc);
      }
      else if (code[pc + 1].op == OP_PLUS)
      {
        return match_plus (code, text, pc);
      }
      else if (code[pc + 1].op == OP_OPTIONAL)
      {
        return match_optional (code, text, pc, code[pc].data.ch);
      }
      /* Regular character match */
      if (*text != code[pc].data.ch)
        return NULL;
      text++;
      pc++;
      break;

    case OP_DOT:
      /* Check if next instruction is a quantifier */
      if (code[pc + 1].op == OP_STAR)
      {
        return match_star (code, text, pc);
      }
      else if (code[pc + 1].op == OP_PLUS)
      {
        return match_plus (code, text, pc);
      }
      else if (code[pc + 1].op == OP_OPTIONAL)
      {
        return match_optional (code, text, pc, 0);
      }
      /* Regular dot match */
      if (*text == '\0')
        return NULL;
      text++;
      pc++;
      break;

    case OP_CHARCLASS:
    {
      /* Check if next instruction is a quantifier */
      if (code[pc + 1].op == OP_STAR)
      {
        return match_star (code, text, pc);
      }
      else if (code[pc + 1].op == OP_PLUS)
      {
        return match_plus (code, text, pc);
      }
      else if (code[pc + 1].op == OP_OPTIONAL)
      {
        return match_optional (code, text, pc, 0);
      }
      /* Regular character class match */
      if (*text == '\0')
        return NULL;
      unsigned char ch = *text;
      if (!(code[pc].data.charclass.bitmap[ch / 8] & (1 << (ch % 8))))
      {
        return NULL;
      }
      text++;
      pc++;
      break;
    }

    case OP_GROUP_START:
    {
      /* Check if next instruction after group is a quantifier */
      int group_end_pc = pc + code[pc].data.offset;
      if (code[group_end_pc + 1].op == OP_STAR)
      {
        return match_group_star (code, text, pc);
      }
      else if (code[group_end_pc + 1].op == OP_PLUS)
      {
        return match_group_plus (code, text, pc);
      }
      else if (code[group_end_pc + 1].op == OP_OPTIONAL)
      {
        return match_optional_group (code, text, pc);
      }
      /* Regular group match - just continue into the group */
      pc++;
      break;
    }

    case OP_GROUP_END:
      /* End of group - continue after group */
      pc++;
      break;

    case OP_STAR:
    case OP_PLUS:
      /* These should be handled by the preceding character/dot/charclass/group */
      pc++;
      break;

    case OP_START:
      /* This should be handled at a higher level */
      pc++;
      break;

    case OP_END:
      return (*text == '\0') ? text : NULL;

    case OP_BRANCH:
      /* Branch instructions are not used in this implementation */
      return NULL;

    case OP_OPTIONAL:
      /* These should be handled by the preceding character/dot/charclass/group */
      pc++;
      break;

    default:
      return NULL;
    }
  }
}

static const char *
match_star (const instruction_t *code, const char *text, int pc)
{
  const char *p             = text;
  const instruction_t *atom = &code[pc];

  /* Greedily match as many characters as possible */
  while (*p && char_matches_atom (atom, *p))
  {
    p++;
  }

  /*
   * Backtrack from the longest match to find a position where the
   * rest of the pattern can match.
   */
  while (true)
  {
    const char *rest = match_here (code, p, pc + 2);
    if (rest)
    {
      return rest; /* Found a match for the rest of the pattern */
    }
    if (p == text)
    {
      break; /* Cannot backtrack further */
    }
    p--; /* Backtrack one character */
  }

  return NULL;
}

static const char *
match_plus (const instruction_t *code, const char *text, int pc)
{
  const char *p             = text;
  const instruction_t *atom = &code[pc];

  /* Must match at least once */
  if (!*p || !char_matches_atom (atom, *p))
  {
    return NULL;
  }
  p++;

  /* Greedily match as many additional characters as possible */
  while (*p && char_matches_atom (atom, *p))
  {
    p++;
  }

  /*
   * Backtrack from the longest match to find a position where the
   * rest of the pattern can match.
   */
  while (true)
  {
    const char *rest = match_here (code, p, pc + 2);
    if (rest)
    {
      return rest; /* Found a match */
    }
    if (p == text + 1)
    {
      /*
       * We've backtracked to the single character we mandatorily matched,
       * and it failed. We can't backtrack further.
       */
      break;
    }
    p--; /* Backtrack one character */
  }

  return NULL;
}

static const char *
match_optional (const instruction_t *code, const char *text, int pc, unsigned char ch)
{
  /* Try without matching the optional element first (zero matches) */
  const char *rest = match_here (code, text, pc + 2);
  if (rest)
    return rest;

  /* Try matching the optional element once */
  if (*text != '\0')
  {
    bool char_matches = false;

    switch (code[pc].op)
    {
    case OP_CHAR:
      char_matches = (*text == ch);
      break;
    case OP_DOT:
      char_matches = true;
      break;
    case OP_CHARCLASS:
    {
      unsigned char test_ch = *text;
      char_matches          = (code[pc].data.charclass.bitmap[test_ch / 8] & (1 << (test_ch % 8))) != 0;
      break;
    }
    default:
      char_matches = false;
      break;
    }

    if (char_matches)
    {
      return match_here (code, text + 1, pc + 2);
    }
  }

  return NULL;
}

static const char *
match_optional_group (const instruction_t *code, const char *text, int pc)
{
  /* Try without matching the optional group first (zero matches) */
  int group_end_pc = pc + code[pc].data.offset;
  const char *rest = match_here (code, text, group_end_pc + 2);
  if (rest)
    return rest;

  /* Try matching the optional group once */
  return match_here (code, text, pc + 1);
}

static const char *
match_group_star (const instruction_t *code, const char *text, int pc)
{
  int group_end_pc = pc + code[pc].data.offset;
  const char *p    = text;
  // Greedily match as many group repetitions as possible
  while (1)
  {
    const char *rest = match_here (code, p, group_end_pc + 2);
    if (rest)
      return rest; // rest of pattern matches
    // Try to match one more group
    const char *next = match_here (code, p, pc + 1);
    if (!next || next == p)
      break; // no more matches or zero-length match
    p = next;
  }
  return NULL;
}

static const char *
match_group_plus (const instruction_t *code, const char *text, int pc)
{
  int group_end_pc = pc + code[pc].data.offset;
  const char *p    = text;
  // Must match at least one group
  const char *first = match_here (code, p, pc + 1);
  if (!first || first == p)
    return NULL;
  p = first;
  // Greedily match as many group repetitions as possible
  while (1)
  {
    const char *rest = match_here (code, p, group_end_pc + 2);
    if (rest)
      return rest; // rest of pattern matches
    const char *next = match_here (code, p, pc + 1);
    if (!next || next == p)
      break;
    p = next;
  }
  return NULL;
}

void
vibrex_free (vibrex_t *compiled_pattern)
{
  if (compiled_pattern)
  {
    free (compiled_pattern->code);
    free (compiled_pattern->original_pattern);
    free_alternatives (compiled_pattern);
    free (compiled_pattern->common_prefix);
    free (compiled_pattern->common_suffix);
    vibrex_free (compiled_pattern->compiled_suffix);
    free (compiled_pattern->char_dispatch.char_list);
    free (compiled_pattern->dotstar_literal);
    free (compiled_pattern);
  }
}

static inline bool
char_matches_atom (const instruction_t *atom, unsigned char c)
{
  switch (atom->op)
  {
  case OP_CHAR:
    return c == atom->data.ch;
  case OP_DOT:
    return true;
  case OP_CHARCLASS:
    return (atom->data.charclass.bitmap[c / 8] & (1 << (c % 8))) != 0;
  default:
    return false;
  }
}

/* Alternation optimization implementations */
static bool
analyze_alternations (vibrex_t *pattern, int depth)
{
  const char *pat = pattern->original_pattern;
  size_t patlen   = strlen (pat);
  if (pat[0] == '|' || pat[patlen - 1] == '|' || strstr (pat, "||") != NULL)
  {
    return false; /* Alternation with empty alternative is a syntax error */
  }
  char *pattern_copy = malloc (strlen (pattern->original_pattern) + 1);
  if (!pattern_copy)
    return false;
  strcpy (pattern_copy, pattern->original_pattern);

  /* Count alternatives */
  size_t alt_count = 1;
  for (const char *p = pattern->original_pattern; *p; p++)
  {
    if (*p == '|')
      alt_count++;
  }

  /* Allocate alternatives array */
  pattern->alt_capacity = alt_count;
  pattern->alternatives = calloc (alt_count, sizeof (alternative_t));
  if (!pattern->alternatives)
  {
    free (pattern_copy);
    return false;
  }

  /* Parse alternatives */
  char *saveptr;
  char *alt          = strtok_r (pattern_copy, "|", &saveptr);
  pattern->alt_count = 0;

  while (alt && pattern->alt_count < alt_count)
  {
    /* Trim whitespace */
    while (*alt == ' ')
      alt++;
    size_t len = strlen (alt);
    while (len > 0 && alt[len - 1] == ' ')
      alt[--len] = '\0';

    if (len == 0)
    {
      free (pattern_copy);
      return false; /* Empty alternative is a syntax error */
    }

    pattern->alternatives[pattern->alt_count].pattern_text = malloc (len + 1);
    if (!pattern->alternatives[pattern->alt_count].pattern_text)
    {
      free (pattern_copy);
      return false;
    }
    strcpy (pattern->alternatives[pattern->alt_count].pattern_text, alt);
    pattern->alternatives[pattern->alt_count].pattern_len = len;
    pattern->alt_count++;

    alt = strtok_r (NULL, "|", &saveptr);
  }

  free (pattern_copy);

  /* Check if all alternatives are anchored at the start */
  bool all_are_start_anchored = pattern->alt_count > 0;
  if (all_are_start_anchored)
  {
    for (size_t i = 0; i < pattern->alt_count; i++)
    {
      if (pattern->alternatives[i].pattern_len == 0 || pattern->alternatives[i].pattern_text[0] != '^')
      {
        all_are_start_anchored = false;
        break;
      }
    }
  }

  if (all_are_start_anchored)
  {
    pattern->anchored_start = true;
    for (size_t i = 0; i < pattern->alt_count; i++)
    {
      char *p_text = pattern->alternatives[i].pattern_text;
      size_t p_len = pattern->alternatives[i].pattern_len;
      memmove (p_text, p_text + 1, p_len); /* p_len is strlen, so moves null term */
      pattern->alternatives[i].pattern_len--;
    }
  }

  /* Try character dispatch optimization first */
  if (build_char_dispatch (pattern))
  {
    return true;
  }

  /* Find common prefix if character dispatch not applicable */
  pattern->common_prefix_len = find_common_prefix (pattern);
  if (pattern->common_prefix_len > 0)
  {
    pattern->first_char            = pattern->common_prefix[0];
    pattern->is_first_char_literal = true;
  }

  /* Find common suffix */
  pattern->common_suffix_len = find_common_suffix (pattern);
  if (pattern->common_suffix_len > 0)
  {
    if (pattern->common_prefix_len + pattern->common_suffix_len >= pattern->alternatives[0].pattern_len)
    {
      pattern->common_suffix_len = 0; /* Avoid overlap */
    }
  }

  if (pattern->common_suffix_len > 0)
  {
    const char *first_alt_text = pattern->alternatives[0].pattern_text;
    size_t first_alt_len       = pattern->alternatives[0].pattern_len;
    const char *suffix_text    = first_alt_text + first_alt_len - pattern->common_suffix_len;

    char *suffix_pattern_str = malloc (pattern->common_suffix_len + 1);
    if (!suffix_pattern_str)
      return false;
    strncpy (suffix_pattern_str, suffix_text, pattern->common_suffix_len);
    suffix_pattern_str[pattern->common_suffix_len] = '\0';

    pattern->common_suffix   = suffix_pattern_str;
    pattern->compiled_suffix = compile_simple_regex (pattern->common_suffix, pattern->anchored_end, depth);

    if (!pattern->compiled_suffix)
      return false;
  }

  /* Pre-compile all alternatives */
  return precompile_alternatives (pattern, depth);
}

static bool
build_char_dispatch (vibrex_t *pattern)
{
  /* Check if all alternatives are single characters */
  bool all_single_chars = true;
  for (size_t i = 0; i < pattern->alt_count; i++)
  {
    if (!is_single_char_pattern (pattern->alternatives[i].pattern_text))
    {
      all_single_chars = false;
      break;
    }
  }

  if (!all_single_chars)
  {
    return false;
  }

  /* Build character dispatch table */
  pattern->char_dispatch.is_single_char_dispatch = true;
  pattern->char_dispatch.char_count              = pattern->alt_count;
  pattern->char_dispatch.char_list               = malloc (pattern->alt_count);
  if (!pattern->char_dispatch.char_list)
  {
    return false;
  }

  for (size_t i = 0; i < pattern->alt_count; i++)
  {
    char ch                                              = get_single_char_from_pattern (pattern->alternatives[i].pattern_text);
    pattern->char_dispatch.char_table[(unsigned char)ch] = true;
    pattern->char_dispatch.char_list[i]                  = ch;
  }

  return true;
}

static size_t
find_common_prefix (vibrex_t *pattern)
{
  if (pattern->alt_count < 2)
    return 0;

  size_t prefix_len = 0;
  size_t min_len    = pattern->alternatives[0].pattern_len;

  /* Find minimum length */
  for (size_t i = 1; i < pattern->alt_count; i++)
  {
    if (pattern->alternatives[i].pattern_len < min_len)
    {
      min_len = pattern->alternatives[i].pattern_len;
    }
  }

  /* Find common prefix length, but be smart about regex syntax */
  for (size_t pos = 0; pos < min_len; pos++)
  {
    char ch        = pattern->alternatives[0].pattern_text[pos];
    bool all_match = true;

    /* Skip regex metacharacters as they don't make good common prefixes */
    if (ch == '[' || ch == '(' || ch == '.' || ch == '*' ||
        ch == '+' || ch == '?' || ch == '^' || ch == '$' || ch == '|')
    {
      break;
    }

    for (size_t i = 1; i < pattern->alt_count; i++)
    {
      if (pattern->alternatives[i].pattern_text[pos] != ch)
      {
        all_match = false;
        break;
      }
    }

    if (!all_match)
      break;
    prefix_len++;
  }

  /* Only store prefix if it's meaningful (more than just single chars or short) */
  if (prefix_len >= 2)
  {
    pattern->common_prefix = malloc (prefix_len + 1);
    if (pattern->common_prefix)
    {
      strncpy (pattern->common_prefix, pattern->alternatives[0].pattern_text, prefix_len);
      pattern->common_prefix[prefix_len] = '\0';
    }
  }
  else
  {
    prefix_len = 0; /* Don't use short prefixes */
  }

  return prefix_len;
}

static size_t
find_common_suffix (vibrex_t *pattern)
{
  if (pattern->alt_count < 2)
    return 0;

  size_t min_len = (size_t)-1;
  for (size_t i = 0; i < pattern->alt_count; i++)
  {
    size_t effective_len = pattern->alternatives[i].pattern_len - pattern->common_prefix_len;
    if (effective_len < min_len)
    {
      min_len = effective_len;
    }
  }

  if (min_len == (size_t)-1 || min_len == 0)
    return 0;

  size_t suffix_len = 0;
  for (size_t i = 1; i <= min_len; i++)
  {
    const char *alt0_text = pattern->alternatives[0].pattern_text;
    size_t alt0_len       = pattern->alternatives[0].pattern_len;
    char ch               = alt0_text[alt0_len - i];

    bool all_match = true;
    for (size_t j = 1; j < pattern->alt_count; j++)
    {
      const char *alt_text = pattern->alternatives[j].pattern_text;
      size_t alt_len       = pattern->alternatives[j].pattern_len;
      if (alt_text[alt_len - i] != ch)
      {
        all_match = false;
        break;
      }
    }

    if (!all_match)
      break;
    suffix_len++;
  }

  if (suffix_len > 0)
  {
    const char *alt0_text     = pattern->alternatives[0].pattern_text;
    size_t alt0_len           = pattern->alternatives[0].pattern_len;
    char first_char_of_suffix = alt0_text[alt0_len - suffix_len];

    /*
     * Heuristic: If a short common suffix is just a closing bracket/paren,
     * it's likely we're incorrectly splitting an atom like `[a-z]`.
     * This prevents compilation errors for patterns like `[0-9]|[a-z]`.
     */
    if (suffix_len == 1 && (first_char_of_suffix == ']' || first_char_of_suffix == ')'))
    {
      return 0;
    }
  }

  return suffix_len;
}

static bool
precompile_alternatives (vibrex_t *pattern, int depth)
{
  for (size_t i = 0; i < pattern->alt_count; i++)
  {
    const char *alt_full_pattern = pattern->alternatives[i].pattern_text;
    size_t alt_full_len          = pattern->alternatives[i].pattern_len;

    size_t middle_len       = alt_full_len - pattern->common_prefix_len - pattern->common_suffix_len;
    const char *middle_text = alt_full_pattern + pattern->common_prefix_len;

    char *middle_pattern_str = malloc (middle_len + 1);
    if (!middle_pattern_str)
      return false;
    strncpy (middle_pattern_str, middle_text, middle_len);
    middle_pattern_str[middle_len] = '\0';

    /* An alternative can be empty after stripping prefix/suffix */
    if (strlen (middle_pattern_str) == 0)
    {
      pattern->alternatives[i].compiled = compile_simple_regex ("", false, depth);
    }
    else
    {
      pattern->alternatives[i].compiled = compile_simple_regex (middle_pattern_str, false, depth);
    }

    free (middle_pattern_str);
    if (!pattern->alternatives[i].compiled)
      return false;
  }
  return true;
}

static vibrex_t *
compile_simple_regex (const char *pattern_str, bool anchored_end, int depth)
{
  vibrex_t *compiled = malloc (sizeof (vibrex_t));
  if (!compiled)
    return NULL;

  compiled->code_capacity = 64;
  compiled->code          = malloc (compiled->code_capacity * sizeof (instruction_t));
  if (!compiled->code)
  {
    free (compiled);
    return NULL;
  }

  compiled->code_size                             = 0;
  compiled->anchored_start                        = false;
  compiled->anchored_end                          = anchored_end;
  compiled->has_dotstar                           = false;
  compiled->has_alternation                       = false;
  compiled->alternatives                          = NULL;
  compiled->alt_count                             = 0;
  compiled->alt_capacity                          = 0;
  compiled->char_dispatch.is_single_char_dispatch = false;
  compiled->char_dispatch.char_list               = NULL;
  compiled->char_dispatch.char_count              = 0;
  compiled->common_prefix                         = NULL;
  compiled->common_prefix_len                     = 0;
  compiled->common_suffix                         = NULL;
  compiled->common_suffix_len                     = 0;
  compiled->compiled_suffix                       = NULL;
  memset (compiled->char_dispatch.char_table, 0, sizeof (compiled->char_dispatch.char_table));
  compiled->first_char            = '\0';
  compiled->is_first_char_literal = false;
  compiled->dotstar_literal       = NULL;
  compiled->dotstar_literal_len   = 0;
  compiled->dotstar_anchored_end  = false;

  compiled->original_pattern = malloc (strlen (pattern_str) + 1);
  if (!compiled->original_pattern)
  {
    vibrex_free (compiled);
    return NULL;
  }
  strcpy (compiled->original_pattern, pattern_str);

  const char *p = pattern_str;

  if (*p == '^')
  {
    compiled->anchored_start = true;
    p++;
  }

  if (!compile_pattern (compiled, &p, depth))
  {
    vibrex_free (compiled);
    return NULL;
  }

  if (*p == '$' || anchored_end)
  {
    if (*p == '$')
      p++; /* Consume character if present */
    compiled->anchored_end = true;
    if (compiled->code_size == 0 || compiled->code[compiled->code_size - 1].op != OP_END)
    {
      instruction_t end_instr = {.op = OP_END};
      if (!add_instruction (compiled, end_instr))
      {
        vibrex_free (compiled);
        return NULL;
      }
    }
  }

  instruction_t match_instr = {.op = OP_MATCH};
  if (!add_instruction (compiled, match_instr))
  {
    vibrex_free (compiled);
    return NULL;
  }

  if (compiled->code_size >= 3 &&
      compiled->code[0].op == OP_DOT &&
      compiled->code[1].op == OP_STAR)
  {
    compiled->has_dotstar = true;
    // Extended: check for .*[literal] or .*[literal]$
    size_t i             = 2;
    size_t literal_start = i;
    while (i < compiled->code_size - 1 && compiled->code[i].op == OP_CHAR)
    {
      i++;
    }
    bool ends_with_end = (i < compiled->code_size - 1 && compiled->code[i].op == OP_END);
    if ((i > literal_start) && (i == compiled->code_size - 1 || (ends_with_end && i == compiled->code_size - 2)))
    {
      // Found .*[literal] or .*[literal]$
      size_t literal_len        = i - literal_start;
      compiled->dotstar_literal = malloc (literal_len + 1);
      if (compiled->dotstar_literal)
      {
        for (size_t j = 0; j < literal_len; ++j)
        {
          compiled->dotstar_literal[j] = compiled->code[literal_start + j].data.ch;
        }
        compiled->dotstar_literal[literal_len] = '\0';
        compiled->dotstar_literal_len          = literal_len;
        compiled->dotstar_anchored_end         = ends_with_end;
      }
    }
  }

  /* First character optimization: check if first instruction is a literal char */
  if (!compiled->anchored_start && compiled->code_size > 0 && compiled->code[0].op == OP_CHAR)
  {
    if (compiled->code_size < 2 || (compiled->code[1].op != OP_STAR && compiled->code[1].op != OP_OPTIONAL))
    {
      compiled->first_char            = compiled->code[0].data.ch;
      compiled->is_first_char_literal = true;
    }
  }

  return compiled;
}

static void
free_alternatives (vibrex_t *pattern)
{
  if (pattern->alternatives)
  {
    for (size_t i = 0; i < pattern->alt_count; i++)
    {
      if (pattern->alternatives[i].compiled)
      {
        vibrex_free (pattern->alternatives[i].compiled);
      }
      free (pattern->alternatives[i].pattern_text);
    }
    free (pattern->alternatives);
    pattern->alternatives = NULL;
  }
}

static bool
is_single_char_pattern (const char *pattern)
{
  if (!pattern || strlen (pattern) != 1)
    return false;
  /* Check for special regex characters */
  char ch = pattern[0];
  return ch != '.' && ch != '*' && ch != '+' && ch != '?' &&
         ch != '^' && ch != '$' && ch != '[' && ch != '(' && ch != '|';
}

static char
get_single_char_from_pattern (const char *pattern)
{
  return pattern[0]; /* Assumes is_single_char_pattern returned true */
}

static bool
match_alternations_optimized (const vibrex_t *pattern, const char *text)
{
  /* Fast character dispatch for single character alternatives */
  if (pattern->char_dispatch.is_single_char_dispatch)
  {
    if (pattern->anchored_start)
    {
      return *text && pattern->char_dispatch.char_table[(unsigned char)*text];
    }
    /* Check if text contains any of the alternative characters */
    for (const char *p = text; *p; p++)
    {
      if (pattern->char_dispatch.char_table[(unsigned char)*p])
      {
        return true;
      }
    }
    return false;
  }

  /* Handle globally anchored alternations */
  if (pattern->anchored_start)
  {
    const char *match_text = text;
    if (pattern->common_prefix_len > 0)
    {
      if (strncmp (text, pattern->common_prefix, pattern->common_prefix_len) != 0)
      {
        return false;
      }
      match_text += pattern->common_prefix_len;
    }

    for (size_t i = 0; i < pattern->alt_count; i++)
    {
      if (pattern->alternatives[i].compiled)
      {
        const vibrex_t *alt = pattern->alternatives[i].compiled;
        const char *middle_match_end = match_here (alt->code, match_text, 0);

        if (middle_match_end)
        {
          /* Middle part matched, now try suffix */
          if (pattern->compiled_suffix)
          {
            if (match_here (pattern->compiled_suffix->code, middle_match_end, 0))
            {
              return true;
            }
          }
          else if (alt->anchored_end)
          {
            if (*middle_match_end == '\0')
              return true;
          }
          else
          {
            return true;
          }
        }
      }
    }
    return false;
  }

  /* Handle common prefix optimization */
  if (pattern->common_prefix_len > 0)
  {
    /* Search for prefix matches */
    const char *p = text;
    while ((p = strstr (p, pattern->common_prefix)) != NULL)
    {
      const char *match_text = p + pattern->common_prefix_len;

      for (size_t i = 0; i < pattern->alt_count; i++)
      {
        if (pattern->alternatives[i].compiled)
        {
          const vibrex_t *alt          = pattern->alternatives[i].compiled;
          const char *middle_match_end = match_here (alt->code, match_text, 0);

          if (middle_match_end)
          {
            /* Middle part matched, now try suffix */
            if (pattern->compiled_suffix)
            {
              if (match_here (pattern->compiled_suffix->code, middle_match_end, 0))
              {
                return true;
              }
            }
            else if (alt->anchored_end)
            {
              if (*middle_match_end == '\0')
                return true;
            }
            else
            {
              return true;
            }
          }
        }
      }
      p++; /* Continue searching from the next character */
    }
    return false;
  }

  /* No common prefix - try each alternative respecting their anchor constraints */
  for (size_t i = 0; i < pattern->alt_count; i++)
  {
    if (!pattern->alternatives[i].compiled)
      continue;

    const vibrex_t *alt = pattern->alternatives[i].compiled;
    const char *p       = text;

    if (alt->anchored_start)
    {
      const char *middle_match_end = match_here (alt->code, p, 0);
      if (middle_match_end)
      {
        if (pattern->compiled_suffix)
        {
          const char *final_match_end = match_here (pattern->compiled_suffix->code, middle_match_end, 0);
          if (final_match_end)
          {
            if (!alt->anchored_end || *final_match_end == '\0')
            {
              return true;
            }
          }
        }
        else if (alt->anchored_end)
        {
          if (*middle_match_end == '\0')
            return true;
        }
        else
        {
          return true;
        }
      }
    }
    else
    {
      /* First-char optimization for individual alternatives */
      if (alt->is_first_char_literal)
      {
        const char *current_pos = p;
        while ((current_pos = strchr (current_pos, alt->first_char)) != NULL)
        {
          const char *middle_match_end = match_here (alt->code, current_pos, 0);
          if (middle_match_end)
          {
            if (pattern->compiled_suffix)
            {
              const char *final_match_end = match_here (pattern->compiled_suffix->code, middle_match_end, 0);
              if (final_match_end)
              {
                if (alt->anchored_end)
                {
                  if (*final_match_end == '\0')
                    return true;
                }
                else
                {
                  return true;
                }
              }
            }
            else if (alt->anchored_end)
            {
              if (*middle_match_end == '\0')
                return true;
            }
            else
            {
              return true;
            }
          }
          current_pos++;
        }
      }
      else
      {
        do
        {
          const char *middle_match_end = match_here (alt->code, p, 0);
          if (middle_match_end)
          {
            if (pattern->compiled_suffix)
            {
              const char *final_match_end = match_here (pattern->compiled_suffix->code, middle_match_end, 0);
              if (final_match_end)
              {
                /* If the whole pattern is anchored, the suffix match must be at the end */
                if (alt->anchored_end)
                {
                  if (*final_match_end == '\0')
                    return true;
                }
                else
                {
                  return true;
                }
              }
            }
            else if (alt->anchored_end)
            {
              if (*middle_match_end == '\0')
                return true;
            }
            else
            {
              return true;
            }
          }
        } while (*p++);
      }
    }
  }

  return false;
}