// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameStartup.h"
#include <CryCore/Platform/platform_impl.inl>
#include <CryCore/Platform/CryLibrary.h>
#include <CryInput/IHardwareMouse.h>
#include <ILevelSystem.h>
#include "game/Game.h"

#if defined(_LIB)
extern "C" IGameStartup* CreateGameStartup();
#endif

CGameStartup* CGameStartup::Create()
{
	static char buff[sizeof(CGameStartup)];
	return new(buff) CGameStartup();
}

CGameStartup::CGameStartup()
	: m_pGame(nullptr),
	m_gameRef(&m_pGame)
{
}

CGameStartup::~CGameStartup()
{
	if (m_pGame)
	{
		m_pGame->Shutdown();
		m_pGame = nullptr;
	}
}

IGameRef CGameStartup::Init(SSystemInitParams& startupParams)
{
#ifndef _LIB
	gEnv = startupParams.pSystem->GetGlobalEnvironment();
#endif // !_LIB

	CryRegisterFlowNodes();

	static char pGameBuffer[sizeof(CGame)];
	m_pGame = new((void*)pGameBuffer)CGame();

	if (m_pGame->Init())
	{
		return m_gameRef;
	}

	return IGameRef(&m_pGame);
}

void CGameStartup::Shutdown()
{
	CryUnregisterFlowNodes();
	this->~CGameStartup();
}