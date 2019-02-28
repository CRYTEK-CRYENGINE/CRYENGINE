// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>
#include "Log.h"

// NOTE: only strings prefixed with 'error: ' are reported by the remote compiler
void ReportError(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	vfprintf(stderr, format, args);

 	va_end(args);
}

void DumpError(const char* str)
{
	fwrite(str, sizeof(char), strlen(str), stderr);
}
