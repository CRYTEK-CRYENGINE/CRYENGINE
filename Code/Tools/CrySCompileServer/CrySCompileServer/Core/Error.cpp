// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdTypes.hpp"
#include "Error.hpp"
#include "tinyxml/tinyxml.h"
#include "Server/CrySimpleErrorLog.hpp"
#include "Server/CrySimpleJob.hpp"

#include <time.h>

ICryError::ICryError(EErrorType t)
  : m_eType(t),
    m_numDupes(0)
{
}

void logmessage(const char *text, ...)
{
	va_list arg;
	va_start(arg, text);
  
	char szBuffer[256];
  char *error = szBuffer;
	int bufferlen = sizeof(szBuffer)-1;
  memset(szBuffer, 0, sizeof(szBuffer));

  long req = CCrySimpleJob::GlobalRequestNumber();
	
  int ret = _snprintf(error, bufferlen, "% 8d | ", req);

  if(ret <= 0) return;
  error += ret;
  bufferlen -= ret;

  time_t ltime;
  time( &ltime );
  struct tm *today = localtime( &ltime );
  ret = (int)strftime( error, bufferlen, "%d/%m %H:%M:%S | ", today );

  if(ret <= 0) return;
  error += ret;
  bufferlen -= ret;

  int count = vsnprintf(error, bufferlen, text, arg);

  printf("%s", szBuffer);

	OutputDebugString(szBuffer);

	va_end(arg);
}
