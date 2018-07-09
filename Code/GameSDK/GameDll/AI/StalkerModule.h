// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef StalkerModule_h
#define StalkerModule_h

#include "GameAIHelpers.h"
#include <CryPhysics/RayCastQueue.h>

struct StalkerInstance : public CGameAIInstanceBase
{
public:
	StalkerInstance()
		: rayID(0)
		, asyncState(AsyncReady)
		, lastVisibleFromTarget(false)
		, lastInTargetFov(false)
	{
	}

	QueuedRayID rayID;
	AsyncState asyncState;
	bool lastVisibleFromTarget;
	bool lastInTargetFov;
};

class StalkerModule : public AIModuleWithInstanceUpdate<StalkerModule, StalkerInstance, 4, 4>
{
private:
	virtual void InitializeInstance(const InstanceInitContext<StalkerInstance>& context);
	virtual void DeinitializeInstance(InstanceID instanceID);
	virtual const char* GetName() const { return "StalkerModule"; }
	virtual void UpdateInstance(StalkerInstance& instance, float frameTime);

	void QueueLineOfSightRay(class Agent& stalker, IAIObject* target, StalkerInstance& instance);
	void LineOfSightRayComplete(const QueuedRayID& rayID, const RayCastResult& result);
	StalkerInstance* FindInstanceForRay(const QueuedRayID& rayID);	
};

#endif // StalkerModule_h