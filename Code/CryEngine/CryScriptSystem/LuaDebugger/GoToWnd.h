// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GOTOWND_H__
#define __GOTOWND_H__

#pragma once

#include "LUADBG.h"

class CGoToWnd : public _TinyDialog
{
public:
	BOOL Create(_TinyWindow* pParent = NULL)
	{
		return _TinyDialog::Create(MAKEINTRESOURCE(IDD_GOTO), pParent);
	};
	BOOL DoModal(_TinyWindow* pParent = NULL)
	{
		m_pLUADbg = (CLUADbg*)pParent;
		return _TinyDialog::DoModal(MAKEINTRESOURCE(IDD_GOTO), pParent);
	};

protected:
	_BEGIN_DLG_MSG_MAP(_TinyDialog)
	_MESSAGE_HANDLER(WM_INITDIALOG, OnInit)
	_BEGIN_COMMAND_HANDLER()
	_COMMAND_HANDLER(IDOK, OnOk)
	_COMMAND_HANDLER(IDCANCEL, OnCancel)
	_END_COMMAND_HANDLER()
	_END_DLG_MSG_MAP()

	LRESULT OnInit(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		::SendMessage(hWnd, WM_NEXTDLGCTL, (WPARAM)::GetDlgItem(hWnd, IDC_EDIT1), TRUE);
		return 0;
	}

	LRESULT OnOk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (m_pLUADbg)
		{
			const char buffer[256] = { 0 };
			// get line number
			::SendMessage(::GetDlgItem(hWnd, IDC_EDIT1), WM_GETTEXT, 256, (LPARAM)buffer);
			m_pLUADbg->PlaceLineMarker(atoi(buffer));
		}
		_TinyVerify(EndDialog(hWnd, 0));

		return 0;
	};

	LRESULT OnCancel(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		_TinyVerify(EndDialog(hWnd, 0));
		return 0;
	};

	CLUADbg* m_pLUADbg;
};

#endif
