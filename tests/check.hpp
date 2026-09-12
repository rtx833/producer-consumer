#pragma once

#include <cstdio>
#include <cstdlib>

// Minimal assertion helper for dependency-free tests.
#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            std::exit(1);                                                                  \
        }                                                                                  \
    } while (false)
