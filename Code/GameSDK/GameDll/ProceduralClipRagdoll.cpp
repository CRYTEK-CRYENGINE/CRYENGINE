// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: ProceduralClip for Ragdolling from CryMannequin

-------------------------------------------------------------------------
History:
- 11.18.11: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryExtension/ClassWeaver.h>

#include "Player.h"

#include "ProceduralContextRagdoll.h"
#include <CrySerialization/IArchive.h>

struct SRagdollParams : public IProceduralParams
{
	SRagdollParams()
		: sleep(0.0f)
		, stiffness(500.0f)
	{
	}

	virtual void Serialize(Serialization::IArchive& ar)
	{
		ar(sleep, "Sleep", "Sleep");
		ar(stiffness, "Stiffness", "Stiffness");
	}
	float sleep;
	float stiffness;
};

class CProceduralClipRagdoll : public TProceduralContextualClip<CProceduralContextRagdoll, SRagdollParams>
{
public:
	CProceduralClipRagdoll()
		: m_stiffness(0.0f)
		, m_randomTriggerTime(0.0f)
		, m_totalTimePassed(0.0f)
		, m_ragdollSleep(false)		
	{
	}

protected:
	virtual void OnEnter(float blendTime, float duration, const SRagdollParams &params)
	{
		m_randomTriggerTime = cry_random(0.0f, blendTime);
		m_ragdollSleep = bool(params.sleep>0.0f);
		m_stiffness = params.stiffness;
	}
	virtual void Update(float timePassed)
	{
		m_totalTimePassed += timePassed;
		if( !m_context->IsInRagdoll() && m_totalTimePassed > m_randomTriggerTime ) 
		{
			m_context->EnableRagdoll( m_entity->GetId(), m_ragdollSleep, m_stiffness, true );
		}
	}
	virtual void OnExit(float blendTime)
	{
		m_totalTimePassed = 0.0f;
		m_context->DisableRagdoll( blendTime );
	}

private:
	float m_stiffness;
	float m_randomTriggerTime;
	float m_totalTimePassed;
	bool  m_ragdollSleep;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipRagdoll, "Ragdoll");
