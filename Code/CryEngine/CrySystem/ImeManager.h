// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySystem/IImeManager.h>
#include <CrySystem/IWindowMessageHandler.h>

// IME manager utility
// This class is responsible for handling IME specific window messages
class CImeManager : public IImeManager, public IWindowMessageHandler
{
	// No copy/assign
	CImeManager(const CImeManager&);
	void operator=(const CImeManager&);

public:
	CImeManager();
	virtual ~CImeManager();

	// Check if IME is supported
	virtual bool IsImeSupported() { return m_pScaleformMessageHandler != NULL; }

	// This is called by Scaleform in the case that IME support is compiled in
	// Returns false if IME should not be used
	virtual bool SetScaleformHandler(IWindowMessageHandler* pHandler);

#if CRY_PLATFORM_WINDOWS
	// IWindowMessageHandler
	virtual void PreprocessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual bool HandleMessage(HWND hWnd, UINT UMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	// ~IWindowMessageHandler
#endif

private:
	IWindowMessageHandler* m_pScaleformMessageHandler; // If Scaleform IME is enabled, this is not NULL
};
