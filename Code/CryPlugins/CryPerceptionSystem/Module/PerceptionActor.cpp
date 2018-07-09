// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PerceptionActor.h"

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>

static const uint32 kOnCloseContactCRC = CCrc32::Compute("OnCloseContact");

static const float kCloseContactTimeOutValue = 1.5f;

CPerceptionActor::CPerceptionActor() :
	CPerceptionActor(nullptr)
{
}

//-----------------------------------------------------------------------------------------------------------
CPerceptionActor::CPerceptionActor(IAIActor* pAIActor) :
	m_pAIActor(pAIActor),
	m_closeContactTimeOut(0.0f),
	m_meleeRange(2.0f),
	m_perceptionDisabled(0)
{}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionActor::Update(float frameDelta)
{
	if (!IsEnabled())
		return;
	
	if (m_closeContactTimeOut > 0.0f)
	{
		m_closeContactTimeOut = fmax(0.0f, m_closeContactTimeOut - frameDelta);
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionActor::Reset()
{
	m_closeContactTimeOut = 0.0f;
	m_perceptionDisabled = 0;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionActor::Enable(bool enable)
{
	if (enable)
		--m_perceptionDisabled;
	else
		++m_perceptionDisabled;

	CRY_ASSERT(m_perceptionDisabled >= 0); // Below zero? More disables then enables!
	CRY_ASSERT(m_perceptionDisabled < 16); // Just a little sanity check
}

//-----------------------------------------------------------------------------------------------------------
bool CPerceptionActor::IsEnabled() const
{
	return m_perceptionDisabled <= 0;
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionActor::CheckCloseContact(IAIObject* pTarget)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (CloseContactEnabled())
	{
		if (m_pAIActor->CastToIAIObject()->CastToIPuppet())
		{
			if (m_pAIActor->GetAttentionTarget() != pTarget)
				return;
		}
		
		const float distSq = Distance::Point_PointSq(m_pAIActor->CastToIAIObject()->GetPos(), pTarget->GetPos());
		if (distSq < sqr(m_meleeRange))
		{
			m_pAIActor->SetSignal(1, "OnCloseContact", pTarget->GetEntity(), 0, kOnCloseContactCRC);
			
			m_closeContactTimeOut = kCloseContactTimeOutValue;
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
void CPerceptionActor::Serialize(TSerialize ser)
{
	ser.Value("m_closeContactTimeOut", m_closeContactTimeOut);
}