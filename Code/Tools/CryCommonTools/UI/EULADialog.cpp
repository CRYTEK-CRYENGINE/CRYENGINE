// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EULADialog.h"
#include "Win32GUI.h"
#include "ModuleHelpers.h"
#include "Richedit.h"
#include <Windows.h>

EULADialog::EULADialog()
:	m_frameWindow(),
	m_cancelButton(_T("Cancel"), this, &EULADialog::CancelPressed),
	m_buttonSpacer(0, 0, 2000, 0),
	m_acceptButton(_T("Accept"), this, &EULADialog::AcceptPressed),
	m_buttonLayout(Layout::DirectionHorizontal),
	m_edit()
{
	Win32GUI::Initialize();

	m_buttonLayout.AddComponent(&m_buttonSpacer);
	m_buttonLayout.AddComponent(&m_cancelButton);
	m_buttonLayout.AddComponent(&m_acceptButton);

	m_frameWindow.AddComponent(&m_edit);
	m_frameWindow.AddComponent(&m_buttonLayout);
}

namespace
{
	class EditStreamCallbackObject
	{
	public:
		EditStreamCallbackObject(const char* data, int size): data(data), position(0), size(size) {}

		static DWORD WINAPI EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
		{
			return reinterpret_cast<EditStreamCallbackObject*>((uintptr_t)dwCookie)->EditStreamCallback_Member(dwCookie, pbBuff, cb, pcb);
		}

	private:
		DWORD EditStreamCallback_Member(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG* pcb)
		{
			int bytesToRead = (std::min)(this->size - this->position, (int)cb);
			std::memcpy(pbBuff, this->data + this->position, bytesToRead);
			this->position += bytesToRead;
			if (pcb)
				*pcb = bytesToRead;
			return 0;
		}

		const char* data;
		int position;
		int size;
	};
}

EULADialog::UserResponse EULADialog::Run(int width, int height, TCHAR* resourceID)
{
	m_frameWindow.Show(true, width, height);

	// Attempt to load the resource.
	HINSTANCE module = ModuleHelpers::GetCurrentModule(ModuleHelpers::CurrentModuleSpecifier_Library);
	HRSRC resource = (resourceID ? FindResource(module, resourceID, RT_RCDATA) : 0);
	int resourceLength = (resource ? SizeofResource(module, resource) : 0);
	HGLOBAL resourceGlobal = (resource ? LoadResource(module, resource) : 0);
	void* resourceData = (resourceGlobal ? LockResource(resourceGlobal) : 0);
	// No need to unlock/delete data.

	m_userResponse = UserResponseNone;

	// Load the text.
	if (resourceData && resourceLength > 0)
	{
		EditStreamCallbackObject callbackObject((const char*)resourceData, resourceLength);
		EDITSTREAM editStream;
		std::memset(&editStream, 0, sizeof(editStream));
		editStream.dwCookie = (DWORD_PTR)&callbackObject;
		editStream.pfnCallback = &EditStreamCallbackObject::EditStreamCallback;
		SendMessage((HWND)m_edit.m_edit, EM_STREAMIN, SF_RTF, (LPARAM)&editStream);
	}

	MSG msg;
	BOOL status;
	while (m_userResponse == UserResponseNone && (status = GetMessage(&msg, HWND(0), UINT(0), UINT(0))) != 0)
	{
		if (status == -1)
		{
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	m_frameWindow.Show(false, 0, 0);

	return m_userResponse;
}

void EULADialog::CancelPressed()
{
	m_userResponse = UserResponseCancel;
}

void EULADialog::AcceptPressed()
{
	m_userResponse = UserResponseAccept;
}

EULADialog::UserResponse EULADialog::Show(int width, int height, TCHAR* resourceID)
{
	EULADialog dlg;
	return dlg.Run(width, height, resourceID);
}
