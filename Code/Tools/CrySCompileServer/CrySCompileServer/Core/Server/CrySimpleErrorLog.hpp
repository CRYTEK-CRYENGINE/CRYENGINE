// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSIMPLEERRORLOG__
#define __CRYSIMPLEERRORLOG__

#include <vector>
#include <list>
#include "CrySimpleMutex.hpp"

class ICryError;
typedef std::list<ICryError*> tdErrorList;

class CCrySimpleErrorLog
{
	CCrySimpleMutex							m_LogMutex;       // protects both below variables
	tdErrorList										m_Log;            // error log
	unsigned int                m_lastErrorTime;  // last time an error came in (we mail out a little after we've stopped receiving errors)

	void												Init();
	void												SendMail();
	CCrySimpleErrorLog();
public:

	bool												Add(ICryError *err);
	void                        Tick();

	static CCrySimpleErrorLog&	Instance();
};

#endif
