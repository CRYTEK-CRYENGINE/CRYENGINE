// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   History:
   - 17:01:2006:		Created by Marco Koegler

*************************************************************************/
#include <StdAfx.h>
#include "EffectSystem.h"
#include "BloodSplats.h"
#include "GroundEffect.h"

CEffectSystem::CEffectSystem()
{
}

CEffectSystem::~CEffectSystem()
{
}

bool CEffectSystem::Init()
{
	CBloodSplats* pBloodSplats = new CBloodSplats;
	pBloodSplats->Init(0, 2.5f);
	BindEffect("BloodSplats_Human", pBloodSplats);

	pBloodSplats = new CBloodSplats;
	pBloodSplats->Init(1, 2.5f);
	BindEffect("BloodSplats_Alien", pBloodSplats);

	return true;
}

void CEffectSystem::Update(float delta)
{
	for (TEffectVec::iterator i = m_effects.begin(); i != m_effects.end(); )
	{
		if ((*i)->GetState() == eES_Activating && (*i)->Activating(delta))
		{
			(*i)->SetState(eES_Updating);
		}
		if ((*i)->GetState() == eES_Updating && (*i)->Update(delta))
		{
			(*i)->SetState(eES_Deactivating);
		}
		if ((*i)->GetState() == eES_Deactivating && (*i)->Deactivating(delta))
		{
			(*i)->OnDeactivate();
			(*i)->SetState(eES_Deactivated);
		}

		++i;
	}
}

void CEffectSystem::Shutdown()
{
}

EffectId CEffectSystem::GetEffectId(const char* name)
{
	TNameToId::iterator i = m_nameToId.find(CONST_TEMP_STRING(name));

	if (i == m_nameToId.end())
		return -1;

	return (*i).second;
}

void CEffectSystem::Activate(const EffectId& eid)
{
	if (eid < 0 || eid >= m_effects.size())
		return;

	if (m_effects[eid]->OnActivate())
	{
		m_effects[eid]->SetState(eES_Activating);
	}
}

bool CEffectSystem::BindEffect(const char* name, IEffect* pEffect)
{
	if (GetEffectId(name) != -1)
		return false;

	m_nameToId[name] = m_effects.size();
	m_effects.push_back(pEffect);
	pEffect->SetState(eES_Deactivated);

	return true;
}

IGroundEffect* CEffectSystem::CreateGroundEffect(IEntity* pEntity)
{
	return new CGroundEffect(pEntity);
}

void CEffectSystem::RegisterFactory(const char* name, IEffect*(*func)(), bool isAI)
{
	m_effectClasses[name] = func;
}

void CEffectSystem::GetMemoryStatistics(ICrySizer* s)
{
	SIZER_SUBCOMPONENT_NAME(s, "EffectSystem");
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_nameToId);
	s->AddObject(m_effects);

	for (TEffectClassMap::iterator iter = m_effectClasses.begin(); iter != m_effectClasses.end(); ++iter)
		s->AddObject(iter->first);
}
