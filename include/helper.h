#ifndef HELPER_H
#define HELPER_H

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <algorithm>

#define VERBOSE
#define INFO

#ifdef VERBOSE
 #define PRINT_VERBOSE(msg, args...) fprintf(stdout, "VERBOSE: %s %d %s: " msg, __FILE__, __LINE__, __func__, ##args)
#else
 #define PRINT_VERBOSE(msg, args...)
#endif

#ifdef INFO
 #define PRINT_INFO(msg, args...) fprintf(stdout, "INFO: %s %d %s: " msg, __FILE__, __LINE__, __func__, ##args)
#else
 #define PRINT_INFO(msg, args...)
#endif

#define PANIC(msg, args...) {\
							fprintf(stderr, "PANIC: %s %d %s: " msg, __FILE__, __LINE__, __func__, ##args);\
							exit(1); }

#endif