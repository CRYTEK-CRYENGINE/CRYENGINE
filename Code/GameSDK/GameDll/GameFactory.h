// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
  -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Register the factory templates used to create classes from names
               e.g. REGISTER_FACTORY(pFramework, "Player", CPlayer, false);
               or   REGISTER_FACTORY(pFramework, "Player", CPlayerG4, false);

               Since overriding this function creates template based linker errors,
               it's been replaced by a standalone function in its own cpp file.

  -------------------------------------------------------------------------
  History:
  - 17:8:2005   Created by Nick Hesketh - Refactor'd from Game.cpp/h

*************************************************************************/

#ifndef __GAMEFACTORY_H__
#define __GAMEFACTORY_H__

#if _MSC_VER > 1000
# pragma once
#endif


struct IGameFramework;

// Register the factory templates used to create classes from names. Called via CGame::Init()
void InitGameFactory(IGameFramework *pFramework);


#endif // ifndef __GAMEFACTORY_H__
