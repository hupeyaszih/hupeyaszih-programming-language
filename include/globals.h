#ifndef GLOBALS_H
#define GLOBALS_H


#include <stdio.h>
#define C_LOG(format, ...) printf("[LOG] " format, ##__VA_ARGS__)
#define C_LOG_ERR(format, ...) printf("[ERR] " format, ##__VA_ARGS__)
#define C_LOG_WARN(format, ...) printf("[WARNING] " format, ##__VA_ARGS__)


#define DEBUG
#ifdef DEBUG
#define LOG_M(format, ...) printf("[LOG] " format, ##__VA_ARGS__)
#define LOG_M_ERR(format, ...) printf("[ERR] " format, ##__VA_ARGS__)
#define LOG_M_WARN(format, ...) printf("[WARNING] " format, ##__VA_ARGS__)
#else
#define LOG_M(format, ...) 
#define LOG_M_ERR(format, ...) 
#define LOG_M_WARN(format, ...) 
#endif


#endif
