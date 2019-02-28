// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryAISystem/IAuditionMap.h>

struct IEntityListenerComponent : public IEntityComponent
{
	typedef std::function<bool(const Perception::SSoundStimulusParams&)> UserConditionCallback;
	typedef std::function<void(const Perception::SSoundStimulusParams&)> SoundHeardCallback;
	
	virtual void SetUserConditionCallback(const UserConditionCallback& callback) = 0;
	virtual void SetSoundHeardCallback(const SoundHeardCallback& callback) = 0;

	static void ReflectType(Schematyc::CTypeDesc<IEntityListenerComponent>& desc)
	{
		desc.SetGUID("E7168B1B-E08F-43B0-BD10-0C48DE89FFB8"_cry_guid);
	}
};