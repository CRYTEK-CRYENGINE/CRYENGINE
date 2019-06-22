// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool* pIgnore)
{
#if defined(eCryModule)
	const char* szModuleName = GetCryModuleName(eCryModule);
#else
	const char* szModuleName = "Undefined";
#endif
	if (gEnv && gEnv->pLog)
		gEnv->pLog->LogError("Assertion Failed! %s:%d reason:%s Module:%s", szFile, line, tls_szAssertMessage, szModuleName);

	return false;
}
