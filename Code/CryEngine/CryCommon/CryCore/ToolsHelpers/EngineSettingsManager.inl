// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(CRY_PLATFORM)
	#error CRY_PLATFORM is not defined, probably #include "stdafx.h" is missing.
#endif

#include "EngineSettingsManager.h"

#pragma warning (disable:4312)
#pragma warning(disable: 4244)

#if defined(CRY_ENABLE_RC_HELPER)

	#include <CryCore/Platform/CryWindows.h>
	#include <CryCore/Assert/CryAssert.h>

	#define IEntity IEntity_AfxOleOverride
	#include <CryCore/Platform/CryWindows.h>
	#include <shlobj.h>   // requires <windows.h>
	#include <shellapi.h> // requires <windows.h>
	#undef IEntity

	#include <stdio.h>

	#define REG_SOFTWARE           L"Software\\"
	#define REG_COMPANY_NAME       L"Crytek\\"
	#define REG_SETTING            L"Settings\\"
	#define REG_CRYTEK_SETTING_KEY REG_SOFTWARE REG_COMPANY_NAME REG_SETTING

// pseudo-variable that represents the DOS header of the module
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

	#define INFOTEXT L"Please specify the directory of your CryENGINE installation (RootPath):"

using namespace SettingsManagerHelpers;

static bool g_bWindowQuit;
static CEngineSettingsManager* g_pThis = 0;
static const unsigned int IDC_hEditRootPath = 100;
static const unsigned int IDC_hBtnBrowse = 101;

BOOL BrowseForFolder(HWND hWnd, LPCWSTR szInitialPath, LPWSTR szPath, LPCWSTR szTitle);

// Desc: Static msg handler which passes messages to the application class.
LRESULT static CALLBACK WndProcSettingsManager(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	assert(g_pThis);

	if (uMsg == WM_INITDIALOG)
	{
		int a = 0;   // placeholder for debug breakpoint
	}

	return g_pThis->HandleProc(hWnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
namespace
{
class RegKey
{
public:
	RegKey(const wchar_t* key, bool writeable);
	~RegKey();
	void* pKey;
};
}

//////////////////////////////////////////////////////////////////////////
RegKey::RegKey(const wchar_t* key, bool writeable)
{
	HKEY hKey;
	LONG result;
	if (writeable)
		result = RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, 0, 0, KEY_WRITE, 0, &hKey, 0);
	else
		result = RegOpenKeyExW(HKEY_CURRENT_USER, key, 0, KEY_READ, &hKey);
	pKey = hKey;
}

//////////////////////////////////////////////////////////////////////////
RegKey::~RegKey()
{
	RegCloseKey((HKEY)pKey);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CEngineSettingsManager::CEngineSettingsManager(const wchar_t* moduleName, const wchar_t* iniFileName)
	: m_hWndParent(0)
{
	m_sModuleName.clear();
	m_sModuleFileName.clear();

	RestoreDefaults();

	if (iniFileName != NULL)
	{
		m_sModuleFileName = iniFileName;
	}
	else if (moduleName != NULL)
	{
		m_sModuleName = moduleName;

		// find INI filename located in module path
		HMODULE hInstance = GetModuleHandleW(moduleName);
		wchar_t szFilename[_MAX_PATH];
		GetModuleFileNameW((HINSTANCE)&__ImageBase, szFilename, _MAX_PATH);
		wchar_t drive[_MAX_DRIVE];
		wchar_t dir[_MAX_DIR];
		wchar_t fname[_MAX_FNAME];
		wchar_t ext[1] = L"";
		_wsplitpath_s(szFilename, drive, dir, fname, ext);
		_wmakepath_s(szFilename, drive, dir, fname, L"ini");
		m_sModuleFileName = szFilename;
	}

	if (m_sModuleFileName.length() && LoadValuesFromConfigFile(m_sModuleFileName.c_str()))
	{
		m_bGetDataFromRegistry = false;
		return;
	}

	m_bGetDataFromRegistry = true;

	// load basic content from registry
	LoadEngineSettingsFromRegistry();
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::RestoreDefaults()
{
	// Engine
	SetKey("ENG_RootPath", L"");

	// RC
	SetKey("RC_ShowWindow", false);
	SetKey("RC_HideCustom", false);
	SetKey("RC_Parameters", L"");
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetModuleSpecificStringEntryUtf16(const char* key, SettingsManagerHelpers::CWCharBuffer wbuffer)
{
	if (wbuffer.getSizeInElements() <= 0)
	{
		return false;
	}

	if (!m_bGetDataFromRegistry)
	{
		if (!HasKey(key))
		{
			wbuffer[0] = 0;
			return false;
		}
		if (!GetValueByRef(key, wbuffer))
		{
			wbuffer[0] = 0;
			return false;
		}
	}
	else
	{
		CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
		s.append(m_sModuleName.c_str());
		RegKey superKey(s.c_str(), false);
		if (!superKey.pKey)
		{
			wbuffer[0] = 0;
			return false;
		}
		if (!GetRegValue(superKey.pKey, key, wbuffer))
		{
			wbuffer[0] = 0;
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetModuleSpecificStringEntryUtf8(const char* key, SettingsManagerHelpers::CCharBuffer buffer)
{
	if (buffer.getSizeInElements() <= 0)
	{
		return false;
	}

	wchar_t wBuffer[1024];

	if (!GetModuleSpecificStringEntryUtf16(key, SettingsManagerHelpers::CWCharBuffer(wBuffer, sizeof(wBuffer))))
	{
		buffer[0] = 0;
		return false;
	}

	SettingsManagerHelpers::ConvertUtf16ToUtf8(wBuffer, buffer);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetModuleSpecificIntEntry(const char* key, int& value)
{
	value = 0;

	if (!m_bGetDataFromRegistry)
	{
		if (!HasKey(key))
		{
			return false;
		}
		if (!GetValueByRef(key, value))
		{
			return false;
		}
	}
	else
	{
		CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
		s.append(m_sModuleName.c_str());
		RegKey superKey(s.c_str(), false);
		if (!superKey.pKey)
		{
			return false;
		}
		if (!GetRegValue(superKey.pKey, key, value))
		{
			value = 0;
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetModuleSpecificBoolEntry(const char* key, bool& value)
{
	value = false;

	if (!m_bGetDataFromRegistry)
	{
		if (!HasKey(key))
		{
			return false;
		}
		if (!GetValueByRef(key, value))
		{
			return false;
		}
	}
	else
	{
		CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
		s.append(m_sModuleName.c_str());
		RegKey superKey(s.c_str(), false);
		if (!superKey.pKey)
		{
			return false;
		}
		if (!GetRegValue(superKey.pKey, key, value))
		{
			value = false;
			return false;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetModuleSpecificStringEntryUtf16(const char* key, const wchar_t* str)
{
	SetKey(key, str);
	if (!m_bGetDataFromRegistry)
	{
		return StoreData(false);
	}

	CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
	s.append(m_sModuleName.c_str());
	RegKey superKey(s.c_str(), true);
	if (superKey.pKey)
	{
		return SetRegValue(superKey.pKey, key, str);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetModuleSpecificStringEntryUtf8(const char* key, const char* str)
{
	wchar_t wbuffer[512];
	SettingsManagerHelpers::ConvertUtf8ToUtf16(str, SettingsManagerHelpers::CWCharBuffer(wbuffer, sizeof(wbuffer)));

	return SetModuleSpecificStringEntryUtf16(key, wbuffer);
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetModuleSpecificIntEntry(const char* key, const int& value)
{
	SetKey(key, value);
	if (!m_bGetDataFromRegistry)
	{
		return StoreData(false);
	}

	CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
	s.append(m_sModuleName.c_str());
	RegKey superKey(s.c_str(), true);
	if (superKey.pKey)
	{
		return SetRegValue(superKey.pKey, key, value);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetModuleSpecificBoolEntry(const char* key, const bool& value)
{
	SetKey(key, value);
	if (!m_bGetDataFromRegistry)
	{
		return StoreData(false);
	}

	CFixedString<wchar_t, 256> s = L"Software\\Crytek\\Settings\\";
	s.append(m_sModuleName.c_str());
	RegKey superKey(s.c_str(), true);
	if (superKey.pKey)
	{
		return SetRegValue(superKey.pKey, key, value);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetRootPath(const wchar_t* szRootPath)
{
	CFixedString<wchar_t, MAX_PATH> path = szRootPath;
	size_t const len = path.length();

	if ((len > 0) && ((path[len - 1] == '\\') || (path[len - 1] == '/')))
	{
		path.setLength(len - 1);
	}

	SetKey("ENG_RootPath", path.c_str());
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::HasKey(const char* key)
{
	return m_keyValueArray.find(key) != 0;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const char* key, const wchar_t* value)
{
	m_keyValueArray.set(key, value);
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const char* key, bool value)
{
	m_keyValueArray.set(key, (value ? "true" : "false"));
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetKey(const char* key, int value)
{
	char buf[40];
	sprintf_s(buf, "%d", value);
	m_keyValueArray.set(key, buf);
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::GetRootPathUtf16(SettingsManagerHelpers::CWCharBuffer wbuffer)
{
	LoadEngineSettingsFromRegistry();
	GetValueByRef("ENG_RootPath", wbuffer);
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::GetRootPathAscii(SettingsManagerHelpers::CCharBuffer buffer)
{
	wchar_t wbuffer[MAX_PATH];
	GetRootPathUtf16(SettingsManagerHelpers::CWCharBuffer(wbuffer, sizeof(wbuffer)));
	SettingsManagerHelpers::GetAsciiFilename(wbuffer, buffer);
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetInstalledBuildRootPathUtf16(const int index, SettingsManagerHelpers::CWCharBuffer name, SettingsManagerHelpers::CWCharBuffer path)
{
	RegKey key(L"Software\\Crytek\\Settings\\CryExport\\ProjectBuilds", false);
	if (key.pKey)
	{
		DWORD type;
		DWORD nameSizeInBytes = DWORD(name.getSizeInBytes());
		DWORD pathSizeInBytes = DWORD(path.getSizeInBytes());
		LONG result = RegEnumValueW((HKEY)key.pKey, index, name.getPtr(), &nameSizeInBytes, NULL, &type, (BYTE*)path.getPtr(), &pathSizeInBytes);
		if (result == ERROR_SUCCESS)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::CallSettingsDialog(void* pParent)
{
	HWND hParent = (HWND)pParent;

	wchar_t pathBuffer[1024];
	GetRootPathUtf16(SettingsManagerHelpers::CWCharBuffer(pathBuffer, sizeof(pathBuffer)));
	if (!pathBuffer[0])
	{
		CallRootPathDialog(hParent);
		return;
	}

	if (FindWindowA(NULL, "CryENGINE Settings"))
		return;

	CFixedString<wchar_t, 1024> params;
	params.append(m_sModuleName.c_str());
	params.append(L" \"");
	params.append(m_sModuleFileName.c_str());
	params.append(L"\"");

	HINSTANCE res = ::ShellExecuteW(hParent, L"open", L"Tools\\SettingsMgr.exe", params.c_str(), pathBuffer, SW_SHOWNORMAL);

	if (res < (HINSTANCE)33)
	{
		MessageBoxA(hParent, "Could not execute CryENGINE settings dialog.\nPlease run Tools\\SettingsMgr.exe.", "Error", MB_OK | MB_ICONERROR);
		CallRootPathDialog(hParent);
		return;
	}

	Sleep(1000); // Sleep because CrySleep is not available in this scope!
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::CallRootPathDialog(void* pParent)
{
	HWND hParent = (HWND)pParent;

	g_bWindowQuit = false;
	g_pThis = this;

	const wchar_t* szWindowClass = L"CRYENGINEROOTPATHUI";

	// Register the window class
	WNDCLASSW wndClass = {
		0,    WndProcSettingsManager, 0,                   DLGWINDOWEXTRA,          GetModuleHandleW(0),
		NULL, LoadCursorW(NULL,       (LPCWSTR)IDC_ARROW), (HBRUSH)COLOR_BTNSHADOW, NULL, szWindowClass
	};

	RegisterClassW(&wndClass);

	bool bReEnableParent = false;

	if (IsWindowEnabled(hParent))
	{
		bReEnableParent = true;
		EnableWindow(hParent, false);
	}

	int cwX = CW_USEDEFAULT;
	int cwY = CW_USEDEFAULT;
	int cwW = 400 + 2 * GetSystemMetrics(SM_CYFIXEDFRAME);
	int cwH = 92 + 2 * GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CYSMCAPTION);

	if (hParent != NULL)
	{
		// center window over parent
		RECT rParentRect;
		GetWindowRect(hParent, &rParentRect);
		cwX = rParentRect.left + (rParentRect.right - rParentRect.left) / 2 - cwW / 2;
		cwY = rParentRect.top + (rParentRect.bottom - rParentRect.top) / 2 - cwH / 2;
	}

	// Create the window
	HWND hDialogWnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT, szWindowClass, L"CryENGINE RootPath", WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
	                                  cwX, cwY, cwW, cwH, hParent, NULL, GetModuleHandleW(0), NULL);

	// ------------------------------------------

	LoadEngineSettingsFromRegistry();

	HINSTANCE hInst = GetModuleHandleW(0);
	HGDIOBJ hDlgFont = GetStockObject(DEFAULT_GUI_FONT);

	// Engine Root Path

	HWND hStat0 = CreateWindowW(L"STATIC", INFOTEXT, WS_CHILD | WS_VISIBLE,
	                            8, 8, 380, 16, hDialogWnd, NULL, hInst, NULL);
	SendMessageW(hStat0, WM_SETFONT, (WPARAM)hDlgFont, FALSE);

	wchar_t buffer[512];
	GetValueByRef("ENG_RootPath", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer)));

	HWND hWndRCPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", buffer, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT | WS_TABSTOP | ES_READONLY,
	                                  8, 32, 344, 22, hDialogWnd, (HMENU)IDC_hEditRootPath, hInst, NULL);
	SendMessageW(hWndRCPath, WM_SETFONT, (WPARAM)hDlgFont, FALSE);

	m_hBtnBrowse = CreateWindowW(L"BUTTON", L"...", WS_CHILD | WS_VISIBLE,
	                             360, 32, 32, 22, hDialogWnd, (HMENU)IDC_hBtnBrowse, hInst, NULL);
	SendMessageW((HWND)m_hBtnBrowse, WM_SETFONT, (WPARAM)hDlgFont, FALSE);

	// std buttons

	HWND hWndOK = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP,
	                            130, 62, 70, 22, hDialogWnd, (HMENU)IDOK, hInst, NULL);
	SendMessageW(hWndOK, WM_SETFONT, (WPARAM)hDlgFont, FALSE);

	HWND hWndCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_CHILD | WS_VISIBLE | ES_LEFT | WS_TABSTOP,
	                                210, 62, 70, 22, hDialogWnd, (HMENU)IDCANCEL, hInst, NULL);
	SendMessageW(hWndCancel, WM_SETFONT, (WPARAM)hDlgFont, FALSE);

	SetFocus(hWndRCPath);

	// ------------------------------------------

	{
		MSG msg;

		while (!g_bWindowQuit)
		{
			GetMessageW(&msg, NULL, 0, 0);

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	// ------------------------------------------

	DestroyWindow(hDialogWnd);
	UnregisterClassW(szWindowClass, GetModuleHandleW(0));

	if (hParent)
	{
		if (bReEnableParent)
			EnableWindow(hParent, true);

		BringWindowToTop(hParent);
	}

	g_pThis = NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::SetParentDialog(unsigned long window)
{
	m_hWndParent = window;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::StoreData(bool bForceFileOutput)
{
	if (!bForceFileOutput && m_bGetDataFromRegistry)
	{
		bool res = StoreEngineSettingsToRegistry();

		if (!res)
			MessageBoxA((HWND)m_hWndParent, "Could not store data to registry.", "Error", MB_OK | MB_ICONERROR);
		return res;
	}

	// store data to INI file

	FILE* file;
	_wfopen_s(&file, m_sModuleFileName.c_str(), L"wb");
	if (file == NULL)
		return false;

	char buffer[2048];

	for (size_t i = 0; i < m_keyValueArray.size(); ++i)
	{
		const SKeyValue& kv = m_keyValueArray[i];

		fprintf_s(file, kv.key.c_str());
		fprintf_s(file, " = ");

		if (kv.value.length() > 0)
		{
			SettingsManagerHelpers::ConvertUtf16ToUtf8(kv.value.c_str(), SettingsManagerHelpers::CCharBuffer(buffer, sizeof(buffer)));
			fprintf_s(file, "%s", buffer);
		}

		fprintf_s(file, "\r\n");
	}

	fclose(file);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::LoadValuesFromConfigFile(const wchar_t* szFileName)
{
	m_keyValueArray.clear();

	// read file to memory

	FILE* file;
	_wfopen_s(&file, szFileName, L"rb");
	if (file == NULL)
		return false;

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	if (size < 0)
		return false;
	fseek(file, 0, SEEK_SET);
	char* data = new char[size + 1];
	fread_s(data, size, 1, size, file);
	fclose(file);

	wchar_t wBuffer[1024];

	// parse file for root path

	int start = 0, end = 0;
	while (end < size)
	{
		while (end < size && data[end] != '\n')
			end++;

		memcpy(data, &data[start], end - start);
		data[end - start] = 0;
		start = end = end + 1;

		CFixedString<char, 2048> line(data);
		size_t equalsOfs;
		for (equalsOfs = 0; equalsOfs < line.length(); ++equalsOfs)
		{
			if (line[equalsOfs] == '=')
			{
				break;
			}
		}
		if (equalsOfs < line.length())
		{
			CFixedString<char, 256> key;
			CFixedString<wchar_t, 1024> value;

			key.appendAscii(line.c_str(), equalsOfs);
			key.trim();

			SettingsManagerHelpers::ConvertUtf8ToUtf16(line.c_str() + equalsOfs + 1, SettingsManagerHelpers::CWCharBuffer(wBuffer, sizeof(wBuffer)));
			value.append(wBuffer);
			value.trim();

			// Stay compatible to deprecated rootpath key
			if (key.equals("RootPath"))
			{
				key = "ENG_RootPath";
				if (value[value.length() - 1] == '\\' || value[value.length() - 1] == '/')
				{
					value.set(value.c_str(), value.length() - 1);
				}
			}

			m_keyValueArray.set(key.c_str(), value.c_str());
		}
	}
	delete[] data;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const char* valueName, const wchar_t* value)
{
	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	size_t const sizeInBytes = (wcslen(value) + 1) * sizeof(value[0]);
	return (ERROR_SUCCESS == RegSetValueExW((HKEY)key, name.c_str(), 0, REG_SZ, (BYTE*)value, DWORD(sizeInBytes)));
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const char* valueName, bool value)
{
	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	DWORD dwVal = value;
	return (ERROR_SUCCESS == RegSetValueExW((HKEY)key, name.c_str(), 0, REG_DWORD, (BYTE*)&dwVal, sizeof(dwVal)));
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::SetRegValue(void* key, const char* valueName, int value)
{
	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	DWORD dwVal = value;
	return (ERROR_SUCCESS == RegSetValueExW((HKEY)key, name.c_str(), 0, REG_DWORD, (BYTE*)&dwVal, sizeof(dwVal)));
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const char* valueName, SettingsManagerHelpers::CWCharBuffer wbuffer)
{
	if (wbuffer.getSizeInElements() <= 0)
	{
		return false;
	}

	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	DWORD type;
	DWORD sizeInBytes = DWORD(wbuffer.getSizeInBytes());
	if (ERROR_SUCCESS != RegQueryValueExW((HKEY)key, name.c_str(), NULL, &type, (BYTE*)wbuffer.getPtr(), &sizeInBytes))
	{
		wbuffer[0] = 0;
		return false;
	}

	const size_t sizeInElements = sizeInBytes / sizeof(wbuffer[0]);
	if (sizeInElements > wbuffer.getSizeInElements()) // paranoid check
	{
		wbuffer[0] = 0;
		return false;
	}

	// According to MSDN documentation for RegQueryValueEx(), strings returned by the function
	// are not zero-terminated sometimes, so we need to terminate them by ourselves.
	if (wbuffer[sizeInElements - 1] != 0)
	{
		if (sizeInElements >= wbuffer.getSizeInElements())
		{
			// No space left to put terminating zero character
			wbuffer[0] = 0;
			return false;
		}
		wbuffer[sizeInElements] = 0;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const char* valueName, bool& value)
{
	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	// Open the appropriate registry key
	DWORD type, dwVal = 0, size = sizeof(dwVal);
	bool res = (ERROR_SUCCESS == RegQueryValueExW((HKEY)key, name.c_str(), NULL, &type, (BYTE*)&dwVal, &size));
	if (res)
	{
		value = (dwVal != 0);
	}
	else
	{
		wchar_t buffer[100];
		res = GetRegValue(key, valueName, SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer)));
		if (res)
		{
			value = (wcscmp(buffer, L"true") == 0);
		}
	}
	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetRegValue(void* key, const char* valueName, int& value)
{
	SettingsManagerHelpers::CFixedString<wchar_t, 256> name;
	name.appendAscii(valueName);

	// Open the appropriate registry key
	DWORD type, dwVal = 0, size = sizeof(dwVal);

	bool res = (ERROR_SUCCESS == RegQueryValueExW((HKEY)key, name.c_str(), NULL, &type, (BYTE*)&dwVal, &size));
	if (res)
		value = dwVal;

	return res;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::StoreEngineSettingsToRegistry()
{
	if (!m_bGetDataFromRegistry)
		return true;

	// make sure the path in registry exists

	RegKey key1(L"Software\\Crytek", true);
	if (!key1.pKey)
	{
		RegKey key0(L"Software", true);
		HKEY hKey1;
		RegCreateKeyA((HKEY)key0.pKey, "Crytek", &hKey1);
		if (!hKey1)
			return false;
	}

	RegKey key2(L"Software\\Crytek\\Settings", true);
	if (!key2.pKey)
	{
		RegKey key1(L"Software\\Crytek", true);
		HKEY hKey2;
		RegCreateKeyA((HKEY)key1.pKey, "Settings", &hKey2);
		if (!hKey2)
			return false;
	}

	bool bRet = true;

	RegKey key(L"Software\\Crytek\\Settings", true);
	if (!key.pKey)
	{
		bRet = false;
	}
	else
	{
		wchar_t buffer[1024];

		// Engine Specific

		if (GetValueByRef("ENG_RootPath", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
		{
			bRet &= SetRegValue(key.pKey, "ENG_RootPath", buffer);
		}
		else
		{
			bRet = false;
		}

		// ResourceCompiler Specific

		if (GetValueByRef("RC_ShowWindow", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
		{
			const bool b = wcscmp(buffer, L"true") == 0;
			SetRegValue(key.pKey, "RC_ShowWindow", b);
		}

		if (GetValueByRef("RC_HideCustom", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
		{
			const bool b = wcscmp(buffer, L"true") == 0;
			SetRegValue(key.pKey, "RC_HideCustom", b);
		}

		if (GetValueByRef("RC_Parameters", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
		{
			SetRegValue(key.pKey, "RC_Parameters", buffer);
		}
	}

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::StoreLicenseSettingsToRegistry()
{
	if (!m_bGetDataFromRegistry)
		return true;

	bool bRet = true;

	// make sure the path in registry exists

	RegKey key1(REG_SOFTWARE REG_COMPANY_NAME, true);
	if (!key1.pKey)
	{
		RegKey key0(REG_SOFTWARE, true);
		HKEY hKey1;
		RegCreateKeyW((HKEY)key0.pKey, REG_COMPANY_NAME, &hKey1);
		if (!hKey1)
			return false;
	}

	RegKey key2(REG_CRYTEK_SETTING_KEY, true);
	if (!key2.pKey)
	{
		RegKey key1(REG_SOFTWARE REG_COMPANY_NAME, true);
		HKEY hKey2;
		RegCreateKeyW((HKEY)key1.pKey, REG_SETTING, &hKey2);
		if (!hKey2)
			return false;
	}

	RegKey key(REG_CRYTEK_SETTING_KEY, true);
	if (!key.pKey)
	{
		bRet = false;
	}

	return bRet;
}

//////////////////////////////////////////////////////////////////////////
void CEngineSettingsManager::LoadEngineSettingsFromRegistry()
{
	if (!m_bGetDataFromRegistry)
		return;

	wchar_t buffer[1024];

	bool bResult;

	// Engine Specific (Deprecated value)
	RegKey key(L"Software\\Crytek\\Settings", false);
	if (key.pKey)
	{
		// Engine Specific
		if (GetRegValue(key.pKey, "ENG_RootPath", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
			SetKey("ENG_RootPath", buffer);

		// Engine Specific
		if (GetRegValue(key.pKey, "ENG_BinPath", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
			SetKey("ENG_BinPath", buffer);

		// ResourceCompiler Specific
		if (GetRegValue(key.pKey, "RC_ShowWindow", bResult))
			SetKey("RC_ShowWindow", bResult);
		if (GetRegValue(key.pKey, "RC_HideCustom", bResult))
			SetKey("RC_HideCustom", bResult);
		if (GetRegValue(key.pKey, "RC_Parameters", SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
			SetKey("RC_Parameters", buffer);
	}
}

//////////////////////////////////////////////////////////////////////////
long CEngineSettingsManager::HandleProc(void* pWnd, long uMsg, long wParam, long lParam)
{
	HWND hWnd = (HWND)pWnd;

	switch (uMsg)
	{
	case WM_CREATE:
		break;

	case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case IDC_hBtnBrowse:
				{
					HWND hWndEdit = GetDlgItem(hWnd, IDC_hEditRootPath);
					wchar_t target[MAX_PATH];
					BrowseForFolder(hWnd, NULL, target, INFOTEXT);
					SetWindowTextW(hWndEdit, target);
				}
				break;

			case IDOK:
				{
					HWND hItemWnd = GetDlgItem(hWnd, IDC_hEditRootPath);
					wchar_t szPath[MAX_PATH];
					GetWindowTextW(hItemWnd, szPath, MAX_PATH);
					SetRootPath(szPath);
					StoreData(false);
				}

			case IDCANCEL:
				{
					g_bWindowQuit = true;
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		{
			g_bWindowQuit = true;
			break;
		}
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////
BOOL BrowseForFolder(HWND hWnd, LPCWSTR szInitialPath, LPWSTR szPath, LPCWSTR szTitle)
{
	wchar_t szDisplay[MAX_PATH];

	PREFAST_SUPPRESS_WARNING(6031)
	CoInitialize(NULL);

	BROWSEINFOW bi = { 0 };
	bi.hwndOwner = hWnd;
	bi.pszDisplayName = szDisplay;
	bi.lpszTitle = szTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = NULL;
	bi.lParam = (LPARAM)szInitialPath;

	LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);

	if (pidl != NULL)
	{
		BOOL retval = SHGetPathFromIDListW(pidl, szPath);
		CoTaskMemFree(pidl);
		CoUninitialize();
		return TRUE;
	}

	szPath[0] = 0;
	CoUninitialize();
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetValueByRef(const char* key, SettingsManagerHelpers::CWCharBuffer wbuffer) const
{
	if (wbuffer.getSizeInElements() <= 0)
	{
		return false;
	}

	const SKeyValue* p = m_keyValueArray.find(key);
	if (!p || (p->value.length() + 1) > wbuffer.getSizeInElements())
	{
		wbuffer[0] = 0;
		return false;
	}
	wcscpy(wbuffer.getPtr(), p->value.c_str());
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetValueByRef(const char* key, bool& value) const
{
	wchar_t buffer[100];
	if (!GetValueByRef(key, SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
	{
		return false;
	}
	value = (wcscmp(buffer, L"true") == 0);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEngineSettingsManager::GetValueByRef(const char* key, int& value) const
{
	wchar_t buffer[100];
	if (!GetValueByRef(key, SettingsManagerHelpers::CWCharBuffer(buffer, sizeof(buffer))))
	{
		return false;
	}
	value = wcstol(buffer, 0, 10);
	return true;
}

#endif //(CRY_ENABLE_RC_HELPER)
