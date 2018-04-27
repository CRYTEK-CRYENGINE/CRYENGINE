// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IGameStateRecorder.h
//  Version:     v1.00
//  Created:     3/2008 by Luciano Morpurgo.
//  Compilers:   Visual Studio.NET
//  Description: (Interface) Checks the player and other game specific objects' states and communicate them to the TestManager
//
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __IGAMESTATERECORDER_H__
#define __IGAMESTATERECORDER_H__

#pragma once

typedef enum
{
	GPM_Disabled,
	GPM_SingleActor,
	GPM_AllActors
} EGameProfileMode;

struct IGameplayListener;
struct IEntity;
struct GameplayEvent;

struct IGameStateRecorder
{
	virtual ~IGameStateRecorder(){}
	virtual void  Enable(bool bEnable, bool bRecording) = 0;

	virtual void  OnRecordedGameplayEvent(IEntity* pEntity, const GameplayEvent& event, int currentFrame = 0, bool bRecording = false) = 0;
	virtual void  RegisterListener(IGameplayListener* pL) = 0;
	virtual void  UnRegisterListener(IGameplayListener* pL) = 0;
	virtual float RenderInfo(float y, bool bRecording) = 0;
	virtual void  GetMemoryStatistics(ICrySizer* s) = 0;
	virtual void  Release() = 0;
	virtual bool  IsEnabled() = 0;
	virtual void  Update() = 0;
};

#endif
