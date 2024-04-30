// Copyright © 2024. GothicKit Contributors
// SPDX-License-Identifier: MIT-Modern-Variant
#include "_Internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

enum { DmGlob_logBufferSize = 4096 };

static void DmInt_defaultLogger(void* ctx, DmLogLevel lvl, char const* msg);

static DmLogLevel DmGlob_logLevel = DmLogLevel_INFO;
static DmLogHandler* DmGlob_logCallback = NULL;
static void* DmGlob_logContext = NULL;
static thread_local char DmGlob_logBuffer[DmGlob_logBufferSize];
static mtx_t DmGlob_stdoutLock;

void Dm_setLogger(DmLogLevel lvl, DmLogHandler* log, void* ctx) {
	DmGlob_logCallback = log;
	DmGlob_logContext = ctx;
	DmGlob_logLevel = lvl;
}

void Dm_setLoggerDefault(DmLogLevel lvl) {
	DmGlob_logCallback = DmInt_defaultLogger;
	DmGlob_logContext = NULL;
	DmGlob_logLevel = lvl;

	mtx_destroy(&DmGlob_stdoutLock);
	(void) mtx_init(&DmGlob_stdoutLock, mtx_plain);
}

void Dm_setLoggerLevel(DmLogLevel lvl) {
	DmGlob_logLevel = lvl;
}

void Dm_report(DmLogLevel lvl, char const* fmt, ...) {
	if (DmGlob_logCallback == NULL || lvl > DmGlob_logLevel) {
		return;
	}

	va_list ap;
	va_start(ap, fmt);

	(void) vsnprintf(DmGlob_logBuffer, DmGlob_logBufferSize - 1, fmt, ap);
	DmGlob_logCallback(DmGlob_logContext, lvl, DmGlob_logBuffer);

	va_end(ap);
}

#define ANSI_RESET "\x1B[0m"
#define ANSI_GRAY "\x1B[90m"
#define ANSI_RED "\x1B[31m"
#define ANSI_GREEN "\x1B[32m"
#define ANSI_YELLOW "\x1B[33m"
#define ANSI_BLUE "\x1B[34m"
#define ANSI_MAGENTA "\x1B[35m"
#define ANSI_BOLD "\x1B[1m"
#define PREFIX "[" ANSI_MAGENTA ANSI_BOLD "DirectMusic" ANSI_RESET "]"

static void DmInt_defaultLogger(void* ctx, DmLogLevel lvl, char const* msg) {
	(void) ctx;

	struct tm now;
	time_t now_t = time(NULL);
	(void) gmtime_r(&now_t, &now);

	char const* format = PREFIX " (     ) › %s: %s\n";
	switch (lvl) {
	case DmLogLevel_FATAL:
		format = PREFIX " (" ANSI_RED "FATAL" ANSI_RESET ") › %s\n";
		break;
	case DmLogLevel_ERROR:
		format = PREFIX " (" ANSI_RED "ERROR" ANSI_RESET ") › %s\n";
		break;
	case DmLogLevel_WARN:
		format = PREFIX " (" ANSI_YELLOW "WARN " ANSI_RESET ") › %s\n";
		break;
	case DmLogLevel_INFO:
		format = PREFIX " (" ANSI_BLUE "INFO " ANSI_RESET ") › %s\n";
		break;
	case DmLogLevel_DEBUG:
		format = PREFIX " (" ANSI_GREEN "DEBUG" ANSI_RESET ") › %s\n";
		break;
	case DmLogLevel_TRACE:
		format = PREFIX " (TRACE) › %s\n";
		break;
	}

	(void) mtx_lock(&DmGlob_stdoutLock);
	(void) fprintf(stderr,
	               ANSI_GRAY "%04d-%02d-%02d %02d:%02d:%02d " ANSI_RESET,
	               now.tm_year + 1900,
	               now.tm_mon + 1,
	               now.tm_mday,
	               now.tm_hour,
	               now.tm_min,
	               now.tm_sec);

	(void) fprintf(stderr, format, msg);
	(void) mtx_unlock(&DmGlob_stdoutLock);
}
