/*
 * rexx_errtext.h - the ARexx error texts (Phase 250), shared by the Rexxc
 * commands and the interpreter; plain C, no AmigaOS headers.
 *
 * Copyright (C) 2026 by G. Bartsch. Licensed under the MIT License.
 */
#ifndef LXA_REXX_ERRTEXT_H
#define LXA_REXX_ERRTEXT_H

/* ARexx error texts (ERRORTEXT(), RX, the "+++ Error" messages), as on
 * ARexx 1.15; empty strings are unused numbers */
static const char *const lxa_rexx_errtext[] __attribute__((unused)) = {
    "Undiagnosed internal error",           /*  0 */
    "Program not found",
    "Execution halted",
    "Insufficient memory",
    "",
    "Unmatched quote",                      /*  5 */
    "Unterminated comment",
    "",
    "Unrecognized token",
    "Symbol or string >65535 characters",
    "Invalid message packet",               /* 10 */
    "Command string error",
    "Error return from function",
    "Host environment not found",
    "Requested library not found",
    "Function not found",                   /* 15 */
    "Function did not return value",
    "Wrong number of arguments",
    "Invalid argument to function",
    "Invalid PROCEDURE",
    "Unexpected THEN or WHEN",              /* 20 */
    "Unexpected ELSE or OTHERWISE",
    "Unexpected BREAK, LEAVE or ITERATE",
    "Invalid statement in SELECT",
    "Missing or multiple THEN",
    "Missing OTHERWISE",                    /* 25 */
    "Missing or unexpected END",
    "Symbol mismatch",
    "Invalid DO syntax",
    "Incomplete IF or SELECT",
    "Label not found",                      /* 30 */
    "Symbol expected",
    "Symbol or string expected",
    "Invalid keyword",
    "Required keyword missing",
    "Extraneous characters",                /* 35 */
    "Keyword conflict",
    "Invalid template",
    "",
    "Uninitialized variable",
    "Invalid variable name",                /* 40 */
    "Invalid expression",
    "Unbalanced parentheses",
    "Expression nesting >32",
    "Invalid expression result",
    "Expression required",                  /* 45 */
    "Boolean value not 0 or 1",
    "Arithmetic conversion error",
    "Invalid operand",
};

#define LXA_REXX_NUMERR ((long)(sizeof(lxa_rexx_errtext) / sizeof(lxa_rexx_errtext[0])))

static inline const char *lxa_rexx_error_text(long code)
{
    if (code < 0 || code >= LXA_REXX_NUMERR)
        return lxa_rexx_errtext[0];
    return lxa_rexx_errtext[code];
}

#endif
