// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryString/CryPath.h>
#include <CryInput/IInput.h>
#include <CryCore/Project/CryModuleDefs.h>

#define IDC_CRYASSERT_EDIT_LINE         1000
#define IDC_CRYASSERT_EDIT_FILE         1001
#define IDC_CRYASSERT_EDIT_CONDITION    1002
#define IDC_CRYASSERT_BUTTON_CONTINUE   1003
#define IDC_CRYASSERT_EDIT_REASON       1004
#define IDC_CRYASSERT_BUTTON_IGNORE     1005
#define IDC_CRYASSERT_BUTTON_REPORT     1006
#define IDC_CRYASSERT_BUTTON_STOP       1007
#define IDC_CRYASSERT_BUTTON_BREAK      1008
#define IDC_CRYASSERT_BUTTON_IGNORE_ALL 1009
#define IDC_CRYASSERT_BUTTON_MODULE     1010

#define IDC_CRYASSERT_STATIC_TEXT       0

#define DLG_TITLE                       L"Assertion Failed"
#define DLG_FONT                        L"MS Sans Serif"
#define DLG_ITEM_TEXT_0                 L"Continue"
#define DLG_ITEM_TEXT_1                 L"Stop"
#define DLG_ITEM_TEXT_2                 L"Info"
#define DLG_ITEM_TEXT_3                 L""
#define DLG_ITEM_TEXT_4                 L"Line"
#define DLG_ITEM_TEXT_5                 L""
#define DLG_ITEM_TEXT_6                 L"File"
#define DLG_ITEM_TEXT_7                 L"Condition"
#define DLG_ITEM_TEXT_8                 L""
#define DLG_ITEM_TEXT_9                 L"failed"
#define DLG_ITEM_TEXT_10                L""
#define DLG_ITEM_TEXT_11                L"Reason"
#define DLG_ITEM_TEXT_12                L"Ignore"
#define DLG_ITEM_TEXT_13                L"Report as Bug"
#define DLG_ITEM_TEXT_14                L"Break"
#define DLG_ITEM_TEXT_15                L"Ignore All"

namespace Cry {
	namespace Assert {

		template<int iTitleSize>
		struct SDlgItem
		{
			// If use my struct instead of DLGTEMPLATE, or else (for some strange reason) it is not DWORD aligned !!
			DWORD style;
			DWORD dwExtendedStyle;
			short x;
			short y;
			short cx;
			short cy;
			WORD  id;
			WORD  ch;
			WORD  c;
			WCHAR t[iTitleSize];
			WORD  dummy;
		};
#define SDLGITEM(TEXT, V) SDlgItem<sizeof(TEXT) / 2> V;

		struct SDlgData
		{
			DLGTEMPLATE dlt;
			WORD        _menu;
			WORD        _class;
			WCHAR       _title[sizeof(DLG_TITLE) / 2];
			WORD        pointSize;
			WCHAR       _font[sizeof(DLG_FONT) / 2];

			SDLGITEM(DLG_ITEM_TEXT_0, i0);
			SDLGITEM(DLG_ITEM_TEXT_12, i12);
			SDLGITEM(DLG_ITEM_TEXT_15, i15);
			SDLGITEM(DLG_ITEM_TEXT_14, i14);
			SDLGITEM(DLG_ITEM_TEXT_1, i1);
			SDLGITEM(DLG_ITEM_TEXT_2, i2);
			SDLGITEM(DLG_ITEM_TEXT_3, i3);
			SDLGITEM(DLG_ITEM_TEXT_4, i4);
			SDLGITEM(DLG_ITEM_TEXT_5, i5);
			SDLGITEM(DLG_ITEM_TEXT_6, i6);
			SDLGITEM(DLG_ITEM_TEXT_7, i7);
			SDLGITEM(DLG_ITEM_TEXT_8, i8);
			SDLGITEM(DLG_ITEM_TEXT_9, i9);
			SDLGITEM(DLG_ITEM_TEXT_10, i10);
			SDLGITEM(DLG_ITEM_TEXT_11, i11);
			SDLGITEM(DLG_ITEM_TEXT_13, i13);
#if defined(eCryModule)
			SDlgItem<256> i16;
#endif
		};

		//-----------------------------------------------------------------------------------------------------

		struct SCryAssertInfo
		{
			const char*  pszCondition;
			const char*  pszFile;
			const char*  pszMessage;
			unsigned int uiLine;

			enum
			{
				BUTTON_CONTINUE,
				BUTTON_IGNORE,
				BUTTON_IGNORE_ALL,
				BUTTON_BREAK,
				BUTTON_STOP,
				BUTTON_REPORT_AS_BUG,
			}            btnChosen;

			unsigned int uiX;
			unsigned int uiY;
		};

		//-----------------------------------------------------------------------------------------------------

		static thread_local SCryAssertInfo* gs_pAssertInfo;

		static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
		{
			switch (uiMsg)
			{
			case WM_INITDIALOG:
			{
				gs_pAssertInfo = (SCryAssertInfo*)lParam;

				char buf[MAX_PATH];
				const bool bFolded = PathUtil::SimplifyFilePath(gs_pAssertInfo->pszFile, buf, MAX_PATH, PathUtil::ePathStyle_Windows);

				SetWindowTextA(GetDlgItem(hDlg, IDC_CRYASSERT_EDIT_CONDITION), gs_pAssertInfo->pszCondition);
				SetWindowTextA(GetDlgItem(hDlg, IDC_CRYASSERT_EDIT_FILE), bFolded ? buf : gs_pAssertInfo->pszFile);

				char szLine[MAX_PATH];
				cry_sprintf(szLine, "%d", gs_pAssertInfo->uiLine);
				SetWindowTextA(GetDlgItem(hDlg, IDC_CRYASSERT_EDIT_LINE), szLine);

				if (gs_pAssertInfo->pszMessage && gs_pAssertInfo->pszMessage[0] != '\0')
				{
					SetWindowTextA(GetDlgItem(hDlg, IDC_CRYASSERT_EDIT_REASON), gs_pAssertInfo->pszMessage);
				}
				else
				{
					SetWindowTextA(GetDlgItem(hDlg, IDC_CRYASSERT_EDIT_REASON), "No Reason");
				}

				SetWindowPos(hDlg, HWND_TOPMOST, gs_pAssertInfo->uiX, gs_pAssertInfo->uiY, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE);
			}
			break;

			case WM_COMMAND:
			{
				switch (LOWORD(wParam))
				{
				case IDCANCEL:
				case IDC_CRYASSERT_BUTTON_CONTINUE:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_CONTINUE;
					EndDialog(hDlg, 0);
					break;

				case IDC_CRYASSERT_BUTTON_IGNORE:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_IGNORE;
					EndDialog(hDlg, 0);
					break;

				case IDC_CRYASSERT_BUTTON_IGNORE_ALL:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_IGNORE_ALL;
					EndDialog(hDlg, 0);
					break;

				case IDC_CRYASSERT_BUTTON_BREAK:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_BREAK;
					EndDialog(hDlg, 0);
					break;

				case IDC_CRYASSERT_BUTTON_STOP:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_STOP;
					EndDialog(hDlg, 1);
					break;

				case IDC_CRYASSERT_BUTTON_REPORT:
					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_REPORT_AS_BUG;
					EndDialog(hDlg, 0);
					break;

				case IDC_CRYASSERT_BUTTON_MODULE:
#ifdef eCryModule
					Cry::Assert::DisableAssertionsForModule(eCryModule);
#endif

					gs_pAssertInfo->btnChosen = SCryAssertInfo::BUTTON_IGNORE;
					EndDialog(hDlg, 0);
					break;
				}
				if (gEnv && gEnv->pInput)
				{
					gEnv->pInput->ClearKeyState();
				}
			}
			break;

			case WM_DESTROY:
			{
				if (gs_pAssertInfo)
				{
					RECT rcWindowBounds;
					GetWindowRect(hDlg, &rcWindowBounds);
					gs_pAssertInfo->uiX = rcWindowBounds.left;
					gs_pAssertInfo->uiY = rcWindowBounds.top;
				}
			}
			break;

			default:
				return FALSE;
			}

			return TRUE;
		}

		//-----------------------------------------------------------------------------------------------------

		static const char* gs_strRegSubKey = "Software\\Crytek\\AssertWindow";
		static const char* gs_strRegXValue = "AssertInfoX";
		static const char* gs_strRegYValue = "AssertInfoY";

		//-----------------------------------------------------------------------------------------------------

		void RegistryReadUInt32(const char* strSubKey, const char* strRegName, unsigned int* puiValue, unsigned int uiDefault)
		{
			HKEY hKey;
			RegCreateKeyExA(HKEY_CURRENT_USER, strSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);

			DWORD dwType;
			DWORD dwLength = sizeof(DWORD);

			if (ERROR_SUCCESS != RegQueryValueExA(hKey, strRegName, 0, &dwType, (BYTE*)puiValue, &dwLength))
			{
				*puiValue = uiDefault;
			}

			RegCloseKey(hKey);
		}

		//-----------------------------------------------------------------------------------------------------

		void RegistryWriteUInt32(const char* strSubKey, const char* strRegName, unsigned int uiValue)
		{
			HKEY hKey;
			RegCreateKeyExA(HKEY_CURRENT_USER, strSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
			RegSetValueExA(hKey, strRegName, 0, REG_DWORD, (BYTE*)&uiValue, sizeof(DWORD));
			RegCloseKey(hKey);
		}

		//-----------------------------------------------------------------------------------------------------

		class CCursorShowerWithStack
		{
		public:
			void StoreCurrentAndShow()
			{
				m_numberOfShows = 1;

				if (gEnv && gEnv->pInput)
				{
					while (gEnv->pInput->ShowCursor(true) < 0)
					{
						++m_numberOfShows;
					}
				}
			}

			void RevertToPrevious()
			{
				while (m_numberOfShows > 0)
				{
					if (gEnv && gEnv->pInput)
					{
						gEnv->pInput->ShowCursor(false);
					}
					--m_numberOfShows;
				}
			}

		private:
			int m_numberOfShows;
		};

		bool CryAssert(const char* szCondition, const char* szFile, unsigned int line, bool* pIgnore)
		{
			if (Cry::Assert::ShowDialogOnAssert() && !Cry::Assert::IgnoreAllAsserts())
			{
				SCryAssertInfo assertInfo;

				assertInfo.pszCondition = szCondition;
				assertInfo.pszFile = szFile;
				assertInfo.pszMessage = tls_szAssertMessage;
				assertInfo.uiLine = line;
				assertInfo.btnChosen = SCryAssertInfo::BUTTON_CONTINUE;

				RegistryReadUInt32(gs_strRegSubKey, gs_strRegXValue, &assertInfo.uiX, 10);
				RegistryReadUInt32(gs_strRegSubKey, gs_strRegYValue, &assertInfo.uiY, 10);

				CCursorShowerWithStack cursorShowerWithStack;
				cursorShowerWithStack.StoreCurrentAndShow();

#if defined(eCryModule)
				SDlgItem<256> currentModuleItem = { BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | (gEnv ? 0 : WS_DISABLED) | WS_TABSTOP, 0, 165, 113, 100, 14, IDC_CRYASSERT_BUTTON_MODULE, 0xFFFF, 0x0080 };

				wstring message = L"Ignore All From ";
				message += CryStringUtils::UTF8ToWStr(GetCryModuleName(eCryModule));
				cry_strcpy_wchar(currentModuleItem.t, message.c_str());
#else
				SDlgItem<256> currentModuleItem = { BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_TABSTOP, 0, 165, 113, 100, 14, IDC_CRYASSERT_BUTTON_MODULE, 0xFFFF, 0x0080 };
				cry_strcpy_wchar(currentModuleItem.t, L"Ignore All From <No Module>");
#endif
				const int numEntries = 17;

				SDlgData dialog =
				{
					{ DS_SETFOREGROUND | DS_MODALFRAME | DS_3DLOOK | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 0, numEntries, 0,   0,   380, 134 },
					0, 0, DLG_TITLE, 8, DLG_FONT,
					{ BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,                                                                      0, 7,           113, 50,  14,  IDC_CRYASSERT_BUTTON_CONTINUE, 0xFFFF, 0x0080, DLG_ITEM_TEXT_0, 0 },
					{ BS_DEFPUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,                                                                   0, 60,          113, 50,  14,  IDC_CRYASSERT_BUTTON_IGNORE, 0xFFFF, 0x0080, DLG_ITEM_TEXT_12, 0 },
					{ BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | (gEnv ? 0 : WS_DISABLED) | WS_TABSTOP,                                           0, 113,         113, 50,  14,  IDC_CRYASSERT_BUTTON_IGNORE_ALL, 0xFFFF, 0x0080, DLG_ITEM_TEXT_15, 0 },
					{ BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,                                                                      0, 271,         113, 50,  14,  IDC_CRYASSERT_BUTTON_BREAK, 0xFFFF, 0x0080, DLG_ITEM_TEXT_14, 0 },
					{ BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,                                                                      0, 324,         113, 50,  14,  IDC_CRYASSERT_BUTTON_STOP, 0xFFFF, 0x0080, DLG_ITEM_TEXT_1, 0 },
					{ BS_GROUPBOX | WS_CHILD | WS_VISIBLE,                                                                                     0, 7,           7,   366, 100, IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0080, DLG_ITEM_TEXT_2, 0 },
					{ ES_LEFT | ES_AUTOHSCROLL | ES_READONLY | WS_BORDER | WS_CHILD | WS_VISIBLE,                                              0, 50,          48,  25,  13,  IDC_CRYASSERT_EDIT_LINE, 0xFFFF, 0x0081, DLG_ITEM_TEXT_3, 0 },
					{ WS_CHILD | WS_VISIBLE,                                                                                                   0, 14,          50,  14,  8,   IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0082, DLG_ITEM_TEXT_4, 0 },
					{ ES_LEFT | ES_AUTOHSCROLL | ES_READONLY | WS_BORDER | WS_CHILD | WS_VISIBLE,                                              0, 50,          32,  290, 13,  IDC_CRYASSERT_EDIT_FILE, 0xFFFF, 0x0081, DLG_ITEM_TEXT_5, 0 },
					{ WS_CHILD | WS_VISIBLE,                                                                                                   0, 14,          34,  12,  8,   IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0082, DLG_ITEM_TEXT_6, 0 },
					{ WS_CHILD | WS_VISIBLE,                                                                                                   0, 13,          18,  30,  8,   IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0082, DLG_ITEM_TEXT_7, 0 },
					{ ES_LEFT | ES_AUTOHSCROLL | ES_READONLY | WS_BORDER | WS_CHILD | WS_VISIBLE,                                              0, 50,          16,  290, 13,  IDC_CRYASSERT_EDIT_CONDITION, 0xFFFF, 0x0081, DLG_ITEM_TEXT_8, 0 },
					{ WS_CHILD | WS_VISIBLE,                                                                                                   0, 348,         19,  18,  8,   IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0082, DLG_ITEM_TEXT_9, 0 },
					{ ES_LEFT | ES_AUTOHSCROLL | ES_READONLY | WS_BORDER | WS_CHILD | WS_VISIBLE,                                              0, 50,          67,  290, 13,  IDC_CRYASSERT_EDIT_REASON, 0xFFFF, 0x0081, DLG_ITEM_TEXT_10, 0 },
					{ WS_CHILD | WS_VISIBLE,                                                                                                   0, 15,          69,  26,  8,   IDC_CRYASSERT_STATIC_TEXT, 0xFFFF, 0x0082, DLG_ITEM_TEXT_11, 0 },
					{ BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE,                                                                                   0, 127,         88,  72,  12,  IDC_CRYASSERT_BUTTON_REPORT, 0xFFFF, 0x0080, DLG_ITEM_TEXT_13, 0 },
		#if defined(eCryModule)
					currentModuleItem
		#endif
				};

				const INT_PTR dialogResult = DialogBoxIndirectParam(CryGetCurrentModule(), (DLGTEMPLATE*)&dialog, GetDesktopWindow(), DlgProc, (LPARAM)&assertInfo);
				if (dialogResult != 0 && dialogResult != 1) // 0 is success, 1 is is used by Stop command
				{
					// opening the dialog failed: don't show further dialogs, but break on this one
					Cry::Assert::ShowDialogOnAssert(false);
					return true;
				}

				cursorShowerWithStack.RevertToPrevious();

				RegistryWriteUInt32(gs_strRegSubKey, gs_strRegXValue, assertInfo.uiX);
				RegistryWriteUInt32(gs_strRegSubKey, gs_strRegYValue, assertInfo.uiY);

				switch (assertInfo.btnChosen)
				{
				case SCryAssertInfo::BUTTON_CONTINUE:
					break;
				case SCryAssertInfo::BUTTON_IGNORE_ALL:
					Cry::Assert::IgnoreAllAsserts(true);
					// intentional fall through
				case SCryAssertInfo::BUTTON_IGNORE:
					*pIgnore = true;
					break;
				case SCryAssertInfo::BUTTON_BREAK:
					return true;
				case SCryAssertInfo::BUTTON_STOP:
					abort();
					break;
				case SCryAssertInfo::BUTTON_REPORT_AS_BUG:
					if (gEnv && gEnv->pSystem)
					{
						gEnv->pSystem->ReportBug("Assert: %s - %s", assertInfo.pszCondition, tls_szAssertMessage);
					}
					return true;
				}
			}
			return false;
		}
	}
}

#undef IDC_CRYASSERT_EDIT_LINE
#undef IDC_CRYASSERT_EDIT_FILE
#undef IDC_CRYASSERT_EDIT_CONDITION
#undef IDC_CRYASSERT_BUTTON_CONTINUE
#undef IDC_CRYASSERT_EDIT_REASON
#undef IDC_CRYASSERT_BUTTON_IGNORE
#undef IDC_CRYASSERT_BUTTON_REPORT
#undef IDC_CRYASSERT_BUTTON_STOP
#undef IDC_CRYASSERT_BUTTON_BREAK
#undef IDC_CRYASSERT_BUTTON_IGNORE_ALL
#undef IDC_CRYASSERT_BUTTON_MODULE

#undef IDC_CRYASSERT_STATIC_TEXT

#undef DLG_TITLE
#undef DLG_FONT
#undef DLG_ITEM_TEXT_0
#undef DLG_ITEM_TEXT_1
#undef DLG_ITEM_TEXT_2
#undef DLG_ITEM_TEXT_3
#undef DLG_ITEM_TEXT_4
#undef DLG_ITEM_TEXT_5
#undef DLG_ITEM_TEXT_6
#undef DLG_ITEM_TEXT_7
#undef DLG_ITEM_TEXT_8
#undef DLG_ITEM_TEXT_9
#undef DLG_ITEM_TEXT_10
#undef DLG_ITEM_TEXT_11
#undef DLG_ITEM_TEXT_12
#undef DLG_ITEM_TEXT_13
#undef DLG_ITEM_TEXT_14
#undef DLG_ITEM_TEXT_15	  
