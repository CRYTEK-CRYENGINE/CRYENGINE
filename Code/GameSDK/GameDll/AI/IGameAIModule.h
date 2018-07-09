// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef IGameAIModule_h
#define IGameAIModule_h

// For an overview of the GameAISystem take a look in GameAISystem.cpp

struct IGameAIModule
{
	virtual ~IGameAIModule() {}
	virtual void EntityEnter(EntityId entityID) = 0;
	virtual void EntityLeave(EntityId entityID) = 0;
	virtual void EntityPause(EntityId entityID) = 0;
	virtual void EntityResume(EntityId entityID) = 0;
	virtual void Reset(bool bUnload) {}
	virtual void Update(float dt) {}
	virtual void Serialize(TSerialize ser) {}
	virtual void PostSerialize() {}
	virtual const char* GetName() const = 0;
};

#endif // IGameAIModule_h