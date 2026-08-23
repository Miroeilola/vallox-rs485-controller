// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Miro Eilola / Mironet
//
// Minimal assertion helper shared by the host test binaries. Each test file
// defines its own `checks` and `failures` counters and a main() that prints
// them; see test_vallox_protocol.c for the pattern.
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
            failures++;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        long long _a = (long long)(a), _b = (long long)(b);                    \
        checks++;                                                              \
        if (_a != _b) {                                                        \
            printf("FAIL %s:%d  %s == %s  (%lld != %lld)\n", __FILE__,         \
                   __LINE__, #a, #b, _a, _b);                                  \
            failures++;                                                        \
        }                                                                      \
    } while (0)

#define REPORT() (printf("%d checks, %d failures\n", checks, failures), failures == 0 ? 0 : 1)
#endif
