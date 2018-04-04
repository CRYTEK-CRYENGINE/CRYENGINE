// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "GameFactory.h"

#include <CryFlowGraph/IFlowBaseNode.h>

CGame::CGame()
{
}

CGame::~CGame()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);

	if (gEnv->pGameFramework->StartedGameContext())
	{
		gEnv->pGameFramework->EndGameContext();
	}
}

bool CGame::Init()
{
	CGameFactory::Init();
	gEnv->pGameFramework->SetGameGUID(GAME_GUID);

	// Request loading of the example map next frame
	gEnv->pConsole->ExecuteString("map example", false, true);
	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CGame");

	return true;
}

void CGame::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
}

int CGame::Update(bool haveFocus, unsigned int updateFlags)
{
	return 1;
}

void CGame::Shutdown()
{
	this->~CGame();
}

void CGame::GetMemoryStatistics(ICrySizer* s)
{
	s->Add(*this);
}

const char* CGame::GetLongName()
{
	return GAME_LONGNAME;
}

const char* CGame::GetName()
{
	return GAME_NAME;
}
