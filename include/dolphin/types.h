#ifndef _DOLPHIN_TYPES_H_
#define _DOLPHIN_TYPES_H_

#ifdef SONAR_PORT
#include <sonar/legacy_types.h>
#else
typedef signed   char          s8;
typedef unsigned char          u8;
typedef signed   short int     s16;
typedef unsigned short int     u16;
typedef signed   long          s32;
typedef unsigned long          u32;
typedef signed   long long int s64;
typedef unsigned long long int u64;

typedef float  f32;
typedef double f64;
#endif

typedef char *Ptr;
#ifndef SONAR_PORT
typedef unsigned int uintptr_t; // Manually added
#endif

#ifndef ADDR_TYPES_DEFINED
#define ADDR_TYPES_DEFINED
/* Integers that hold an address: the console's 32-bit types, and a whole host pointer in the sonarlib port */
#ifdef SONAR_PORT
typedef uintptr_t uaddr;
typedef intptr_t saddr;
#else
typedef u32 uaddr;
typedef int saddr;
#endif
/* saddr where the console's signature took an s32, so its mangled name keeps the long */
#ifdef SONAR_PORT
typedef intptr_t saddr_long;
#else
typedef s32 saddr_long;
#endif
#endif

typedef int BOOL;

#define FALSE 0
#define TRUE 1

#if defined(__MWERKS__)
#define AT_ADDRESS(addr) : (addr)
#elif defined(__GNUC__)
//#define AT_ADDRESS(addr) __attribute__((address((addr))))
#define AT_ADDRESS(addr)  // was removed in GCC. define in linker script instead.
#else
#error unknown compiler
#endif

#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))

#ifdef SONAR_PORT
#include <limits.h>
#else
#define INT_MIN -2147483648
#define INT_MAX 2147483647
#endif

#ifndef NULL
#ifndef __cplusplus
#define NULL ((void*)0)
#else
#define NULL 0
#endif
#endif

#ifdef SONAR_PORT
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#else
#include "libc/stdio.h"
#include "libc/stdarg.h"
#include "libc/string.h"
#include "libc/ctype.h"
#endif

#include "cmath.h"

#endif
