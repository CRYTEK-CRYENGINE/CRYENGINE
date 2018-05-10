// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ImeManager.h"

#define IME_CVAR_NAME "sys_ime"
const int cDefaultImeEnabled = 0;

// Constructor
CImeManager::CImeManager() : m_pScaleformMessageHandler(NULL)
{
	assert(gEnv && gEnv->pSystem && gEnv->pConsole);
	gEnv->pSystem->RegisterWindowMessageHandler(this);
	REGISTER_INT(IME_CVAR_NAME, cDefaultImeEnabled, VF_REQUIRE_APP_RESTART, "0 = IME disabled, 1 = IME enabled (if supported)");
}

// Destructor
CImeManager::~CImeManager()
{
	assert(gEnv && gEnv->pSystem);
	gEnv->pSystem->UnregisterWindowMessageHandler(this);
}

// Set Scaleform handler
bool CImeManager::SetScaleformHandler(IWindowMessageHandler* pHandler)
{
	// Unregister the handler
	if (pHandler == NULL)
	{
		m_pScaleformMessageHandler = NULL;
		return true;
	}

	// Note: For editor and dedicated server, we don't want to actually have an IME
	// For editor maybe we need this in the future, but for now just disable it
	if (gEnv->IsDedicated() || gEnv->IsEditor())
	{
		return false;
	}

	if (gEnv->pConsole)
	{
		ICVar* pVar = gEnv->pConsole->GetCVar(IME_CVAR_NAME);
		if (pVar && pVar->GetIVal() == 1)
		{
			m_pScaleformMessageHandler = pHandler;
			return true;
		}
	}

	// Not allowed by configuration
	return false;
}

#if CRY_PLATFORM_WINDOWS
// Preprocess message
void CImeManager::PreprocessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (m_pScaleformMessageHandler)
	{
		m_pScaleformMessageHandler->PreprocessMessage(hWnd, uMsg, wParam, lParam);
	}
}

// Handle message
bool CImeManager::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	if (m_pScaleformMessageHandler)
	{
		return m_pScaleformMessageHandler->HandleMessage(hWnd, uMsg, wParam, lParam, pResult);
	}
	else
	{
		switch (uMsg)
		{
		// In case Scaleform doesn't supply an override, just discard all IME messages
		// If we don't do this, the default IME behavior is used by the OS, which may involve helper windows,
		// which doesn't play nice a full-screen application
		case WM_IME_STARTCOMPOSITION:
		case WM_IME_KEYDOWN:
		case WM_IME_COMPOSITION:
		case WM_IME_ENDCOMPOSITION:
		case WM_IME_NOTIFY:
		case WM_IME_CHAR:
		case WM_IME_CONTROL:
		case WM_IME_COMPOSITIONFULL:
		case WM_IME_SELECT:
		case WM_IME_SETCONTEXT:
			*pResult = 0;
			return true;
		default:
			// All other messages just pass-through
			return false;
		}
	}
}

#endif
