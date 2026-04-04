#ifndef GLOBALS_H
#define GLOBALS_H

#define DEBUG
#ifdef DEBUG
#include <stdio.h>
#define LOG(x) printf("[LOG] %s",x);
#define LOG_SIMPLE(x) printf("%s",x);
#define LOG_ERR(x) printf("[ERR] %s",x);
#define LOG_INT(x) printf("[LOG] %d",x);
#else
#define LOG(x) 
#define LOG_SIMPLE(x)
#define LOG_INT(x) 
#endif

#endif
