// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PickAndThrowProxy.h"
#include "Player.h"
#include "GameCVars.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CPickAndThrowProxy::SPnTProxyParams)

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPickAndThrowProxy::SPnTProxyParams::SPnTProxyParams() : fRadius(0.5f), fHeight(0.5f), proxyShape(ePS_Capsule), vPosPivot(ZERO)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPickAndThrowProxyPtr CPickAndThrowProxy::Create(CPlayer* pPlayer, const IItemParamsNode* pParams)
{
	CRY_ASSERT(pPlayer && pParams);

	CPickAndThrowProxyPtr pNewInstance;
	if (pPlayer && pParams && !pPlayer->IsPlayer() && !pPlayer->IsPoolEntity() && g_pGameCVars->pl_pickAndThrow.useProxies)
	{
		const IItemParamsNode* pRootParams = pParams->GetChild("PickAndThrowProxyParams");
		if (pRootParams)
		{
			pNewInstance.reset(new CPickAndThrowProxy(*pPlayer));
			pNewInstance->ReadXmlData(pRootParams);
		}
	}

	return pNewInstance;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPickAndThrowProxy::CPickAndThrowProxy(CPlayer& player) : m_player(player), m_pPickNThrowProxy(NULL), m_pLastPlayerPhysics(NULL), m_bNeedsReloading(false)
{
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CPickAndThrowProxy::~CPickAndThrowProxy()
{
	Unphysicalize();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CPickAndThrowProxy::Physicalize()
{
	IEntity* pPlayerEntity = m_player.GetEntity();
	IPhysicalEntity* pPlayerPhysics = pPlayerEntity->GetPhysics();

	const bool bPhysicsChanged = (pPlayerPhysics != m_pLastPlayerPhysics);

	// Don't create it again if already exists and its associated physic entity is the same
	if ((m_pPickNThrowProxy && !bPhysicsChanged) || !g_pGameCVars->pl_pickAndThrow.useProxies)
		return;

	CRY_ASSERT(m_pParams != NULL);
	if (!m_pParams)
		return;

	if (bPhysicsChanged)
		Unphysicalize();

	pe_params_pos pp;
	pp.pos = pPlayerEntity->GetWorldPos();
	pp.iSimClass = SC_ACTIVE_RIGID;
	IPhysicalEntity* pPickNThrowProxy = gEnv->pPhysicalWorld->CreatePhysicalEntity(PE_ARTICULATED, &pp, pPlayerEntity, PHYS_FOREIGN_ID_ENTITY);

	phys_geometry *pGeom = NULL;
	switch(m_pParams->proxyShape)
	{
	case ePS_Capsule :
		{
			primitives::capsule prim;
			prim.axis.Set(0,0,1);
			prim.center.zero();
			prim.r = m_pParams->fRadius; 
			prim.hh = m_pParams->fHeight;
			IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::capsule::type, &prim);
			pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

			pGeom->nRefCount = 0;
			pPrimGeom->Release();
		} break;
	case ePS_Sphere :
		{
			primitives::sphere prim;
			prim.center.zero();
			prim.r = m_pParams->fRadius; 
			IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::sphere::type, &prim);
			pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

			pGeom->nRefCount = 0;
			pPrimGeom->Release();
		} break;
	case ePS_Cylinder :
		{
			primitives::cylinder prim;
			prim.axis.Set(0,0,1);
			prim.center.zero();
			prim.r = m_pParams->fRadius; 
			prim.hh = m_pParams->fHeight;
			IGeometry *pPrimGeom = gEnv->pPhysicalWorld->GetGeomManager()->CreatePrimitive(primitives::cylinder::type, &prim);
			pGeom = gEnv->pPhysicalWorld->GetGeomManager()->RegisterGeometry(pPrimGeom, 0);

			pGeom->nRefCount = 0;
			pPrimGeom->Release();
		} break;
	default:
		CRY_ASSERT_MESSAGE(false, "Invalid proxy shape?");
	}

	if (pGeom)
	{
		CRY_ASSERT(pPlayerEntity->GetPhysics() && (pPlayerEntity->GetPhysics()->GetType() == PE_LIVING));
		pe_params_articulated_body pab;
		pab.pHost = pPlayerPhysics;
		m_pLastPlayerPhysics = pPlayerPhysics;
		pab.posHostPivot = m_pParams->vPosPivot;
		pab.bGrounded = 1;
		pab.nJointsAlloc = 1;
		pPickNThrowProxy->SetParams(&pab);

		pe_articgeomparams gp;
		gp.pos.zero();
		gp.flags = CPickAndThrowProxy::geom_colltype_proxy;
		gp.flagsCollider = 0;
		gp.mass = 0.0f;
		pPickNThrowProxy->AddGeometry(pGeom, &gp);

		m_pPickNThrowProxy = pPickNThrowProxy;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CPickAndThrowProxy::Unphysicalize()
{
	if (m_pPickNThrowProxy)
	{
		gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pPickNThrowProxy);
		m_pPickNThrowProxy = NULL;
		m_pLastPlayerPhysics = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CPickAndThrowProxy::OnReloadExtension()
{
	// Entity class could have changed, cache if we need reloading data
	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);

	// Query if params have changed
	CryFixedStringT<64>	sharedParamsName = GetSharedParamsName();
	ISharedParamsConstPtr pSharedParams = pSharedParamsManager->Get(sharedParamsName);

	m_bNeedsReloading = m_pParams != pSharedParams; 
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CPickAndThrowProxy::ReadXmlData(const IItemParamsNode* pRootNode)
{
	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);

	// If we change the SharedParamsManager to accept CRCs on its interface we could compute this once and store
	// the name's CRC32 instead of constructing it here each time this method is invoked (it shouldn't be invoked 
	// too often, though)
	CryFixedStringT<64>	sharedParamsName = GetSharedParamsName();
	ISharedParamsConstPtr pSharedParams = pSharedParamsManager->Get(sharedParamsName);
	if (pSharedParams)
	{
		m_pParams = CastSharedParamsPtr<SPnTProxyParams>(pSharedParams);
	}
	else
	{
		m_pParams.reset();

		SPnTProxyParams newParams;
		const char* szProxyShape = pRootNode->GetAttribute("proxyShape");
		if (szProxyShape)
		{
			if (strcmpi("capsule", szProxyShape) == 0)
			{
				newParams.proxyShape = ePS_Capsule;
			}
			else if (strcmpi("sphere", szProxyShape) == 0)
			{
				newParams.proxyShape = ePS_Sphere;
			}
			else if (strcmpi("cylinder", szProxyShape) == 0)
			{
				newParams.proxyShape = ePS_Cylinder;
			}
		}

		pRootNode->GetAttribute("proxyRadius", newParams.fRadius);
		pRootNode->GetAttribute("proxyHeight", newParams.fHeight);
		pRootNode->GetAttribute("proxyPivot", newParams.vPosPivot);

		m_pParams = CastSharedParamsPtr<SPnTProxyParams>(pSharedParamsManager->Register(sharedParamsName, newParams));
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CryFixedStringT<64>	CPickAndThrowProxy::GetSharedParamsName() const
{
	const char* szEntityClassName = m_player.GetEntityClassName();
	CryFixedStringT<64>	sharedParamsName;
	sharedParamsName.Format("%s_%s", SPnTProxyParams::s_typeInfo.GetName(), szEntityClassName);

	return sharedParamsName;
}
