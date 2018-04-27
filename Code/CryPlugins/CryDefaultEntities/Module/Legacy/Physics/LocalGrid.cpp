#include "StdAfx.h"
#include "LocalGrid.h"

#include <CryPhysics/IPhysics.h>
#include <CrySerialization/Enum.h>

class CLocalGridEntityRegistrator	: public IEntityRegistrator
{
	virtual void Register() override { RegisterEntityWithDefaultComponent<CLocalGridEntity>("LocalGrid", "Physics", "physicsobject.bmp", true); }
} 
g_gridEntityRegistrator;
CRYREGISTER_CLASS(CLocalGridEntity);

YASLI_ENUM_BEGIN_NESTED(CLocalGridEntity, EGridDim, "GridDimension")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_1,    "1")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_2,    "2")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_4,    "4")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_8,    "8")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_16,   "16")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_32,   "32")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_64,   "64")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_128,  "128")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_256,  "256")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_512,  "512")
YASLI_ENUM_VALUE_NESTED(CLocalGridEntity, eDim_1024, "1024")
YASLI_ENUM_END()


void CLocalGridEntity::OnResetState()
{
	if (m_pGrid)
	{
		GetEntity()->DetachAll(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		if (IPhysicalEntity *pHost = GetEntity()->GetParent()->GetPhysics())
		{
			pe_params_outer_entity poe;
			poe.pOuterEntity = nullptr;
			pHost->SetParams(&poe, 1);
		}
		m_pGrid->AddRef(); // since Physicalize will both Release() and Destroy()
		m_pGrid = nullptr;
		SEntityPhysicalizeParams pp;
		GetEntity()->Physicalize(pp);
	}
	m_shouldApply = true;
}

void CLocalGridEntity::ProcessEvent(const SEntityEvent& event)
{
	CDesignerEntityComponent::ProcessEvent(event);

	switch (event.event)
	{
		case ENTITY_EVENT_ATTACH:
		{
			IEntity *pChild = gEnv->pEntitySystem->GetEntity((EntityId)event.nParam[0]);
			if (pChild->GetPhysics())
			{
				pe_params_pos pp;
				pp.pGridRefEnt = m_pGrid;
				pChild->GetPhysics()->SetParams(&pp);
			}
			break;
		}

		case ENTITY_EVENT_DETACH:
		{
			IEntity *pChild = gEnv->pEntitySystem->GetEntity((EntityId)event.nParam[0]);
			if (pChild->GetPhysics())
			{
				IEntity *pHost = GetEntity()->GetParent();
				for(; pHost && !(pHost->GetParent() && pHost->GetLocalSimParent()); pHost = pHost->GetParent())
					;
				if (!pHost)
				{
					pe_params_pos pp;
					pp.pGridRefEnt = WORLD_ENTITY;
					pChild->GetPhysics()->SetParams(&pp);
				} 
				else
				{
					pHost->GetParent()->AttachChild(pChild, SChildAttachParams(IEntity::EAttachmentFlags::ATTACHMENT_KEEP_TRANSFORMATION | IEntity::EAttachmentFlags::ATTACHMENT_LOCAL_SIM));
				}
			}
			break;
		}

		case ENTITY_EVENT_START_GAME:
		{
			if (!m_shouldApply || !GetEntity()->GetParent() || !GetEntity()->GetParent()->GetPhysics())
				break;

			pe_params_pos pp;
			pp.pos = GetEntity()->GetPos(); pp.q = GetEntity()->GetRotation();
			m_pGrid = gEnv->pPhysicalWorld->SetupEntityGrid(2, Vec3(0), m_size.x,m_size.y, m_cellSize.x,m_cellSize.y, 0,0, 
				GetEntity()->GetParent()->GetPhysics(), QuatT(pp.q,pp.pos));
			pe_params_foreign_data pfd;
			pfd.pForeignData = GetEntity();
			pfd.iForeignData = PHYS_FOREIGN_ID_ENTITY;
			m_pGrid->SetParams(&pfd);
			GetEntity()->AssignPhysicalEntity(m_pGrid);
			m_pGrid->SetParams(&pp);
			pe_simulation_params sp;
			sp.minEnergy = sqr(m_accThresh);
			m_pGrid->SetParams(&sp);

			for(IEntityLink *pLink = GetEntity()->GetEntityLinks(); pLink; pLink = pLink->next)
			{
				IEntity *pPortal = gEnv->pEntitySystem->GetEntity(pLink->entityId);
				if (pPortal && pPortal->GetPhysics())
				{
					IPhysicalEntity *pPhysEnt = pPortal->GetPhysics();
					pe_params_pos ppos;
					ppos.iSimClass = SC_TRIGGER;
					pPhysEnt->SetParams(&ppos);
					pe_status_pos sp;
					pPhysEnt->GetStatus(&sp);
					ppos.pos = sp.pos; ppos.q = sp.q; 
					ppos.pGridRefEnt = GetEntity()->GetParent()->GetPhysics();
					pe_action_add_constraint aac;
					aac.pBuddy = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_STATIC, &ppos);
					pe_params_part ppart;
					for(ppart.ipart = 0; pPhysEnt->GetParams(&ppart); ppart.ipart++)
					{
						pe_geomparams gp;
						gp.pos = ppart.pos; gp.q = ppart.q;	gp.scale = ppart.scale;
						gp.flags = ppart.flagsOR; gp.flagsCollider = ppart.flagsColliderOR;
						aac.pBuddy->AddGeometry(ppart.pPhysGeom, &gp, ppart.partid);
					}
					if (IEntityLink *pLink = pPortal->GetEntityLinks())
						if (IEntity *pLinkEnt = gEnv->pEntitySystem->GetEntity(pLink->entityId))
							if (pLinkEnt->GetPhysics())
							{
								pe_params_outer_entity poe;
								poe.pOuterEntity = pLinkEnt->GetPhysics();
								pPhysEnt->SetParams(&poe);
								aac.pBuddy->SetParams(&poe);
								pe_params_flags pf; pf.flagsAND = ~pef_traceable;
								poe.pOuterEntity->SetParams(&pf);
							}
					pe_params_pos pgrid; pgrid.pGridRefEnt = m_pGrid;
					pPhysEnt->SetParams(&pgrid);
					pPhysEnt->Action(&aac);
				}
			}

			Vec3 sz = Vec3(m_size.x*m_cellSize.x, m_size.y*m_cellSize.y, m_height)*0.5f, center = GetEntity()->GetWorldPos()+GetEntity()->GetWorldRotation()*sz;
			Quat qWorld = GetEntity()->GetWorldRotation();
			SEntityProximityQuery epq;
			epq.box.SetTransformedAABB(GetEntity()->GetWorldTM(), AABB(Vec3(0), sz*2));
			gEnv->pEntitySystem->QueryProximity(epq);
			for(int i=0; i<epq.nCount; i++)
			{
				AABB bbox;
				epq.pEntities[i]->GetWorldBounds(bbox);
				if (epq.pEntities[i] == GetEntity() || epq.pEntities[i] == GetEntity()->GetParent() || max(Vec3(0), ((bbox.GetCenter()-center)*qWorld).abs()-sz).len2() > 1e-6f)
					continue;
				GetEntity()->AttachChild(epq.pEntities[i], SChildAttachParams(IEntity::ATTACHMENT_LOCAL_SIM | IEntity::ATTACHMENT_KEEP_TRANSFORMATION));
			}
			m_shouldApply = false;
			break;
		}
	}
}
