// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryGame/IGameFramework.h>
#include <CryGame/IGameStartup.h>
#include <CrySystem/IWindowMessageHandler.h>

#if defined(CVARS_WHITELIST)
class CCVarsWhiteList : public ICVarsWhitelist
{
public:
	// ICVarsWhiteList
	bool IsWhiteListed(const string& command, bool silent);
	// ~ICVarsWhiteList
};
#endif // defined(CVARS_WHITELIST)

class GameStartupErrorObserver : public IErrorObserver
{
public:
	void OnAssert(const char* condition, const char* message, const char* fileName, unsigned int fileLineNumber);
	void OnFatalError(const char* message);
};

class CGameStartup :
	public IGameStartup, public ISystemEventListener, public IWindowMessageHandler
{
public:
	virtual IGameRef     Init(SSystemInitParams& startupParams);

	virtual void         Shutdown();
	virtual const char*  GetPatch() const                                                  { return nullptr; }
	virtual bool         GetRestartMod(char* pModNameBuffer, int modNameBufferSizeInBytes) { return false; }
	virtual const uint8* GetRSAKey(uint32* pKeySize) const;

	IGameRef             Reset(ISystem* pSystem);

	virtual void         OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);

	static CGameStartup* Create();

protected:
	CGameStartup();
	virtual ~CGameStartup() {}

private:
	void        HandleResizeForVOIP(WPARAM wparam);
	static void FullScreenCVarChanged(ICVar* pVar);

#if CRY_PLATFORM_WINDOWS
	bool HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* pResult);
#endif

	IGame*                   m_pGame;
	IGameRef                 m_GameRef;

	bool                     m_quit;
	bool                     m_fullScreenCVarSetup;
	int8                     m_nVOIPWasActive;

	IGameFramework*          m_pFramework;
	GameStartupErrorObserver m_errorObsever;
};
