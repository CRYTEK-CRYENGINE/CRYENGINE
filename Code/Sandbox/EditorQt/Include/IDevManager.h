// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/QuestionDialog.h"

struct IDevManager
{
	virtual bool HasValidDefaultLicense() const = 0;
};

class CDevManager : public IDevManager
{
public:
	CDevManager() {}
	virtual ~CDevManager() {}

	virtual bool HasValidDefaultLicense() const override
	{
		if (gEnv && gEnv->pConsole)
		{
			ICVar* pCVar = gEnv->pConsole->GetCVar("ed_useDevManager");
			if (pCVar && pCVar->GetIVal())
			{
				if ((INT_PTR)ShellExecute(nullptr, "open", GetLauncherAppPath(), "--silent", nullptr, SW_SHOWDEFAULT) <= 32)
				{
					if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SCritical(QObject::tr(""),
					                                                                        QObject::tr("You need a valid CRYENGINE Launcher installation on your system in order to run CRYENGINE Sandbox.\n"
					                                                                                    "\nPlease visit www.cryengine.com for more details.\n"
					                                                                                    "\n"
					                                                                                    "Do you want to go there now?"),
					                                                                        QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No, QDialogButtonBox::StandardButton::Yes))
					{
						ShellExecute(nullptr, "open", "http://www.cryengine.com", nullptr, nullptr, SW_SHOWNORMAL);
					}
					return false;
				}
			}
		}
		return true;
	}

private:
	string GetLauncherAppPath() const
	{
		DWORD pathLength = _MAX_PATH;
		wchar_t pathValue[_MAX_PATH + 1]; // +1 because RegQueryValueEx doesn't necessarily 0-terminate.
		DWORD dwType = REG_SZ;
		HKEY hKey = 0;

		if ((RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Wow6432Node\\GfaceGmbh\\ce-launcher", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) ||
		    (RegQueryValueExW(hKey, L"Path", nullptr, &dwType, (LPBYTE)&pathValue, &pathLength) != ERROR_SUCCESS))
		{
			if ((RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\GfaceGmbh\\ce-launcher", 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) ||
			    (RegQueryValueExW(hKey, L"Path", nullptr, &dwType, (LPBYTE)&pathValue, &pathLength) != ERROR_SUCCESS))
			{
				return string("0");
			}
		}

		pathValue[pathLength] = L'\0';

		RegCloseKey(hKey);
		string fullPath = CryStringUtils::WStrToUTF8(pathValue);
		fullPath += string("live\\CRYENGINE_Launcher.exe");

		return fullPath;
	}
};

