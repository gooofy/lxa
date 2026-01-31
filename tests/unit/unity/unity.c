/* =========================================================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-2021 Mike Karlesky, Mark VanderVoord, Greg Williams
    [SPDX-License-Identifier: MIT]
    
    Lightweight implementation for lxa project
========================================================================= */

#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

/*-------------------------------------------------------
 * Global Storage for Unity
 *-------------------------------------------------------*/

struct UNITY_STORAGE_T Unity;

/*-------------------------------------------------------
 * Internal Helpers
 *-------------------------------------------------------*/

static const char* UnityStrNull = "NULL";

static void UnityPrintChar(const char c)
{
    putchar(c);
}

static void UnityPrint(const char* string)
{
    if (string == NULL)
    {
        printf("(null)");
        return;
    }
    printf("%s", string);
}

static void UnityPrintNumber(const UNITY_INT number)
{
    printf("%d", (int)number);
}

static void UnityPrintNumberUnsigned(const UNITY_UINT number)
{
    printf("%u", (unsigned int)number);
}

static void UnityPrintNumberHex(const UNITY_UINT number, const char nibbles)
{
    switch (nibbles)
    {
        case 2:  printf("0x%02X", (unsigned int)(number & 0xFF)); break;
        case 4:  printf("0x%04X", (unsigned int)(number & 0xFFFF)); break;
        default: printf("0x%08X", (unsigned int)number); break;
    }
}

static void UnityPrintMask(const UNITY_UINT mask, const UNITY_UINT number)
{
    printf("0x%08X", (unsigned int)(mask & number));
}

static void UnityPrintNumberByStyle(const UNITY_INT number, const UNITY_DISPLAY_STYLE_T style)
{
    switch (style)
    {
        case UNITY_DISPLAY_STYLE_INT:
            UnityPrintNumber(number);
            break;
        case UNITY_DISPLAY_STYLE_UINT:
            UnityPrintNumberUnsigned((UNITY_UINT)number);
            break;
        case UNITY_DISPLAY_STYLE_HEX8:
            UnityPrintNumberHex((UNITY_UINT)number, 2);
            break;
        case UNITY_DISPLAY_STYLE_HEX16:
            UnityPrintNumberHex((UNITY_UINT)number, 4);
            break;
        case UNITY_DISPLAY_STYLE_HEX32:
        case UNITY_DISPLAY_STYLE_POINTER:
            UnityPrintNumberHex((UNITY_UINT)number, 8);
            break;
        default:
            printf("Unknown Style: %d", (int)style);
            break;
    }
}

static void UnityTestResultsBegin(const char* file, const unsigned int line)
{
    printf("%s:%u:%s:", file, line, Unity.CurrentTestName);
}

static void UnityTestResultsFailBegin(const unsigned int line)
{
    UnityTestResultsBegin(Unity.TestFile, line);
    printf("FAIL:");
}

static void UnityPrintExpectedAndActualStrings(const char* expected, const char* actual)
{
    printf(" Expected ");
    if (expected != NULL)
    {
        printf("'");
        printf("%s", expected);
        printf("'");
    }
    else
    {
        printf("NULL");
    }
    printf(" Was ");
    if (actual != NULL)
    {
        printf("'");
        printf("%s", actual);
        printf("'");
    }
    else
    {
        printf("NULL");
    }
}

/*-------------------------------------------------------
 * Test Control
 *-------------------------------------------------------*/

void UnityBegin(const char* filename)
{
    Unity.TestFile = filename;
    Unity.CurrentTestName = NULL;
    Unity.CurrentTestLineNumber = 0;
    Unity.NumberOfTests = 0;
    Unity.TestFailures = 0;
    Unity.TestIgnores = 0;
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

int UnityEnd(void)
{
    printf("\n");
    printf("-----------------------\n");
    printf("%u Tests %u Failures %u Ignored\n", 
           (unsigned int)Unity.NumberOfTests,
           (unsigned int)Unity.TestFailures,
           (unsigned int)Unity.TestIgnores);
    
    if (Unity.TestFailures == 0U)
    {
        printf("OK\n");
    }
    else
    {
        printf("FAIL\n");
    }
    
    return (int)(Unity.TestFailures);
}

void UnityConcludeTest(void)
{
    if (Unity.CurrentTestIgnored)
    {
        Unity.TestIgnores++;
    }
    else if (Unity.CurrentTestFailed)
    {
        Unity.TestFailures++;
    }
    else
    {
        printf("%s:%u:%s:PASS\n", Unity.TestFile, Unity.CurrentTestLineNumber, Unity.CurrentTestName);
    }
    
    Unity.CurrentTestFailed = 0;
    Unity.CurrentTestIgnored = 0;
}

void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum)
{
    Unity.CurrentTestName = FuncName;
    Unity.CurrentTestLineNumber = (UNITY_UINT)FuncLineNum;
    Unity.NumberOfTests++;
    if (TEST_PROTECT())
    {
        Func();
    }
    UnityConcludeTest();
}

/*-------------------------------------------------------
 * Assertion Functions
 *-------------------------------------------------------*/

void UnityFail(const char* msg, const unsigned int line)
{
    Unity.CurrentTestFailed = 1;
    UnityTestResultsFailBegin(line);
    if (msg != NULL)
    {
        printf(" ");
        printf("%s", msg);
    }
    printf("\n");
}

void UnityIgnore(const char* msg, const unsigned int line)
{
    Unity.CurrentTestIgnored = 1;
    UnityTestResultsBegin(Unity.TestFile, line);
    printf("IGNORE");
    if (msg != NULL)
    {
        printf(": ");
        printf("%s", msg);
    }
    printf("\n");
}

void UnityAssertEqualNumber(const UNITY_INT expected,
                            const UNITY_INT actual,
                            const char* msg,
                            const unsigned int lineNumber,
                            const UNITY_DISPLAY_STYLE_T style)
{
    if (expected != actual)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Expected ");
        UnityPrintNumberByStyle(expected, style);
        printf(" Was ");
        UnityPrintNumberByStyle(actual, style);
        if (msg != NULL)
        {
            printf(": ");
            printf("%s", msg);
        }
        printf("\n");
    }
}

void UnityAssertEqualIntArray(const UNITY_INT* expected,
                              const UNITY_INT* actual,
                              const UNITY_UINT32 num_elements,
                              const char* msg,
                              const unsigned int lineNumber,
                              const UNITY_DISPLAY_STYLE_T style)
{
    UNITY_UINT32 i;
    
    if (expected == NULL || actual == NULL)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" NULL pointer ");
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
        return;
    }
    
    for (i = 0; i < num_elements; i++)
    {
        if (expected[i] != actual[i])
        {
            Unity.CurrentTestFailed = 1;
            UnityTestResultsFailBegin(lineNumber);
            printf(" Element %u: Expected ", (unsigned int)i);
            UnityPrintNumberByStyle(expected[i], style);
            printf(" Was ");
            UnityPrintNumberByStyle(actual[i], style);
            if (msg != NULL)
            {
                printf(": %s", msg);
            }
            printf("\n");
            return;
        }
    }
}

void UnityAssertBits(const UNITY_INT mask,
                     const UNITY_INT expected,
                     const UNITY_INT actual,
                     const char* msg,
                     const unsigned int lineNumber)
{
    if ((mask & expected) != (mask & actual))
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Expected ");
        UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)expected);
        printf(" Was ");
        UnityPrintMask((UNITY_UINT)mask, (UNITY_UINT)actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const unsigned int lineNumber)
{
    if (expected == NULL && actual == NULL) return;
    
    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        UnityPrintExpectedAndActualStrings(expected, actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertEqualStringLen(const char* expected,
                               const char* actual,
                               const UNITY_UINT32 length,
                               const char* msg,
                               const unsigned int lineNumber)
{
    if (expected == NULL && actual == NULL) return;
    
    if (expected == NULL || actual == NULL || strncmp(expected, actual, length) != 0)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        UnityPrintExpectedAndActualStrings(expected, actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertEqualMemory(const void* expected,
                            const void* actual,
                            const UNITY_UINT32 length,
                            const UNITY_UINT32 num_elements,
                            const char* msg,
                            const unsigned int lineNumber)
{
    if (expected == NULL && actual == NULL) return;
    
    if (expected == NULL || actual == NULL)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" NULL pointer");
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
        return;
    }
    
    if (memcmp(expected, actual, length * num_elements) != 0)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Memory mismatch");
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertNumbersWithin(const UNITY_UINT delta,
                              const UNITY_INT expected,
                              const UNITY_INT actual,
                              const char* msg,
                              const unsigned int lineNumber,
                              const UNITY_DISPLAY_STYLE_T style)
{
    UNITY_INT diff = expected - actual;
    if (diff < 0) diff = -diff;
    
    if ((UNITY_UINT)diff > delta)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Values not within delta=%u ", (unsigned int)delta);
        printf(" Expected ");
        UnityPrintNumberByStyle(expected, style);
        printf(" Was ");
        UnityPrintNumberByStyle(actual, style);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertFloatsWithin(const float delta,
                             const float expected,
                             const float actual,
                             const char* msg,
                             const unsigned int lineNumber)
{
    float diff = expected - actual;
    if (diff < 0) diff = -diff;
    
    if (diff > delta)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Expected %f +/- %f Was %f", (double)expected, (double)delta, (double)actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertDoublesWithin(const double delta,
                              const double expected,
                              const double actual,
                              const char* msg,
                              const unsigned int lineNumber)
{
    double diff = expected - actual;
    if (diff < 0) diff = -diff;
    
    if (diff > delta)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Expected %f +/- %f Was %f", expected, delta, actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}

void UnityAssertFloatSpecial(const float actual,
                             const char* msg,
                             const unsigned int lineNumber,
                             const UNITY_UINT style)
{
    /* Simplified: just check for NaN/Inf */
    int is_nan = isnan(actual);
    int is_inf = isinf(actual);
    
    /* style: 0 = expect NaN, 1 = expect +Inf, 2 = expect -Inf, etc. */
    (void)style; /* suppress warning */
    
    if (!is_nan && !is_inf)
    {
        Unity.CurrentTestFailed = 1;
        UnityTestResultsFailBegin(lineNumber);
        printf(" Expected special float, got %f", (double)actual);
        if (msg != NULL)
        {
            printf(": %s", msg);
        }
        printf("\n");
    }
}
