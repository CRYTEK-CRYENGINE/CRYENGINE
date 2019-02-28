// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CrySimpleHTTP__
#define __CrySimpleHTTP__

#include <string>

extern bool g_Success;

class CCrySimpleSock;

class CCrySimpleHTTP
{
	static volatile long 			ms_ExceptionCount;
	CCrySimpleSock*						m_pServerSocket;
	void											Init();
public:
														CCrySimpleHTTP();

	void											Run();
};

#endif
