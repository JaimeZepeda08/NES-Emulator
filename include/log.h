#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>

// External variable to control debug output
extern int debug_enable;

// ANSI color codes
#define COLOR_RESET     "\x1b[0m"
#define COLOR_RED       "\x1b[31m"
#define COLOR_BOLD_RED  "\x1b[1;31m"
#define COLOR_BLUE      "\x1b[34m"  // Blue for CPU
#define COLOR_GREEN     "\x1b[32m"  // Green for PPU
#define COLOR_ORANGE    "\x1b[38;5;208m" // Orange for MEM
#define COLOR_PURPLE    "\x1b[35m"  // Purple for CNTRL

// Error Macros
#define ERROR_MSG(module, msg, ...) \
    fprintf(stderr, COLOR_RED "[ERROR] [%s] " msg COLOR_RESET "\n", module, ##__VA_ARGS__)

#define FATAL_ERROR(module, msg, ...) \
    do { \
        fprintf(stderr, COLOR_BOLD_RED "[FATAL ERROR] [%s] " msg COLOR_RESET "\n", module, ##__VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while (0)

// Debugging Macros
#define DEBUG_MSG_CPU(msg, ...) \
    if (debug_enable) { \
        fprintf(stdout, COLOR_BLUE "[CPU] " msg COLOR_RESET "\n", ##__VA_ARGS__); \
    }

#define DEBUG_MSG_PPU(msg, ...) \
    if (debug_enable) { \
        fprintf(stdout, COLOR_GREEN "[PPU] " msg COLOR_RESET "\n", ##__VA_ARGS__); \
    }

#define DEBUG_MSG_MEM(msg, ...) \
    if (debug_enable) { \
        fprintf(stdout, COLOR_ORANGE "[MEM] " msg COLOR_RESET "\n", ##__VA_ARGS__); \
    }

#define DEBUG_MSG_CNTRL(msg, ...) \
    if (debug_enable) { \
        fprintf(stdout, COLOR_PURPLE "[CNTRL] " msg COLOR_RESET "\n", ##__VA_ARGS__); \
    }

#endif
