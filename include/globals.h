#ifndef GLOBALS_H
#define GLOBALS_H

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#include <stdio.h>
#define C_LOG(format, ...) printf(ANSI_COLOR_CYAN "[LOG] " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define C_LOG_OK(format, ...) printf(ANSI_COLOR_GREEN "[OK] " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define C_LOG_ERR(format, ...) printf(ANSI_COLOR_RED "[ERR] " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define C_LOG_WARN(format, ...) printf(ANSI_COLOR_YELLOW "[WARNING] " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define C_LOG_INFO(format, ...) printf(ANSI_COLOR_BLUE "[INFO] " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)


#define DEBUG
#ifdef DEBUG
#define LOG_M(format, ...) printf(ANSI_COLOR_CYAN "{LOG} " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_M_OK(format, ...) printf(ANSI_COLOR_GREEN "{OK} " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_M_ERR(format, ...) printf(ANSI_COLOR_RED "{ERR} " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_M_WARN(format, ...) printf(ANSI_COLOR_YELLOW "{WARNING} " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#define LOG_M_INFO(format, ...) printf(ANSI_COLOR_BLUE "{INFO} " ANSI_COLOR_RESET format "\n", ##__VA_ARGS__)
#else
#define LOG_M(format, ...) 
#define LOG_M_ERR(format, ...) 
#define LOG_M_WARN(format, ...) 
#define LOG_M_INFO(format, ...) 
#endif


#endif
