// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Executes an ASCII batch file of console commands...

   -------------------------------------------------------------------------
   History:
   - 19:04:2006   10:38 : Created by Jan Müller
   - 26:06:2006           Modified by Timur.

*************************************************************************/

#include "StdAfx.h"
#include "ConsoleBatchFile.h"
#include <CrySystem/ConsoleRegistration.h>
#include <CrySystem/ISystem.h>
#include "XConsole.h"
#include <CryString/CryPath.h>
#include <stdio.h>
#include "System.h"
#include "SystemCFG.h"

IConsole* CConsoleBatchFile::m_pConsole = NULL;

void CConsoleBatchFile::Init()
{
	m_pConsole = gEnv->pConsole;
	REGISTER_COMMAND("exec", (ConsoleCommandFunc)ExecuteFileCmdFunc, 0, "executes a batch file of console commands");
}

//////////////////////////////////////////////////////////////////////////
void CConsoleBatchFile::ExecuteFileCmdFunc(IConsoleCmdArgs* args)
{
	if (!m_pConsole)
		Init();

	if (!args->GetArg(1))
		return;

	ExecuteConfigFile(args->GetArg(1));
}

//////////////////////////////////////////////////////////////////////////
bool CConsoleBatchFile::ExecuteConfigFile(const char* sFilename)
{
	if (!m_pConsole)
		Init();

	string filename = PathUtil::Make(gEnv->pSystem->GetRootFolder(), PathUtil::GetFile(sFilename));
	if (strlen(PathUtil::GetExt(filename)) == 0)
	{
		filename = PathUtil::ReplaceExtension(filename, "cfg");
	}

	//////////////////////////////////////////////////////////////////////////
	CCryFile file;

	{
#if !defined(EXCLUDE_NORMAL_LOG)
		const char* szLog = "Executing console batch file (try game,config,root):";
#endif
		string filenameLog;
		string sfn = PathUtil::GetFile(filename);

		if (!CSystemConfiguration::OpenFile(filename, file, ICryPak::FOPEN_ONDISK))
		{
			CryLog("%s \"%s\" not found!", szLog, filename.c_str());
			return false;
		}
		filenameLog = file.GetFilename();
		CryLog("%s \"%s\" found in %s ...", szLog, filename.c_str(), filenameLog.c_str());
	}

	// Only circumvent whitelist when the file was not in an archive, since we expect archives to be signed in release mode when whitelist is used
	// Note that we still support running release mode without signed / encrypted archives, however this means that a conscious decision to sacrifice security has already been made.
	const bool ignoreWhitelist = file.IsInPak();
	// Only allow console commands in autoexec.cfg
	const bool allowConsoleCommands = !stricmp(sFilename, "autoexec.cfg");

	int nLen = file.GetLength();
	char* sAllText = new char[nLen + 16];
	file.ReadRaw(sAllText, nLen);
	sAllText[nLen] = '\0';
	sAllText[nLen + 1] = '\0';

	/*
	   This can't work properly as ShowConsole() can be called during the execution of the scripts,
	   which means bConsoleStatus is outdated and must not be set at the end of the function

	   bool bConsoleStatus = ((CXConsole*)m_pConsole)->GetStatus();
	   ((CXConsole*)m_pConsole)->SetStatus(false);
	 */

	char* strLast = sAllText + nLen;
	char* str = sAllText;
	while (str < strLast)
	{
		char* s = str;
		while (str < strLast && *str != '\n' && *str != '\r')
			str++;
		*str = '\0';
		str++;
		while (str < strLast && (*str == '\n' || *str == '\r'))
			str++;

		string strLine = s;

		//trim all whitespace characters at the beginning and the end of the current line and store its size
		strLine.Trim();
		size_t strLineSize = strLine.size();

		//skip comments, comments start with ";" or "--" but may have preceding whitespace characters
		if (strLineSize > 0)
		{
			if (strLine[0] == ';')
				continue;
			else if (strLine.find("--") == 0)
				continue;
		}
		//skip empty lines
		else
			continue;

		if (ignoreWhitelist || (gEnv->pSystem->IsCVarWhitelisted(strLine.c_str(), false)))
		{
			if (allowConsoleCommands)
			{
				m_pConsole->ExecuteString(strLine);
			}
			else
			{
				// Parse the line from .cfg, splitting CVar name and value from the string (format is CVarName=CVarValue)
				const size_t pos = strLine.find_first_of('=');
				const string variableName = strLine.substr(0, pos).Trim();
				const string variableValue = strLine.substr(pos + 1).Trim();

				// Notify console to set the value, or defer if the CVar had not been registered yet (likely since parsing occurs very early)
				m_pConsole->LoadConfigVar(variableName, variableValue);
			}
		}
#if defined(DEDICATED_SERVER)
		else
		{
			gEnv->pSystem->GetILog()->LogError("Failed to execute command: '%s' as it is not whitelisted\n", strLine.c_str());
		}
#endif   // defined(DEDICATED_SERVER)
	}
	// See above
	//	((CXConsole*)m_pConsole)->SetStatus(bConsoleStatus);

	delete[]sAllText;
	return true;
}
