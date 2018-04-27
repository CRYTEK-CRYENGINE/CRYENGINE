// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//  (c) 2002-2012 Crytek GmbH 
//  Created 24.1.2002 by Sergiy

#ifndef _IRCLOG_H_
#define _IRCLOG_H_

#include <stdarg.h>


// This interface is used by RC and converters to log events 
struct IRCLog
{
	enum EType
	{
		eType_Info,
		eType_Warning,
		eType_Error
	};

	virtual ~IRCLog()
	{
	}

	virtual void LogV(const EType eType, const char* szFormat, va_list args) = 0;
};


void SetRCLog(IRCLog* pRCLog);
void RCLog(const char* szFormat, ...);
void RCLogWarning(const char* szFormat, ...);
void RCLogError(const char* szFormat, ...);

#endif
