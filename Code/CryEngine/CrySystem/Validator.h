// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

//
//	File: Validator.h
//
//	History:
//	-Feb 09,2004:Created
//
//////////////////////////////////////////////////////////////////////

#ifndef VALIDATOR_H
#define VALIDATOR_H

#if _MSC_VER > 1000
	#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
// Default validator implementation.
//////////////////////////////////////////////////////////////////////////
struct SDefaultValidator : public IValidator
{
	CSystem* m_pSystem;
	SDefaultValidator(CSystem* system) : m_pSystem(system) {};
	virtual void Report(SValidatorRecord& record)
	{
		if (record.text)
		{
			static bool bNoMsgBoxOnWarnings = false;
			if ((record.text[0] == '!') || (m_pSystem->m_sysWarnings && m_pSystem->m_sysWarnings->GetIVal() != 0))
			{
				if (g_cvars.sys_no_crash_dialog)
					return;

				if (bNoMsgBoxOnWarnings)
					return;

#if CRY_PLATFORM_WINDOWS
				ICVar* pFullscreen = (gEnv && gEnv->pConsole) ? gEnv->pConsole->GetCVar("r_Fullscreen") : 0;
				if (pFullscreen && pFullscreen->GetIVal() != 0 && gEnv->pRenderer && gEnv->pRenderer->GetHWND())
				{
					::ShowWindow((HWND)gEnv->pRenderer->GetHWND(), SW_MINIMIZE);
				}
				string strMessage = record.text;
				strMessage += "\n---------------------------------------------\nAbort - terminate application\nRetry - continue running the application\nIgnore - don't show this message box any more";
				switch (::MessageBox(NULL, strMessage.c_str(), "CryEngine Warning", MB_ABORTRETRYIGNORE | MB_DEFBUTTON2 | MB_ICONWARNING | MB_SYSTEMMODAL))
				{
				case IDABORT:
					m_pSystem->GetIConsole()->Exit("User abort requested during showing the warning box with the following message: %s", record.text);
					break;
				case IDRETRY:
					break;
				case IDIGNORE:
					bNoMsgBoxOnWarnings = true;
					m_pSystem->m_sysWarnings->Set(0);
					break;
				}
#endif
			}
		}
	}
};

#endif
