// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
    Assert dialog box for android.

   -------------------------------------------------------------------------
   History:
*************************************************************************/

#if !defined(__CRYASSERT_ANDROID_H__) && defined(USE_CRY_ASSERT) && CRY_PLATFORM_ANDROID
#define __CRYASSERT_ANDROID_H__

static char gs_szMessage[MAX_PATH];

void CryAssertTrace(const char* szFormat, ...)
{
	if (gEnv == 0)
	{
		return;
	}

	if (!gEnv->ignoreAllAsserts || gEnv->bTesting)
	{
		if (szFormat == NULL)
		{
			gs_szMessage[0] = '\0';
		}
		else
		{
			va_list args;
			va_start(args, szFormat);
			cry_vsprintf(gs_szMessage, szFormat, args);
			va_end(args);
		}
	}
}

bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool* pIgnore)
{
	IF_UNLIKELY (!gEnv || !gEnv->pSystem || !gEnv->pLog)
	{
		return false;
	}

	gEnv->pSystem->OnAssert(szCondition, gs_szMessage, szFile, line);

	gEnv->pLog->LogError("Assertion Failed! %s:%d reason:%s", szFile, line, gs_szMessage[0] ? gs_szMessage : "<empty>");

	return false;
}

#endif // __CRYASSERT_ANDROID_H__
