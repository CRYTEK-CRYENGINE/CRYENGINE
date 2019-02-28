// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ILOGGER_H__
#define __ILOGGER_H__

#include <stdarg.h>
#include <stdio.h>

class ILogger
{
public:
	enum ESeverity
	{
		eSeverity_Debug,
		eSeverity_Info,
		eSeverity_Warning,
		eSeverity_Error
	};

	virtual ~ILogger()
	{
	}

	void Log(ESeverity eSeverity, const char* const format, ...)
	{
		char buffer[2048];
		{
			va_list args;
			va_start(args, format);
			_vsnprintf_s(buffer, sizeof(buffer), sizeof(buffer) - 1, format, args);
			va_end(args);
		}
		LogImpl(eSeverity, buffer);
	}

protected:
	virtual void LogImpl(ESeverity eSeverity, const char* text) = 0;
};

#endif
