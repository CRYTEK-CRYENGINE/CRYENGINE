// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "SubstitutionProxy.h"
#include "Entity.h"
#include <CryNetwork/ISerialize.h>

CRYREGISTER_CLASS(CEntityComponentSubstitution);

CEntityComponentSubstitution::CEntityComponentSubstitution()
{
	m_componentFlags.Add(EEntityComponentFlags::NoSave);
}

//////////////////////////////////////////////////////////////////////////
CEntityComponentSubstitution::~CEntityComponentSubstitution()
{
	if (m_pSubstitute)
	{
		m_pSubstitute->ReleaseNode();
		m_pSubstitute = nullptr;
	}
}

void CEntityComponentSubstitution::Done()
{
	// Substitution proxy does not need to be restored if entity system is being rested.
	if (m_pSubstitute)
	{
		//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Done: Ptr=%d", (int)m_pSubstitute);
		//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Done: %s", m_pSubstitute->GetName());
		//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Done: Pos=(%.2f,%.2f,%.2f)", m_pSubstitute->GetPos().x, m_pSubstitute->GetPos().y, m_pSubstitute->GetPos().z);
		gEnv->p3DEngine->RegisterEntity(m_pSubstitute);
		m_pSubstitute->Physicalize(true);
		AABB WSBBox = m_pSubstitute->GetBBox();
		static ICVar* e_on_demand_physics(gEnv->pConsole->GetCVar("e_OnDemandPhysics")),
		*e_on_demand_maxsize(gEnv->pConsole->GetCVar("e_OnDemandMaxSize"));
		if (m_pSubstitute->GetPhysics() && e_on_demand_physics && e_on_demand_physics->GetIVal() &&
		    e_on_demand_maxsize && max(WSBBox.max.x - WSBBox.min.x, WSBBox.max.y - WSBBox.min.y) <= e_on_demand_maxsize->GetFVal())
			gEnv->pPhysicalWorld->AddRefEntInPODGrid(m_pSubstitute->GetPhysics(), &WSBBox.min);
		m_pSubstitute = 0;
	}
}

void CEntityComponentSubstitution::SetSubstitute(IRenderNode* pSubstitute)
{
	m_pSubstitute = pSubstitute;
	//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::SetSubstitute: Ptr=%d", (int)m_pSubstitute);
	//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::SetSubstitute: %s", m_pSubstitute->GetName());
	//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::SetSubstitute: Pos=(%.2f,%.2f,%.2f)", m_pSubstitute->GetPos().x, m_pSubstitute->GetPos().y, m_pSubstitute->GetPos().z);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityComponentSubstitution::NeedGameSerialize()
{
	return m_pSubstitute != 0;
};

void CEntityComponentSubstitution::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case ENTITY_EVENT_DONE:
		Done();
	}
}

//////////////////////////////////////////////////////////////////////////
uint64 CEntityComponentSubstitution::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_DONE) | ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICAL_TYPE_CHANGED) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENABLE_PHYSICS);
}

//////////////////////////////////////////////////////////////////////////
void CEntityComponentSubstitution::GameSerialize(TSerialize ser)
{
	Vec3 center, pos;
	if (ser.IsReading())
	{
		if (!m_pSubstitute)
		{
			ser.Value("SubstCenter", center);
			ser.Value("SubstPos", pos);
			IPhysicalEntity** pents;
			m_pSubstitute = 0;
			int i = gEnv->pPhysicalWorld->GetEntitiesInBox(center - Vec3(0.05f), center + Vec3(0.05f), pents, ent_static);
			for (--i; i >= 0 && !((m_pSubstitute = static_cast<IRenderNode*>(pents[i]->GetForeignData(PHYS_FOREIGN_ID_STATIC))) &&
			                      (m_pSubstitute->GetPos() - pos).len2() < sqr(0.03f) &&
			                      (m_pSubstitute->GetBBox().GetCenter() - center).len2() < sqr(0.03f)); i--)
				;
			if (i < 0)
				m_pSubstitute = 0;
			else if (m_pSubstitute)
			{
				//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Serialize: Ptr=%d", (int)m_pSubstitute);
				//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Serialize: %s", m_pSubstitute->GetName());
				//gEnv->pLog->Log("CRYSIS-3502: CSubstitutionProxy::Serialize: Pos=(%.2f,%.2f,%.2f)", m_pSubstitute->GetPos().x, m_pSubstitute->GetPos().y, m_pSubstitute->GetPos().z);

				m_pSubstitute->Dephysicalize();
				gEnv->p3DEngine->UnRegisterEntityAsJob(m_pSubstitute);
			}
		}
	}
	else
	{
		if (m_pSubstitute)
		{
			ser.Value("SubstCenter", center = m_pSubstitute->GetBBox().GetCenter());
			ser.Value("SubstPos", pos = m_pSubstitute->GetPos());
		}
	}
}
