// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BloodSplats.h"

void CBloodSplats::Init(int type, const CTimeValue& maxTime)
{
	m_type = type;
	m_maxTime = maxTime;
}

bool CBloodSplats::Update(const CTimeValue& delta)
{
	m_currentTime -= delta;

	if (m_currentTime < 0)
		return true;

	nTime scale = (2 * m_currentTime) / m_maxTime;
	if (scale > 1) scale = 1;

	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Amount", BADF(scale * scale));

	return false;
}

bool CBloodSplats::OnActivate()
{
	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Type", (float)m_type);
	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Active", 1.0f);
	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Amount", 1.0f);
	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Spawn", 1.0f);
	m_currentTime = m_maxTime;
	return true;
}

bool CBloodSplats::OnDeactivate()
{
	gEnv->p3DEngine->SetPostEffectParam("BloodSplats_Active", 0.0f);
	return true;
}

void CBloodSplats::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
}
