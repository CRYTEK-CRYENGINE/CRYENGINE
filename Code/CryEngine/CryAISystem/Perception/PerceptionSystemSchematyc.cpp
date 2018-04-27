// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PerceptionSystemSchematyc.h"
#include "Factions/FactionSystemSchematyc.h"

#include "Components/Perception/ObserverComponent.h"
#include "Components/Perception/ObservableComponent.h"
#include "Components/Perception/ListenerComponent.h"

namespace PerceptionSystemSchematyc
{

void SoundStimulEvent(const Vec3& position, float radius, Schematyc::ExplicitEntityId sourceEntityId, SFactionID factionId, 
	Perception::EStimulusObstructionHandling obstructionHandling, Schematyc::CSharedString debugName)
{
	IAuditionMap* pAuditionMap = gEnv->pAISystem->GetAuditionMap();
	if (!pAuditionMap)
		return;

	Perception::SSoundStimulusParams params;
	params.position = position;
	params.radius = radius;
	params.sourceEntityId = static_cast<EntityId>(sourceEntityId);
	params.faction = factionId.id;
	params.obstructionHandling = obstructionHandling;

	pAuditionMap->OnSoundEvent(params, debugName.c_str());
}

void Register(Schematyc::IEnvRegistrar& registrar, Schematyc::CEnvRegistrationScope& parentScope)
{
	//Register Components
	CEntityAIObserverComponent::Register(registrar);
	CEntityAIObservableComponent::Register(registrar);
	CEntityAIListenerComponent::Register(registrar);
	
	const CryGUID PerceptionSystemGUID = "301D7755-A3C2-4BA9-A7CE-483ED826FC67"_cry_guid;

	parentScope.Register(SCHEMATYC_MAKE_ENV_MODULE(PerceptionSystemGUID, "Perception"));
	Schematyc::CEnvRegistrationScope perceptionScope = registrar.Scope(PerceptionSystemGUID);

	//SoundStimulEvent
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SoundStimulEvent, "65B98164-F180-4BC9-81B6-B4FD4610AB1B"_cry_guid, "SoundStimulEvent");
		pFunction->SetDescription("Create sound stimulus event");
		pFunction->BindInput(1, 'pos', "Position");
		pFunction->BindInput(2, 'rad', "Radius");
		pFunction->BindInput(3, 'seid', "Source Entity Id");
		pFunction->BindInput(4, 'fac', "Faction");
		pFunction->BindInput(5, 'ohnd', "Obstruction Handling");
		pFunction->BindInput(6, 'dstr', "Debug String");
		perceptionScope.Register(pFunction);
	}

}
} //! PerceptionSystemSchematyc
