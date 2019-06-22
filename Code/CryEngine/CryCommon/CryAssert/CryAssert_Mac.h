// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool* pIgnore)
{
	if (!Cry::Assert::IgnoreAllAsserts() && Cry::Assert::ShowDialogOnAssert())
	{
		static char s_charBuffer[4096];
		size_t file_len = strlen(szFile);
		cry_sprintf(s_charBuffer,
				"osascript -e 'tell application \"Terminal\"' "
				"-e 'activate' "
				"-e 'set w to do script \"%s/assert_term \\\"%s\\\" \\\"%s\\\" %d \\\"%s\\\"; echo $? > \\\"%s\\\"/.assert_return;\"' "
				"-e 'repeat' "
				"-e 'delay 0.1' "
				"-e 'if not busy of w then exit repeat' "
				"-e 'end repeat' "
				"-e 'do script \"exit 0\" in window 1' "
				"-e 'end tell' &>/dev/null",
				GetModulePath(), szCondition, (file_len > 60) ? szFile + (file_len - 61) : szFile, line, tls_szAssertMessage, GetModulePath());
		int ret = system(s_charBuffer);
		if (ret != 0)
		{
			CryLogAlways("<Assert> Terminal failed to execute");
			return false;
		}
		cry_sprintf(s_charBuffer, "%s/.assert_return", GetModulePath());
		FILE* assert_file = fopen(s_charBuffer, "r");
		if (!assert_file)
		{
			CryLogAlways("<Assert> Couldn't open assert file");
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
			Cry::Assert::IgnoreAllAsserts(true);
			break;
		case 3:
			return true;
			break;
		case 4:
			raise(SIGABRT);
			exit(-1);
			break;
		default:
			CryLogAlways("<Assert> Unknown result in assert file: %d", result);
			return false;
		}
	}

	return false;
}
