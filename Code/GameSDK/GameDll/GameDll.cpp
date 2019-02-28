// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Game DLL entry point.

   -------------------------------------------------------------------------
   History:
   - 2:8:2004   10:38 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "Game.h"
#include "GameStartup.h"
#include "EditorGame.h"

#include <CryCore/Platform/CryLibrary.h>

extern "C"
{
	GAME_API IGame* CreateGame(IGameFramework* pGameFramework)
	{
		ModuleInitISystem(pGameFramework->GetISystem(), "CryGame");

		static char pGameBuffer[sizeof(CGame)];
		return new((void*)pGameBuffer)CGame();
	}

	GAME_API IGameStartup* CreateGameStartup()
	{
		return CGameStartup::Create();
	}
	GAME_API IEditorGame* CreateEditorGame()
	{
		return new CEditorGame();
	}
}
