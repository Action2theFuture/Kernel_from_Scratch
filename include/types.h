#ifndef TYPES_H
#define TYPES_H

/* Fixed-width types for i386 (ILP32). We do not include any host header. */
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
typedef signed long long    int64_t;
typedef unsigned long long  uint64_t;

typedef uint32_t            size_t;
typedef int32_t             ssize_t;
typedef uint32_t            uintptr_t;
typedef int32_t             intptr_t;

typedef _Bool               bool;
#define true  1
#define false 0

#define NULL ((void *)0)

/* Fail the build if the target is not 32-bit x86 (e.g. wrong compiler on the Mac). */
_Static_assert(sizeof(void *) == 4, "kernel must be built for i386 (32-bit)");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

#endif
