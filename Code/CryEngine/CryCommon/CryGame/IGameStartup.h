// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	This is the interface which the Launcher.exe will interact
                with the game dll. For an implementation of this interface
                refer to the GameDll project of the title or MOD you are
                working	on.

   -------------------------------------------------------------------------
   History:
    - 23:7:2004   15:17 : Created by Marco Koegler
    - 30:7:2004   12:00 : Taken-over by Márcio Martins

*************************************************************************/

#pragma once

#include <CrySystem/ISystem.h>
#include <CryGame/IGame.h> // <> required for Interfuscator
#include "IGameRef.h"

struct ILog;
struct ILogCallback;
struct IValidator;
struct ISystemUserCallback;

//! Interfaces used to initialize a game using CRYENGINE.
struct IGameStartup
{
	//! Entry function used to create a new instance of the game.
	//! This is considered deprecated, in favor of ICryPlugin, and will be removed in the future
	//! \return A new instance of the game startup.
	typedef IGameStartup*(* TEntryFunction)();

	// <interfuscator:shuffle>
	virtual ~IGameStartup(){}

	//! Initialize the game and/or any MOD, and get the IGameMod interface.
	//! The shutdown method, must be called independent of this method's return value.
	//! \param startupParams Pointer to SSystemInitParams structure containing system initialization setup!
	//! \return Pointer to a IGameMod interface, or 0 if something went wrong with initialization.
	virtual IGameRef Init(SSystemInitParams& startupParams) = 0;

	//! Shuts down the game and any loaded MOD and delete itself.
	virtual void Shutdown() = 0;

	//! Returns the RSA Public Key used by the engine to decrypt pak files which are encrypted by an offline tool.
	//! Part of the tools package includes a key generator, which will generate a header file with the public key which can be easily included in code.
	//! This *should* ideally be in IGame being game specific info, but paks are loaded very early on during boot and this data is needed to init CryPak.
	virtual const uint8* GetRSAKey(uint32* pKeySize) const {* pKeySize = 0; return NULL; }
	// </interfuscator:shuffle>
};
