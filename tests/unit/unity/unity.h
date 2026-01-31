/* =========================================================================
    Unity Project - A Test Framework for C
    Copyright (c) 2007-2021 Mike Karlesky, Mark VanderVoord, Greg Williams
    [SPDX-License-Identifier: MIT]

    Lightweight version for lxa project
========================================================================= */

#ifndef UNITY_H
#define UNITY_H

#define UNITY_VERSION_MAJOR    2
#define UNITY_VERSION_MINOR    5
#define UNITY_VERSION_BUILD    2
#define UNITY_VERSION          ((UNITY_VERSION_MAJOR << 16) | (UNITY_VERSION_MINOR << 8) | UNITY_VERSION_BUILD)

#include <stddef.h>
#include <stdint.h>

/*-------------------------------------------------------
 * Configuration Options
 *-------------------------------------------------------*/

/* Define to customize maximum message length */
#ifndef UNITY_MAX_MESSAGE_LENGTH
#define UNITY_MAX_MESSAGE_LENGTH 512
#endif

/*-------------------------------------------------------
 * Basic Type Definitions
 *-------------------------------------------------------*/

typedef int64_t  UNITY_INT64;
typedef uint64_t UNITY_UINT64;
typedef int32_t  UNITY_INT32;
typedef uint32_t UNITY_UINT32;
typedef int16_t  UNITY_INT16;
typedef uint16_t UNITY_UINT16;
typedef int8_t   UNITY_INT8;
typedef uint8_t  UNITY_UINT8;

typedef UNITY_INT32  UNITY_INT;
typedef UNITY_UINT32 UNITY_UINT;

/*-------------------------------------------------------
 * Internal Structures
 *-------------------------------------------------------*/

typedef void (*UnityTestFunction)(void);

typedef enum {
    UNITY_DISPLAY_STYLE_INT = 0,
    UNITY_DISPLAY_STYLE_UINT,
    UNITY_DISPLAY_STYLE_HEX8,
    UNITY_DISPLAY_STYLE_HEX16,
    UNITY_DISPLAY_STYLE_HEX32,
    UNITY_DISPLAY_STYLE_POINTER
} UNITY_DISPLAY_STYLE_T;

struct UNITY_STORAGE_T
{
    const char* TestFile;
    const char* CurrentTestName;
    UNITY_UINT  CurrentTestLineNumber;
    UNITY_UINT  NumberOfTests;
    UNITY_UINT  TestFailures;
    UNITY_UINT  TestIgnores;
    UNITY_UINT  CurrentTestFailed;
    UNITY_UINT  CurrentTestIgnored;
};

extern struct UNITY_STORAGE_T Unity;

/*-------------------------------------------------------
 * Test Suite Functions
 *-------------------------------------------------------*/

void UnityBegin(const char* filename);
int  UnityEnd(void);
void UnityConcludeTest(void);
void UnityDefaultTestRun(UnityTestFunction Func, const char* FuncName, const int FuncLineNum);

/*-------------------------------------------------------
 * Assertion Internals
 *-------------------------------------------------------*/

void UnityAssertEqualNumber(const UNITY_INT expected,
                            const UNITY_INT actual,
                            const char* msg,
                            const unsigned int lineNumber,
                            const UNITY_DISPLAY_STYLE_T style);

void UnityAssertEqualIntArray(const UNITY_INT* expected,
                              const UNITY_INT* actual,
                              const UNITY_UINT32 num_elements,
                              const char* msg,
                              const unsigned int lineNumber,
                              const UNITY_DISPLAY_STYLE_T style);

void UnityAssertBits(const UNITY_INT mask,
                     const UNITY_INT expected,
                     const UNITY_INT actual,
                     const char* msg,
                     const unsigned int lineNumber);

void UnityAssertEqualString(const char* expected,
                            const char* actual,
                            const char* msg,
                            const unsigned int lineNumber);

void UnityAssertEqualStringLen(const char* expected,
                               const char* actual,
                               const UNITY_UINT32 length,
                               const char* msg,
                               const unsigned int lineNumber);

void UnityAssertEqualMemory(const void* expected,
                            const void* actual,
                            const UNITY_UINT32 length,
                            const UNITY_UINT32 num_elements,
                            const char* msg,
                            const unsigned int lineNumber);

void UnityAssertNumbersWithin(const UNITY_UINT delta,
                              const UNITY_INT expected,
                              const UNITY_INT actual,
                              const char* msg,
                              const unsigned int lineNumber,
                              const UNITY_DISPLAY_STYLE_T style);

void UnityFail(const char* message, const unsigned int line);
void UnityIgnore(const char* message, const unsigned int line);

void UnityAssertFloatsWithin(const float delta,
                             const float expected,
                             const float actual,
                             const char* msg,
                             const unsigned int lineNumber);

void UnityAssertDoublesWithin(const double delta,
                              const double expected,
                              const double actual,
                              const char* msg,
                              const unsigned int lineNumber);

void UnityAssertFloatSpecial(const float actual,
                             const char* msg,
                             const unsigned int lineNumber,
                             const UNITY_UINT style);

/*-------------------------------------------------------
 * Test Setup/Teardown Macros
 *-------------------------------------------------------*/

#define UNITY_BEGIN() UnityBegin(__FILE__)
#define UNITY_END()   UnityEnd()

#define TEST_PROTECT() 1
#define TEST_ABORT() return

/*-------------------------------------------------------
 * Run Test Macro
 *-------------------------------------------------------*/

#define RUN_TEST(testfunc) \
    Unity.CurrentTestName = #testfunc; \
    Unity.CurrentTestLineNumber = __LINE__; \
    Unity.NumberOfTests++; \
    if (TEST_PROTECT()) { \
        setUp(); \
        testfunc(); \
    } \
    if (TEST_PROTECT()) { \
        tearDown(); \
    } \
    UnityConcludeTest();

/*-------------------------------------------------------
 * Basic Assertions
 *-------------------------------------------------------*/

#define TEST_FAIL_MESSAGE(message)                                                   UnityFail((message), __LINE__)
#define TEST_FAIL()                                                                  TEST_FAIL_MESSAGE(NULL)
#define TEST_IGNORE_MESSAGE(message)                                                 UnityIgnore((message), __LINE__)
#define TEST_IGNORE()                                                                TEST_IGNORE_MESSAGE(NULL)
#define TEST_MESSAGE(message)                                                        UnityMessage((message), __LINE__)
#define TEST_ONLY()

/*-------------------------------------------------------
 * Boolean
 *-------------------------------------------------------*/

#define TEST_ASSERT_TRUE(condition)              UnityAssertEqualNumber((UNITY_INT)(1), (UNITY_INT)(!!(condition)), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_FALSE(condition)             UnityAssertEqualNumber((UNITY_INT)(0), (UNITY_INT)(!!(condition)), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT(condition)                   TEST_ASSERT_TRUE(condition)
#define TEST_ASSERT_TRUE_MESSAGE(condition, message)  UnityAssertEqualNumber((UNITY_INT)(1), (UNITY_INT)(!!(condition)), (message), __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_FALSE_MESSAGE(condition, message) UnityAssertEqualNumber((UNITY_INT)(0), (UNITY_INT)(!!(condition)), (message), __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_MESSAGE(condition, message)  TEST_ASSERT_TRUE_MESSAGE(condition, message)
#define TEST_ASSERT_UNLESS(condition)            TEST_ASSERT_FALSE(condition)
#define TEST_ASSERT_UNLESS_MESSAGE(condition, message) TEST_ASSERT_FALSE_MESSAGE(condition, message)

/*-------------------------------------------------------
 * NULL
 *-------------------------------------------------------*/

#define TEST_ASSERT_NULL(pointer)                UnityAssertEqualNumber((UNITY_INT)(0), (UNITY_INT)((void*)(pointer)), NULL, __LINE__, UNITY_DISPLAY_STYLE_POINTER)
#define TEST_ASSERT_NOT_NULL(pointer)            TEST_ASSERT_TRUE((pointer) != NULL)
#define TEST_ASSERT_NULL_MESSAGE(pointer, message)     UnityAssertEqualNumber((UNITY_INT)(0), (UNITY_INT)((void*)(pointer)), (message), __LINE__, UNITY_DISPLAY_STYLE_POINTER)
#define TEST_ASSERT_NOT_NULL_MESSAGE(pointer, message) TEST_ASSERT_TRUE_MESSAGE((pointer) != NULL, (message))

/*-------------------------------------------------------
 * Integers
 *-------------------------------------------------------*/

#define TEST_ASSERT_EQUAL_INT(expected, actual)                      UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_EQUAL_INT8(expected, actual)                     UnityAssertEqualNumber((UNITY_INT)(UNITY_INT8 )(expected), (UNITY_INT)(UNITY_INT8 )(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_EQUAL_INT16(expected, actual)                    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT16)(expected), (UNITY_INT)(UNITY_INT16)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_EQUAL_INT32(expected, actual)                    UnityAssertEqualNumber((UNITY_INT)(UNITY_INT32)(expected), (UNITY_INT)(UNITY_INT32)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)

#define TEST_ASSERT_EQUAL_UINT(expected, actual)                     UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)
#define TEST_ASSERT_EQUAL_UINT8(expected, actual)                    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )(expected), (UNITY_INT)(UNITY_UINT8 )(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)
#define TEST_ASSERT_EQUAL_UINT16(expected, actual)                   UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT16)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)
#define TEST_ASSERT_EQUAL_UINT32(expected, actual)                   UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT32)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)

#define TEST_ASSERT_EQUAL_HEX(expected, actual)                      UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)
#define TEST_ASSERT_EQUAL_HEX8(expected, actual)                     UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT8 )(expected), (UNITY_INT)(UNITY_UINT8 )(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX8)
#define TEST_ASSERT_EQUAL_HEX16(expected, actual)                    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT16)(expected), (UNITY_INT)(UNITY_UINT16)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX16)
#define TEST_ASSERT_EQUAL_HEX32(expected, actual)                    UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT32)(expected), (UNITY_INT)(UNITY_UINT32)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

#define TEST_ASSERT_EQUAL_PTR(expected, actual)                      UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT)(expected), (UNITY_INT)(UNITY_UINT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_POINTER)
#define TEST_ASSERT_EQUAL(expected, actual)                          TEST_ASSERT_EQUAL_INT((expected), (actual))

/* Integer with messages */
#define TEST_ASSERT_EQUAL_INT_MESSAGE(expected, actual, message)     UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_EQUAL_UINT_MESSAGE(expected, actual, message)    UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), __LINE__, UNITY_DISPLAY_STYLE_UINT)
#define TEST_ASSERT_EQUAL_HEX32_MESSAGE(expected, actual, message)   UnityAssertEqualNumber((UNITY_INT)(expected), (UNITY_INT)(actual), (message), __LINE__, UNITY_DISPLAY_STYLE_HEX32)
#define TEST_ASSERT_EQUAL_PTR_MESSAGE(expected, actual, message)     UnityAssertEqualNumber((UNITY_INT)(UNITY_UINT)(expected), (UNITY_INT)(UNITY_UINT)(actual), (message), __LINE__, UNITY_DISPLAY_STYLE_POINTER)

/*-------------------------------------------------------
 * Comparison 
 *-------------------------------------------------------*/

#define TEST_ASSERT_GREATER_THAN(threshold, actual)                  TEST_ASSERT_TRUE((actual) > (threshold))
#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)              TEST_ASSERT_TRUE((actual) >= (threshold))
#define TEST_ASSERT_LESS_THAN(threshold, actual)                     TEST_ASSERT_TRUE((actual) < (threshold))
#define TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)                 TEST_ASSERT_TRUE((actual) <= (threshold))

#define TEST_ASSERT_INT_WITHIN(delta, expected, actual)              UnityAssertNumbersWithin((UNITY_UINT)(delta), (UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)
#define TEST_ASSERT_UINT_WITHIN(delta, expected, actual)             UnityAssertNumbersWithin((UNITY_UINT)(delta), (UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_UINT)
#define TEST_ASSERT_HEX_WITHIN(delta, expected, actual)              UnityAssertNumbersWithin((UNITY_UINT)(delta), (UNITY_INT)(expected), (UNITY_INT)(actual), NULL, __LINE__, UNITY_DISPLAY_STYLE_HEX32)

/*-------------------------------------------------------
 * Strings
 *-------------------------------------------------------*/

#define TEST_ASSERT_EQUAL_STRING(expected, actual)                   UnityAssertEqualString((const char*)(expected), (const char*)(actual), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_STRING_LEN(expected, actual, len)          UnityAssertEqualStringLen((const char*)(expected), (const char*)(actual), (UNITY_UINT32)(len), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message)  UnityAssertEqualString((const char*)(expected), (const char*)(actual), (message), __LINE__)

/*-------------------------------------------------------
 * Memory
 *-------------------------------------------------------*/

#define TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)              UnityAssertEqualMemory((expected), (actual), (UNITY_UINT32)(len), 1, NULL, __LINE__)
#define TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expected, actual, len, msg) UnityAssertEqualMemory((expected), (actual), (UNITY_UINT32)(len), 1, (msg), __LINE__)

/*-------------------------------------------------------
 * Floats
 *-------------------------------------------------------*/

#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)            UnityAssertFloatsWithin((delta), (expected), (actual), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_FLOAT(expected, actual)                    TEST_ASSERT_FLOAT_WITHIN((float)(expected) * 0.00001f, (expected), (actual))
#define TEST_ASSERT_FLOAT_WITHIN_MESSAGE(delta, expected, actual, message) UnityAssertFloatsWithin((delta), (expected), (actual), (message), __LINE__)

#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)           UnityAssertDoublesWithin((delta), (expected), (actual), NULL, __LINE__)
#define TEST_ASSERT_EQUAL_DOUBLE(expected, actual)                   TEST_ASSERT_DOUBLE_WITHIN((double)(expected) * 0.00001, (expected), (actual))

/*-------------------------------------------------------
 * Backwards Compatibility (deprecated)
 *-------------------------------------------------------*/

#define TEST_ASSERT_EQUAL_INT_ARRAY(expected, actual, num_elements)  UnityAssertEqualIntArray((const UNITY_INT*)(expected), (const UNITY_INT*)(actual), (UNITY_UINT32)(num_elements), NULL, __LINE__, UNITY_DISPLAY_STYLE_INT)

#endif /* UNITY_H */
