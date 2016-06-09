// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   EntityObject.cpp
//  Version:     v1.00
//  Created:     18/5/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EntityObject.h"
#include "Entity.h"

#include <CryRenderer/IRenderer.h>
#include <CryEntitySystem/IEntityRenderState.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IGeomCache.h>

#define MAX_CHARACTER_LOD 10

namespace
{
Matrix34 sIdentMatrix = Matrix34::CreateIdentity();
}

//////////////////////////////////////////////////////////////////////////
CEntityObject::~CEntityObject()
{
	ReleaseObjects();
	if (m_pXForm)
		delete m_pXForm;
	FreeCameraSpacePos();
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::ReleaseObjects()
{
	if (pStatObj)
	{
		pStatObj->Release();
		pStatObj = NULL;
	}
	else if (pCharacter)
	{
		if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(0, 0);

		if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
			pSkeletonPose->DestroyCharacterPhysics(0);

		pCharacter->Release();
		pCharacter = NULL;
	}
	else if (pLight)
	{
		pLight->ReleaseNode();
		pLight = NULL;
	}
	else if (IParticleEmitter* pEmitter = GetParticleEmitter())
	{
		pEmitter->Activate(false);
		pEmitter->SetEntity(NULL, 0);
		pEmitter->Release();
		pChildRenderNode = 0;
	}
	else if (pChildRenderNode)
	{
		pChildRenderNode->ReleaseNode();
		pChildRenderNode = 0;
	}
	if (pFoliage)
	{
		pFoliage->Release();
		pFoliage = 0;
	}

	//if(m_pRNData)
	//	gEnv->p3DEngine->FreeRNTmpData(&m_pRNData);
	//assert(!m_pRNData);
}

bool CEntityObject::GetLocalBounds(AABB& bounds)
{
	if (pStatObj)
	{
		bounds.Add(pStatObj->GetAABB());
		return true;
	}
	else if (pCharacter)
	{
		bounds.Add(pCharacter->GetAABB());
		return true;
	}
	else if (pLight)
	{
		bounds.Add(Vec3Constants<float>::fVec3_Zero, 0.1f);
		return true;
	}
	else if (pChildRenderNode)
	{
		pChildRenderNode->GetLocalBounds(bounds);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CEntityObject::GetParticleEmitter() const
{
	if (pChildRenderNode && pChildRenderNode->GetRenderNodeType() == eERType_ParticleEmitter)
		return static_cast<IParticleEmitter*>(pChildRenderNode);
	else
		return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::SetParent(CEntityObject* pParentSlot)
{
	pParent = pParentSlot;
	if (!m_pXForm)
		m_pXForm = new SEntityObjectXForm;
	bWorldTMValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::SetLocalTM(const Matrix34& localTM)
{
	if (!m_pXForm)
		m_pXForm = new SEntityObjectXForm;

	m_pXForm->localTM = localTM;
	bWorldTMValid = false;
	if (pParent)
	{
		pParent->bWorldTMValid = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::SetCameraSpacePos(const Vec3& cameraSpacePos)
{
	if (m_pCameraSpacePos == NULL)
	{
		m_pCameraSpacePos = new Vec3;
	}
	*m_pCameraSpacePos = cameraSpacePos;
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::GetCameraSpacePos(Vec3& cameraSpacePos)
{
	if (m_pCameraSpacePos)
	{
		cameraSpacePos = *m_pCameraSpacePos;
	}
	else
	{
		cameraSpacePos = ZERO;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::FreeCameraSpacePos()
{
	SAFE_DELETE(m_pCameraSpacePos);
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::UpdateWorldTM(CEntity* pEntity)
{
	if (!pParent)
	{
		// Add entity matrix as parent.
		if (!m_pXForm)
			m_worldTM = pEntity->GetWorldTM();
		else
			m_worldTM = pEntity->GetWorldTM() * m_pXForm->localTM;
	}
	else
	{
		if (!m_pXForm)
			m_worldTM = pParent->GetWorldTM(pEntity);
		else
			m_worldTM = pParent->GetWorldTM(pEntity) * m_pXForm->localTM;
	}
	bWorldTMValid = true;
	bObjectMoved = true;
}

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntityObject::GetLocalTM() const
{
	if (m_pXForm)
		return m_pXForm->localTM;
	return sIdentMatrix;
};

//////////////////////////////////////////////////////////////////////////
const Matrix34& CEntityObject::GetWorldTM(CEntity* pEntity)
{
	if (!bWorldTMValid)
	{
		UpdateWorldTM(pEntity);
	}
	return m_worldTM;
}

CLodValue CEntityObject::ComputeLod(int wantedLod, const SRenderingPassInfo& passInfo)
{
	if (pCharacter)
	{
		return pCharacter->ComputeLod(wantedLod, passInfo);
	}

	return CLodValue(wantedLod);
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::Render(CEntity* pEntity, SRendParams& rParams, int nRndFlags, CRenderProxy* pRenderProxy, const SRenderingPassInfo& passInfo)
{
	if (!bWorldTMValid)
	{
		UpdateWorldTM(pEntity);
	}

	// Override with custom slot material.
	IMaterial* pPrevMtl = rParams.pMaterial;
	if (pMaterial)
		rParams.pMaterial = pMaterial;

	int32 nOldObjectFlags = rParams.dwFObjFlags;
	rParams.dwFObjFlags |= dwFObjFlags;

	if (flags & ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING)
	{
		rParams.dwFObjFlags |= FOB_RENDER_AFTER_POSTPROCESSING;
	}

#ifdef SEG_WORLD
	rParams.nCustomFlags |= (1 << (COB_SW_SHIFT + pEntity->GetSwObjDebugFlag()));
#endif // SEG_WORLD

	//////////////////////////////////////////////////////////////////////////

	rParams.pInstance = this;

	const bool bIsInCameraSpace = (flags & ENTITY_SLOT_RENDER_NEAREST) != 0;

	// Draw static object.
	if (pStatObj)
	{
		rParams.pMatrix = &m_worldTM;
		rParams.dwFObjFlags |= FOB_TRANS_MASK;
		rParams.pFoliage = pFoliage;

		rParams.nSubObjHideMask = nSubObjHideMask;

		// make sure object motion blur can be applied to this object
		if (bObjectMoved)
		{
			rParams.dwFObjFlags |= FOB_DYNAMIC_OBJECT;
			bObjectMoved = false;
		}

		Matrix34 entityTM;
		if (bIsInCameraSpace)
		{
			rParams.pMatrix = &entityTM;
			entityTM = m_worldTM;

			// Camera space
			if (m_pCameraSpacePos)
			{
				// Use camera space relative position
				entityTM.SetTranslation(*m_pCameraSpacePos);
			}
			else
			{
				// We don't have camera space relative position, so calculate it out from world space
				// (This will not have the precision advantages of camera space rendering)
				entityTM.AddTranslation(-gEnv->pSystem->GetViewCamera().GetPosition());
			}
		}

		if (rParams.pMatrix->IsValid())
			pStatObj->Render(rParams, passInfo);
		else
			EntityWarning("CEntityObject::Render: Invalid world matrix: %s", pEntity->GetEntityTextDescription().c_str());
	}
	else if (pCharacter)
	{
		QuatTS Offset;
		Matrix34 PhysLocation(pEntity->GetWorldTM());
		if (m_pXForm)
			Offset = QuatTS(m_pXForm->localTM);
		else
		{
			//CRY_FIXME(03,12,2009,"Animation & Rendering of entities needs to be re-written to avoid derivation of local offset due to float inaccuracy - Richard Semmens");

			if (!Matrix34::IsEquivalent(PhysLocation, m_worldTM))
			{
				Matrix34 invPhysLocation = PhysLocation.GetInverted();

				Matrix34 matOffset = invPhysLocation * m_worldTM;

				Offset = QuatTS(matOffset);
			}
			else
			{
				Offset.SetIdentity();
			}
		}

		if (bIsInCameraSpace)
		{
			// Camera space
			if (m_pCameraSpacePos)
			{
				// Use camera space relative position
				const Matrix33 camRot = Matrix33(gEnv->pSystem->GetViewCamera().GetViewMatrix());
				PhysLocation.SetTranslation(*m_pCameraSpacePos * camRot);
			}
			else
			{
				// We don't have camera space relative position, so calculate it out from world space
				// (This will not have the precision advantages of camera space rendering)
				PhysLocation.AddTranslation(-gEnv->pSystem->GetViewCamera().GetPosition());
			}
			Offset.SetIdentity();
		}

		rParams.pMatrix = &PhysLocation;
		//rParams.pInstance = pCharacter;

		// Disable hand-placed (static) decals on characters
		rParams.dwFObjFlags |= FOB_DYNAMIC_OBJECT;

		pCharacter->Render(rParams, Offset, passInfo);

		const uint32 renderProxyFlags = pRenderProxy->GetFlags();
		if (!passInfo.IsShadowPass() || (renderProxyFlags & CRenderProxy::FLAG_ANIMATE_OFFSCREEN_SHADOW))
		{
			// If We render character, make sure it is also gets animation activated.
			if (!pEntity->m_bInActiveList)
				pEntity->ActivateForNumUpdates(8);
		}
	}
	else if (pChildRenderNode)
	{
		rParams.pMatrix = &m_worldTM;
		//rParams.pInstance = pChildRenderNode;

		pChildRenderNode->m_dwRndFlags = nRndFlags;
		pChildRenderNode->Render(rParams, passInfo);
	}

	rParams.pMaterial = pPrevMtl;
	rParams.dwFObjFlags = nOldObjectFlags;

	if (!passInfo.IsShadowPass())   // Should also ignore rendering into the recursion.
	{
		if (pFoliage)
		{
			pFoliage->SetFlags(pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(rParams.nMaterialLayers & MTL_LAYER_FROZEN) & IFoliage::FLAG_FROZEN);
			static ICVar* g_pWindActivationDist = gEnv->pConsole->GetCVar("e_FoliageWindActivationDist");
			float maxdist = g_pWindActivationDist ? g_pWindActivationDist->GetFVal() : 0.0f;
			Vec3 pos = m_worldTM.GetTranslation();
			if (pStatObj && (gEnv->pSystem->GetViewCamera().GetPosition() - pos).len2() < sqr(maxdist) && gEnv->p3DEngine->GetWind(AABB(pos), false).len2() > 101.0f)
				pStatObj->PhysicalizeFoliage(pEntity->GetPhysics(), m_worldTM, pFoliage, 0, 4);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::OnXForm(CEntity* pEntity)
{
	UpdateWorldTM(pEntity);

	if (!m_worldTM.IsValid())
	{
		EntityWarning("CEntityObject::OnXForm: Invalid world matrix: %s", pEntity->GetEntityTextDescription().c_str());
		return;
	}

	if (pLight)
	{
		ILightSource* pLightSource = pLight;
		// Update light positions.
		CDLight* pDLight = &pLightSource->GetLightProperties();

		pDLight->SetPosition(m_worldTM.GetTranslation());
		pDLight->SetMatrix(m_worldTM);
		pDLight->m_sName = pEntity->GetName(); // For debugging only.
		pDLight->m_nEntityId = pEntity->GetId();
		pEntity->UpdateLightClipBounds(*pDLight);
		pLightSource->SetMatrix(m_worldTM);
		pLight->SetSrcEntity(pEntity);
	}
	else if (pChildRenderNode)
	{
		pChildRenderNode->SetMatrix(m_worldTM);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::OnEntityEvent(CEntity* pEntity, SEntityEvent const& event)
{
	if (pChildRenderNode)
		pChildRenderNode->OnEntityEvent(pEntity, event);
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::OnNotSeenTimeout()
{
	//if (pCharacter)
	//	pCharacter->ReleaseTemporaryResources();
}

//////////////////////////////////////////////////////////////////////////
void CEntityObject::Update(CEntity* pEntity, bool bVisible, bool& bBoundsChanged)
{
	bBoundsChanged = false;
	if (pCharacter)
	{
		int characterFlags = pCharacter->GetFlags();
		if ((flags & ENTITY_SLOT_RENDER) || (characterFlags & CS_FLAG_UPDATE_ALWAYS))
		{
			bBoundsChanged = true; // Lets bounds of character change constantly.
			if (characterFlags & CS_FLAG_UPDATE)
			{
				if (!bWorldTMValid)
					UpdateWorldTM(pEntity);

				QuatTS AnimatedCharacter = QuatTS(m_worldTM);
				const CCamera& camera = GetISystem()->GetViewCamera();
				float fDistance = (camera.GetPosition() - AnimatedCharacter.t).GetLength();
				float fZoomFactor = 0.001f + 0.999f * (RAD2DEG(camera.GetFov()) / 60.f);

				SAnimationProcessParams params;
				params.locationAnimation = AnimatedCharacter;
				params.bOnRender = 0;
				params.zoomAdjustedDistanceFromCamera = fDistance * fZoomFactor;
				pCharacter->StartAnimationProcessing(params);
			}
		}
	}
#if defined(USE_GEOM_CACHES)
	else
	{
		IGeomCacheRenderNode* pGeomCacheRenderNode = GetGeomCacheRenderNode();

		if (pGeomCacheRenderNode && pGeomCacheRenderNode->DidBoundsChange())
		{
			bBoundsChanged = true;
		}
	}
#endif
}

void CEntityObject::GetMemoryUsage(ICrySizer* pSizer) const
{
	{
		SIZER_COMPONENT_NAME(pSizer, "CEntityObject Allocator");
		pSizer->AddObject(g_Alloc_EntitySlot);
	}

	pSizer->AddObject(pCharacter);
	pSizer->AddObject(pStatObj);
	pSizer->AddObject(pLight);
	pSizer->AddObject(pChildRenderNode);
	pSizer->AddObject(pMaterial);
}

//////////////////////////////////////////////////////////////////////////
