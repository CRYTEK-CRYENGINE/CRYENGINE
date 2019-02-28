// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOTOFUNCWND_H__
#define __GOTOFUNCWND_H__

#pragma once

#include "LUADBG.h"

class CGoToFuncWnd : public _TinyDialog
{
public:
	BOOL Create(_TinyWindow* pParent = NULL)
	{
		return _TinyDialog::Create(MAKEINTRESOURCE(IDD_GOTO_FUNC), pParent);
	}
	BOOL DoModal(_TinyWindow* pParent = NULL)
	{
		m_pLUADbg = (CLUADbg*)pParent;
		return _TinyDialog::DoModal(MAKEINTRESOURCE(IDD_GOTO_FUNC), pParent);
	}

protected:
	_BEGIN_DLG_MSG_MAP(_TinyDialog)
	_MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
	_BEGIN_COMMAND_HANDLER()
	_COMMAND_HANDLER(IDOK, OnOk)
	_COMMAND_HANDLER(IDCANCEL, OnCancel)
	_COMMAND_HANDLER(IDC_LIST_FUNC, OnNotifyList)
	_END_COMMAND_HANDLER()
	_END_DLG_MSG_MAP()

	LRESULT OnInit(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		::SendMessage(::GetDlgItem(hWnd, IDC_LIST_FUNC), LB_RESETCONTENT, 0, 0);
		::SendMessage(hWnd, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(hWnd, IDC_LIST_FUNC), TRUE);
		//
		string strSourceCode = m_pLUADbg->GetCurrentSourceFile();
		strSourceCode.replace("\r", "");
		//
		const char* sourceCode = strSourceCode.c_str();
		const char* lineStart = sourceCode;
		int numLines = 0;
		while ((sourceCode = (strchr(sourceCode, '\n'))) != NULL)
		{
			string line = string(lineStart, sourceCode - lineStart);
			string::size_type functionPos = line.find("function");
			if (functionPos != string::npos)
			{
				string::size_type commentPos = line.find("--");
				if (functionPos < commentPos)
				{
					const char* lineWithoutLeadingSpaces = line.c_str() + line.find_first_not_of(" \t");
					LRESULT index = ::SendMessage(::GetDlgItem(hWnd, IDC_LIST_FUNC), LB_ADDSTRING, 0, (LPARAM)lineWithoutLeadingSpaces);
					::SendMessage(::GetDlgItem(hWnd, IDC_LIST_FUNC), LB_SETITEMDATA, index, numLines);
				}
			}
			//
			sourceCode++;// to skip \n
			//
			lineStart = sourceCode;
			//
			numLines++;
		}
		return 0;
	}

	LRESULT OnNotifyList(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (HIWORD(wParam) == LBN_DBLCLK)
		{
			OnOk(m_hWnd, 0, 0, 0);
		}
		return 0;
	}

	LRESULT OnOk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (m_pLUADbg)
		{
			LRESULT selIndex = ::SendMessage(::GetDlgItem(hWnd, IDC_LIST_FUNC), LB_GETCURSEL, 0, 0);
			if (selIndex != LB_ERR)
			{
				LRESULT lineIndex = ::SendMessage(::GetDlgItem(hWnd, IDC_LIST_FUNC), LB_GETITEMDATA, selIndex, 0);
				m_pLUADbg->PlaceLineMarker(UINT(lineIndex + 1));
			}
		}
		_TinyVerify(EndDialog(hWnd, 0));

		return 0;
	}

	LRESULT OnCancel(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		_TinyVerify(EndDialog(hWnd, 0));
		return 0;
	}

	CLUADbg* m_pLUADbg;
};

#endif
