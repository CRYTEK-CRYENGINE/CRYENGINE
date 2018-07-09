// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ICryMannequin.h"
#include <CryExtension/ClassWeaver.h>
#include <Mannequin/Serialization.h>
#include <CryAISystem/IAgent.h>
#include "Player.h"

struct SSetStanceParams : public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(stance, "Stance", "Stance");
	}

	TProcClipString stance;
};

class CProceduralClipSetStance : public TProceduralClip<SSetStanceParams>
{
protected:
	virtual void OnEnter( float blendTime, float duration, const SSetStanceParams& params )
	{
		const EntityId entityId = m_scope->GetEntityId();
		IEntity* entity = gEnv->pEntitySystem->GetEntity( entityId );
		IF_UNLIKELY( !entity )
			return;

		IAIObject* aiObject = entity->GetAI();
		IF_UNLIKELY( !aiObject )
			return;

		CPlayer* player = static_cast<CPlayer*>( g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( entityId ) );
		IF_UNLIKELY( !player )
			return;

		CAIAnimationComponent* aiAnimationComponent = player->GetAIAnimationComponent();
		IF_UNLIKELY( !aiAnimationComponent )
			return;

		const char *stanceName = params.stance.c_str();
		IF_UNLIKELY( !stanceName )
			return;

		int stance = STANCE_NULL;
		for(; stance < STANCE_LAST; ++stance)
		{
			if ( strcmpi( stanceName, GetStanceName( (EStance)stance) ) == 0 )
				break;
		}

		IF_UNLIKELY( (EStance)stance == STANCE_LAST )
			return;

		aiAnimationComponent->ForceStanceTo( *player, (EStance)stance );
		aiAnimationComponent->ForceStanceInAIActorTo( *player, (EStance)stance );
	}

	virtual void Update( float timePassed )
	{
	}

	virtual void OnExit( float blendTime )
	{
	}
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipSetStance, "SetStance");
