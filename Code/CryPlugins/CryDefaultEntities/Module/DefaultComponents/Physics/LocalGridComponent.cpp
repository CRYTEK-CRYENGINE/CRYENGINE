#include "StdAfx.h"

#include "LocalGridComponent.h"
#include <CryPhysics/IPhysics.h>
#include <CryRenderer/IRenderAuxGeom.h>

namespace Cry
{
namespace DefaultComponents
{

CLocalGridComponent::~CLocalGridComponent()
{
}

void CLocalGridComponent::Reset()
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
		m_pGrid->AddRef(); // since entity's Physicalize will both Release() and Destroy()
		m_pGrid = nullptr;
		SEntityPhysicalizeParams pp;
		GetEntity()->Physicalize(pp);
		for(IEntityLink *pLink = GetEntity()->GetEntityLinks(); pLink; pLink = pLink->next)
		{
			IEntity *pPortal = gEnv->pEntitySystem->GetEntity(pLink->entityId);
			if (pPortal && pPortal->GetPhysics())
			{
				pe_status_constraint sc;
				if (pPortal->GetPhysics()->GetStatus(&sc) && sc.pBuddyEntity)
					gEnv->pPhysicalWorld->DestroyPhysicalEntity(sc.pBuddyEntity);
				pe_action_update_constraint auc; auc.bRemove = 1;
				pPortal->GetPhysics()->Action(&auc);
			}
		}
		for(IPhysicalEntity *area : m_physAreas)
		{
			pe_params_pos pp;
			pp.pGridRefEnt = nullptr;
			area->SetParams(&pp);
		}
		m_physAreas.clear();
	}
}

void CLocalGridComponent::Physicalize()
{
	if (!GetEntity()->GetParent() || !GetEntity()->GetParent()->GetPhysics())
		return;

	Reset();

	pe_params_pos pp;
	pp.pos = GetEntity()->GetPos(); pp.q = GetEntity()->GetRotation();
	m_pGrid = gEnv->pPhysicalWorld->SetupEntityGrid(2, Vec3(0), m_sizex,m_sizey, m_cellSize.x,m_cellSize.y, 0,0, 
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
	pe_params_bbox pbb;
	pbb.BBox[0].zero();
	pbb.BBox[1].Set(m_sizex*m_cellSize.x, m_sizey*m_cellSize.y, m_height);
	m_pGrid->SetParams(&pbb);

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
			pe_params_part ppart, ppartFlags;
			ppartFlags.flagsAND = geom_colltype_ray;
			pPhysEnt->SetParams(&ppartFlags);
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
						poe.pOuterEntity->SetParams(&ppartFlags);
					}
			pe_params_pos pgrid; pgrid.pGridRefEnt = m_pGrid;
			pPhysEnt->SetParams(&pgrid);
			pPhysEnt->Action(&aac);
		}
	}

	Vec3 sz = Vec3(m_sizex*m_cellSize.x, m_sizey*m_cellSize.y, m_height)*0.5f, center = GetEntity()->GetWorldPos()+GetEntity()->GetWorldRotation()*sz;
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
	IPhysicalEntity **physAreas;
	int nAreas = gEnv->pPhysicalWorld->GetEntitiesInBox(epq.box.min,epq.box.max, physAreas, ent_areas);
	for(int i=0; i<nAreas; i++)
	{
		physAreas[i]->GetParams(&pbb);
		if ((pbb.BBox[1]-pbb.BBox[0]).GetVolume() > epq.box.GetVolume()*10)
			continue;
		pe_params_pos pp;
		pp.pGridRefEnt = m_pGrid;
		physAreas[i]->SetParams(&pp);
		m_physAreas.push_back(physAreas[i]);
	}
}

void CLocalGridComponent::ProcessEvent(const SEntityEvent& ev)
{
	switch (ev.event)
	{
		case ENTITY_EVENT_ATTACH:
		{
			IEntity *pChild = gEnv->pEntitySystem->GetEntity((EntityId)ev.nParam[0]);
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
			IEntity *pChild = gEnv->pEntitySystem->GetEntity((EntityId)ev.nParam[0]);
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

		case ENTITY_EVENT_RESET:
			Reset();
			break;

		case ENTITY_EVENT_START_GAME:
			Physicalize();
			break;
	}
}

#ifndef RELEASE
void CLocalGridComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
{
	if (context.bSelected)
	{
		auto pRnd = gEnv->pRenderer->GetIRenderAuxGeom();
		Matrix34 mtx = GetWorldTransformMatrix();
		Vec2_tpl<EPowerOf2> size(m_sizex, m_sizey);
		for(int i=0; i<2; i++)
		{
			Vec3 pt[2] = { Vec3(ZERO), Vec3(ZERO) }, h(0,0,m_height);
			pt[1][i^1] = m_cellSize[i^1] * size[i^1];
			for(int j=0,flip=i; j<=(int)size[i]; j++,flip=i^1)
			{
				pt[0][i] = pt[1][i] = m_cellSize[i]*j;
				pRnd->DrawLine(mtx*pt[0], context.debugDrawInfo.lineColor, mtx*pt[1], context.debugDrawInfo.lineColor);
				if (!inrange(j, 0, (int)size[i]))
					pRnd->DrawQuad(mtx*pt[flip], context.debugDrawInfo.color, mtx*pt[flip^1], context.debugDrawInfo.color, mtx * (pt[flip^1]+h), context.debugDrawInfo.color, mtx*(pt[flip]+h), context.debugDrawInfo.color);
			}
		}
		GetEntity()->SetLocalBounds(AABB(Vec3(ZERO), Vec3(m_cellSize.x*m_sizex, m_cellSize.y*m_sizey, m_height)), true);
	}
}
#endif

}
}