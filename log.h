/**
 * Copyright (c) 2017 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See `log.c` for details.
 */

#ifndef XLOG_H
#define XLOG_H

#include <stdio.h>
#include <stdarg.h>

#define XLOG_VERSION "0.1.0"

typedef void (*log_LockFn)(void *udata, int lock);

enum { XLOG_TRACE, XLOG_DEBUG, XLOG_INFO, XLOG_WARN, XLOG_ERROR, XLOG_FATAL };

#define log_trace(...) log_log(XLOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define log_debug(...) log_log(XLOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  log_log(XLOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  log_log(XLOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) log_log(XLOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...) log_log(XLOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

void log_set_udata(void *udata);
void log_set_lock(log_LockFn fn);
void log_set_fp(FILE *fp);
void log_set_level(int level);
void log_set_quiet(int enable);

void log_log(int level, const char *file, int line, const char *fmt, ...);

#endif
