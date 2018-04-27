// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:
    Assert dialog box for Linux. The linux assert dialog is based on a
    small ncurses application which writes the choice to a file. This
    was chosen since there is no default UI system on Linux. X11 wasn't
    used due to the possibility of the system running another display
    protocol (e.g.: WayLand, Mir)

   -------------------------------------------------------------------------
   History:
   - 27:01:2014: Created by Leander Beernaert
*************************************************************************/

#if defined(USE_CRY_ASSERT) && CRY_PLATFORM_LINUX

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

	char simplified[MAX_PATH];
	const char* const szSimplifiedFile = PathUtil::SimplifyFilePath(szFile, simplified, MAX_PATH, PathUtil::ePathStyle_Native) ? simplified : szFile;
	size_t file_len = strlen(szSimplifiedFile);
	size_t file_display_idx = file_len > 60 ? (file_len - 61) : 0; //60 chars is what visually fits in the ncurses dialog

	gEnv->pSystem->OnAssert(szCondition, gs_szMessage, szSimplifiedFile, line);
	gEnv->pLog->LogError("Assertion Failed! file:%s:%d reason:%s", szSimplifiedFile + file_display_idx, line, gs_szMessage[0] ? gs_szMessage : "<empty>");

	static char gs_command_str[4096];
	static CryLockT<CRYLOCK_RECURSIVE> lock;

	if (!gEnv->bUnattendedMode && !gEnv->ignoreAllAsserts)
	{

		CryAutoLock<CryLockT<CRYLOCK_RECURSIVE>> lk(lock);

		char szExecFilePath[_MAX_PATH];
		CryGetExecutableFolder(_MAX_PATH, szExecFilePath);
		if (strchr(szExecFilePath, '"'))
		{
			gEnv->pLog->LogError("<CryAssert Linux> Can not launch assert dialog: CRYENGINE folder contains a double quote character: %s", szExecFilePath);
			return false;
		}

		// the following has many problems with escaping because sh interprets the string
		// it is currently using shell string literals, which is not good enough.
		// there are plans to replace this with a new python/tk dialog --ines Nov/15
		cry_sprintf(gs_command_str,
		            "xterm -geometry 80x20 "
		            "-T 'Assertion Failed' "
		            "-e '$\"%sassert_term\" $\"%s\" $\"%s\" %d $\"%s\"; echo \"$?\" > .assert_return'",
		            szExecFilePath,
		            szCondition, szFile + file_display_idx, line, gs_szMessage);

		int ret = system(gs_command_str);

		if (ret != 0)
		{
			gEnv->pLog->LogError("<CryAssert Linux> Failed to launch assert dialog");
			return false;
		}

		FILE* assert_file = fopen(".assert_return", "r");
		if (!assert_file)
		{
			// there is no error check for the term and assert_term, if the file does not exist, there can be many reasons
			gEnv->pLog->LogError("<CryAssert Linux> Something went wrong with the assert dialog");
			return false;
		}
		int result = -1;
		fscanf(assert_file, "%d", &result);
		fclose(assert_file);

		switch (result)
		{
		case 0:
			break;
		case 1:
			*pIgnore = true;
			break;
		case 2:
			gEnv->ignoreAllAsserts = true;
			break;
		case 3:
			return true;
			break;
		case 4:
			raise(SIGABRT);
			exit(-1);
			break;
		default:
			gEnv->pLog->LogError("<CryAssert Linux> Unrecognized option from assert dialog (%d)", result);
			return false;
		}

	}

	return false;

}

#endif
