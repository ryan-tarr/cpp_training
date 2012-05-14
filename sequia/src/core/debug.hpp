#ifndef _DEBUG_HPP_
#define _DEBUG_HPP_

#include <cassert>

#if defined(__GNUC__)
#define EXPECT_LIKELY(cond)    __builtin_expect((cond), 1)
#define EXPECT_UNLIKELY(cond)  __builtin_expect((cond), 0)
#else
#define EXPECT_LIKELY(cond)    (cond)
#define EXPECT_UNLIKELY(cond)  (cond)
#endif

#if ((defined DEBUG || defined _DEBUG) && !defined DISABLE_ASSERTS)
#define ASSUMEF(cond, ...) \
    if (EXPECT_UNLIKELY(!(cond))) \
    { \
        printf("--- ASSUMPTION FAILED: "); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    }
#define ASSERTF(cond, ...) \
    if (EXPECT_UNLIKELY(!(cond))) \
    { \
        printf("*** ASSERTION FAILED: "); \
        printf(__VA_ARGS__); \
        printf("\n"); \
        assert(cond); \
    }
#else
#define ASSUMEF(cond, ...) ((void) 0)
#define ASSERTF(cond, ...) ((void) 0)
#endif
    
#endif
