// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntitySlot.h"
#include "Entity.h"

#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAnimation/ICryAnimation.h>
#include <Cry3DEngine/IGeomCache.h>

#define MAX_CHARACTER_LOD 10

namespace
{
Matrix34 sIdentMatrix = Matrix34::CreateIdentity();
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot::CEntitySlot(CEntity* pEntity)
	: m_pEntity(pEntity)
{
	m_localTM.SetIdentity();
	m_worldTM.SetIdentity();
	m_nSubObjHideMask = 0;
	m_cameraSpacePos.Set(0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot::~CEntitySlot()
{
	ReleaseObjects();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::Clear()
{
	ReleaseObjects();

	m_pMaterial = nullptr;
	SetFlags(0);
	m_nSubObjHideMask = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::ReleaseObjects()
{
	if (m_pRenderNode)
	{
		m_pRenderNode->SetOwnerEntity(nullptr);
	}

	if (m_pStatObj)
	{
		m_pStatObj->Release();
		m_pStatObj = nullptr;
	}
	else if (m_pCharacter)
	{
		if (ISkeletonAnim* pSkeletonAnim = GetCharacter()->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(0, 0);

		if (ISkeletonPose* pSkeletonPose = GetCharacter()->GetISkeletonPose())
			pSkeletonPose->DestroyCharacterPhysics(0);

		m_pCharacter->Release();
		m_pCharacter = nullptr;
	}

	if (IParticleEmitter* pEmitter = GetParticleEmitter())
	{
		pEmitter->SetEntity(nullptr, 0);
		pEmitter->Activate(false);
		pEmitter->Release();
		m_pRenderNode = nullptr;
	}
	else if (m_pRenderNode)
	{
		gEnv->p3DEngine->DeleteRenderNode(m_pRenderNode);
		m_bRegisteredRenderNode = false;
		m_pRenderNode = nullptr;
	}
	m_renderNodeType = eERType_NotRenderNode;
}

//////////////////////////////////////////////////////////////////////////
// Helper function to set/unset rendre node flags
inline void SetRenderNodeFlags(IRenderNode::RenderFlagsType& inout, const IRenderNode::RenderFlagsType flags, bool bEnable)
{
	if (bEnable)
		inout |= flags;
	else
		inout &= ~flags;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::UpdateRenderNode(bool bForceRecreateNode)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY)
	//if (!m_bPostInit)
	//return;

	ComputeWorldTransform();

	bool bSlotShouldRender =
	  (GetFlags() & ENTITY_SLOT_RENDER) &&
	  (!m_pEntity->IsHidden()) &&
	  (!m_pEntity->IsInvisible());

	if (bForceRecreateNode || !bSlotShouldRender)
	{
		if (m_pRenderNode)
		{
			if (bForceRecreateNode)
			{
				// Only release node if we force node recreation.
				// Not should not be released if we only switching it to not rendered slot
				gEnv->p3DEngine->DeleteRenderNode(m_pRenderNode);
				m_bRegisteredRenderNode = false;
				m_pRenderNode = 0;
				m_renderNodeType = eERType_NotRenderNode;
			}
			else if (m_bRegisteredRenderNode)
			{
				gEnv->p3DEngine->UnRegisterEntityDirect(m_pRenderNode);
				m_bRegisteredRenderNode = false;
			}
		}
	}

	if (!m_pRenderNode && bSlotShouldRender)
	{
		// Recreate proper render node in 3d engine if not exist yet.
		if (GetStatObj())
		{
			m_renderNodeType = eERType_MovableBrush;
			IBrush* pBrushRenderNode = static_cast<IBrush*>(gEnv->p3DEngine->CreateRenderNode(eERType_MovableBrush));
			m_pRenderNode = pBrushRenderNode;
			m_pRenderNode->SetOwnerEntity(m_pEntity);
			pBrushRenderNode->DisablePhysicalization(true); // We physicalize render node with PhysicalProxy instead.
			Matrix34A tm = m_worldTM;
			m_pRenderNode->SetEntityStatObj(GetStatObj(), &tm);
		}
		if (GetCharacter())
		{
			m_renderNodeType = eERType_Character;
			ICharacterRenderNode* pCharacterRenderNode = static_cast<ICharacterRenderNode*>(gEnv->p3DEngine->CreateRenderNode(eERType_Character));
			pCharacterRenderNode->SetOwnerEntity(m_pEntity);
			pCharacterRenderNode->SetCharacter(GetCharacter());
			pCharacterRenderNode->SetMatrix(m_worldTM);
			m_pRenderNode = pCharacterRenderNode;
		}
	}

	if (m_pRenderNode)
	{
		const IEntity::SRenderNodeParams& renderNodeParams = m_pEntity->GetEntityRender()->m_renderNodeParams;

		EERType renderNodeType = m_pRenderNode->GetRenderNodeType();

		IRenderNode::RenderFlagsType renderNodeFlags = m_pRenderNode->GetRndFlags();
		bool bSelected, bHighlighted;
		m_pEntity->GetEditorObjectInfo(bSelected, bHighlighted);
		m_pRenderNode->SetEditorObjectInfo(bSelected, bHighlighted);
		m_pRenderNode->SetEditorObjectId(m_pEntity->GetEditorObjectID());

		{
			uint32 entityFlags = m_pEntity->GetFlags();
			SetRenderNodeFlags(renderNodeFlags, ERF_DYNAMIC_DISTANCESHADOWS, 0 != (m_pEntity->m_flagsExtended & ENTITY_FLAG_EXTENDED_DYNAMIC_DISTANCE_SHADOWS));

			SetRenderNodeFlags(renderNodeFlags, ERF_CASTSHADOWMAPS, (0 != (entityFlags & ENTITY_FLAG_CASTSHADOW)) || (0 != (GetFlags() & ENTITY_SLOT_CAST_SHADOW)));
			SetRenderNodeFlags(renderNodeFlags, ERF_GOOD_OCCLUDER, 0 != (entityFlags & ENTITY_FLAG_GOOD_OCCLUDER));
			SetRenderNodeFlags(renderNodeFlags, ERF_OUTDOORONLY, (0 != (entityFlags & ENTITY_FLAG_OUTDOORONLY)) || (0 != (GetFlags() & ENTITY_SLOT_IGNORE_VISAREAS)));
			SetRenderNodeFlags(renderNodeFlags, ERF_RECVWIND, 0 != (entityFlags & ENTITY_FLAG_RECVWIND));
			SetRenderNodeFlags(renderNodeFlags, ERF_NO_DECALNODE_DECALS, 0 != (entityFlags & ENTITY_FLAG_NO_DECALNODE_DECALS));
			SetRenderNodeFlags(renderNodeFlags, ERF_ENABLE_ENTITY_RENDER_CALLBACK, 0 != (entityFlags & ENTITY_FLAG_SEND_RENDER_EVENT));
			SetRenderNodeFlags(renderNodeFlags, ERF_CUSTOM_VIEW_DIST_RATIO, 0 != (entityFlags & ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO));

			SetRenderNodeFlags(renderNodeFlags, ERF_FOB_RENDER_AFTER_POSTPROCESSING, 0 != (GetFlags() & ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING));
			SetRenderNodeFlags(renderNodeFlags, ERF_FOB_NEAREST, 0 != (GetFlags() & ENTITY_SLOT_RENDER_NEAREST));

			SetRenderNodeFlags(renderNodeFlags, ERF_GI_MODE_BIT0, (0 != (m_pEntity->m_flagsExtended & ENTITY_FLAG_EXTENDED_GI_MODE_BIT0)) || (0 != (GetFlags() & ENTITY_SLOT_GI_MODE_BIT0)));
			SetRenderNodeFlags(renderNodeFlags, ERF_GI_MODE_BIT1, (0 != (m_pEntity->m_flagsExtended & ENTITY_FLAG_EXTENDED_GI_MODE_BIT1)) || (0 != (GetFlags() & ENTITY_SLOT_GI_MODE_BIT1)));
			SetRenderNodeFlags(renderNodeFlags, ERF_GI_MODE_BIT2, (0 != (m_pEntity->m_flagsExtended & ENTITY_FLAG_EXTENDED_GI_MODE_BIT2)) || (0 != (GetFlags() & ENTITY_SLOT_GI_MODE_BIT2)));

			if (renderNodeFlags & ERF_CASTSHADOWMAPS)
				renderNodeFlags |= ERF_HAS_CASTSHADOWMAPS;

			bool bPhysicsIsStatic = false;
			IPhysicalEntity* pPhysEnt = m_pEntity->GetPhysicalEntity();
			if (pPhysEnt)
			{
				pe_type eType = pPhysEnt->GetType();
				bPhysicsIsStatic = eType == PE_NONE || eType == PE_STATIC || eType == PE_AREA;
			}
			SetRenderNodeFlags(renderNodeFlags, ERF_MOVES_EVERY_FRAME, !bPhysicsIsStatic);
		}
		renderNodeFlags |= renderNodeParams.additionalRenderNodeFlags;
		m_pRenderNode->SetRndFlags(renderNodeFlags);

		// Update render node location
		m_pRenderNode->SetMatrix(m_worldTM);

		if (!m_bRegisteredRenderNode)
		{
			if (renderNodeType != eERType_ParticleEmitter)
			{
				m_bRegisteredRenderNode = true;
				gEnv->p3DEngine->RegisterEntity(m_pRenderNode);
			}
		}

		IMaterial* pMaterial = (m_pMaterial) ? m_pMaterial.get() : m_pEntity->GetMaterial();
		if (pMaterial != m_pRenderNode->GetMaterialOverride())
		{
			m_pRenderNode->SetMaterial(pMaterial);
		}
		if (pMaterial)
		{
			m_pRenderNode->SetRndFlags(ERF_COLLISION_PROXY, (pMaterial->GetFlags() & MTL_FLAG_COLLISION_PROXY) != 0);
			m_pRenderNode->SetRndFlags(ERF_RAYCAST_PROXY, (pMaterial->GetFlags() & MTL_FLAG_RAYCAST_PROXY) != 0);
		}

		if (renderNodeType == eERType_MovableBrush || renderNodeType == eERType_Brush)
		{
			static_cast<IBrush*>(m_pRenderNode)->SetSubObjectHideMask(m_nSubObjHideMask);
		}

		if (m_bCameraSpacePos)
		{
			m_pRenderNode->SetCameraSpacePos(&m_cameraSpacePos);
		}
		else
		{
			m_pRenderNode->SetCameraSpacePos(nullptr);
		}

		m_pRenderNode->Hide(!bSlotShouldRender);

		if ((m_pEntity->GetFlags() & ENTITY_FLAG_CUSTOM_VIEWDIST_RATIO) == 0)
		{
			if (GetFlags() & ENTITY_SLOT_RENDER_NEAREST)
				m_pRenderNode->SetLodRatio(0); // Use LOD 0 on nearest objects
			else
				m_pRenderNode->SetLodRatio(renderNodeParams.lodRatio);

			m_pRenderNode->SetViewDistRatio(renderNodeParams.viewDistRatio);
		}

		m_pRenderNode->SetMinSpec(renderNodeParams.minSpec);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::OnXForm(int nWhyFlags)
{
	ComputeWorldTransform();

	if (m_pRenderNode)
	{
		m_pRenderNode->SetMatrix(m_worldTM);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySlot::GetLocalBounds(AABB& bounds)
{
	if (GetStatObj())
	{
		bounds.Add(GetStatObj()->GetAABB());
		return true;
	}
	else if (GetCharacter())
	{
		bounds.Add(GetCharacter()->GetAABB());
		return true;
	}
	else if (m_pRenderNode)
	{
		m_pRenderNode->GetLocalBounds(bounds);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CEntitySlot::GetParticleEmitter() const
{
	if (m_renderNodeType == eERType_ParticleEmitter)
		return static_cast<IParticleEmitter*>(m_pRenderNode);
	else
		return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::InvalidateParticleEmitter()
{
	IParticleEmitter* pEmitter = GetParticleEmitter();
	if (pEmitter)
	{
		pEmitter->InvalidateCachedEntityData();
	}
}

bool CEntitySlot::IsRendered() const
{
	if (!(GetFlags() & ENTITY_SLOT_RENDER))
		return false;

	if (GetParticleEmitter())
	{
		return GetParticleEmitter()->IsAlive();
	}
	else if (m_pRenderNode)
	{
		// If node have temp data allocated for it, it is considered to be rendered by the 3dEngine
		if (m_pRenderNode->m_pTempData.load())
			return true;

		if (std::abs((int)m_pRenderNode->GetDrawFrame() - (int)gEnv->nMainFrameID) < 3)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::PreviewRender(SEntityPreviewContext& context)
{
	if (m_pRenderNode)
	{
		if (!context.bNoRenderNodes)
		{
			assert(context.pRenderParams);
			assert(context.pPassInfo);
			m_pRenderNode->Render(*context.pRenderParams, *context.pPassInfo);
		}

		if (context.debugDrawInfo.bDrawInFront)
		{
			DebugDraw(context.debugDrawInfo);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::ComputeWorldTransform()
{
	CEntitySlot* pParentSlot = m_pEntity->m_render.GetSlot(m_parent);
	if (pParentSlot)
	{
		pParentSlot->ComputeWorldTransform();
		m_worldTM = pParentSlot->m_worldTM * m_localTM;
	}
	else
	{
		m_worldTM = m_pEntity->m_worldTM * m_localTM;
	}
	assert(m_worldTM.IsValid());
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::PostInit()
{
	m_bPostInit = true;

	UpdateRenderNode();

	if (m_pRenderNode && m_pRenderNode->GetRenderNodeType() == eERType_Light)
	{
		// Light specific handling, initialization of clip bounds must happen in post init, so that other entities clip bounds are initialized
		ILightSource* pLightNode = static_cast<ILightSource*>(m_pRenderNode);
		m_pEntity->UpdateLightClipBounds(pLightNode->GetLightProperties());
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetLocalTM(const Matrix34& localTM)
{
	m_localTM = localTM;
	ComputeWorldTransform();

	OnXForm(0);
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetCameraSpacePos(const Vec3& cameraSpacePos)
{
	m_cameraSpacePos = cameraSpacePos;
	m_bCameraSpacePos = true;
	UpdateRenderNode();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::GetCameraSpacePos(Vec3& cameraSpacePos)
{
	cameraSpacePos = m_cameraSpacePos;
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CEntityObject::Render(SRendParams& rParams, int nRndFlags, CRenderProxy* pIEntityRender, const SRenderingPassInfo& passInfo)
   {
     // Override with custom slot material.
     IMaterial *pPrevMtl = rParams.pMaterial;
     if (m_pMaterial)
     rParams.pMaterial = m_pMaterial;

     int32 nOldObjectFlags = rParams.dwFObjFlags;
     //rParams.dwFObjFlags |= dwFObjFlags;

     if(m_flags & ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING)
     {
     rParams.dwFObjFlags |= FOB_RENDER_AFTER_POSTPROCESSING;
     }

   return CLodValue(wantedLod);
   }

   //////////////////////////////////////////////////////////////////////////
   void CEntityObject::Render(SRendParams& rParams, int nRndFlags, CRenderProxy* pIEntityRender, const SRenderingPassInfo& passInfo)
   {
   /*
     // Override with custom slot material.
     IMaterial *pPrevMtl = rParams.pMaterial;
     if (m_pMaterial)
     rParams.pMaterial = m_pMaterial;

     int32 nOldObjectFlags = rParams.dwFObjFlags;
     //rParams.dwFObjFlags |= dwFObjFlags;

     if(m_flags & ENTITY_SLOT_RENDER_AFTER_POSTPROCESSING)
     {
     rParams.dwFObjFlags |= FOB_RENDER_AFTER_POSTPROCESSING;
     }

     //////////////////////////////////////////////////////////////////////////

     rParams.pInstance = this;

     const bool bIsInCameraSpace = (m_flags & ENTITY_SLOT_RENDER_NEAREST) != 0;

     m_worldTM = GetWorldTM(m_pEntity);

     // Draw static object.
     if (pStatObj)
     {
     rParams.pMatrix = &m_worldTM;
     rParams.dwFObjFlags |= FOB_TRANS_MASK;
     rParams.pFoliage = pFoliage;

     rParams.nSubObjHideMask = m_nSubObjHideMask;

     // make sure object motion blur can be applied to this object
     if(bObjectMoved)
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
      if(bCameraSpacePos)
      {
        // Use camera space relative position
        entityTM.SetTranslation(m_cameraSpacePos);
      }
      else
      {
        // We don't have camera space relative position, so calculate it out from world space
        // (This will not have the precision advantages of camera space rendering)
        entityTM.AddTranslation(-gEnv->pSystem->GetViewCamera().GetPosition());
      }
     }

     if(rParams.pMatrix->IsValid())
      pStatObj->Render( rParams, passInfo );
     else
      EntityWarning("CEntityObject::Render: Invalid world matrix: %s", m_pEntity->GetEntityTextDescription().c_str());
     }
     else if (pCharacter)
     {
     QuatTS Offset;
     Matrix34 PhysLocation(m_pEntity->GetWorldTM());
     if (m_pXForm)
      Offset = QuatTS(m_pXForm->localTM);
     else
     {
      //CRY_FIXME(03,12,2009,"Animation & Rendering of entities needs to be re-written to avoid derivation of local offset due to float inaccuracy - Richard Semmens");

      if(!Matrix34::IsEquivalent(PhysLocation,m_worldTM))
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
      if(m_pCameraSpacePos)
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

     const uint32 renderProxyFlags = pIEntityRender->GetFlags();
     if( !passInfo.IsShadowPass() || (renderProxyFlags & CRenderProxy::FLAG_ANIMATE_OFFSCREEN_SHADOW))
     {
      // If We render character, make sure it is also gets animation activated.
      if (!m_pEntity->m_bInActiveList)
        m_pEntity->ActivateForNumUpdates(8);
     }
     }
     else if (pChildRenderNode)
     {
     rParams.pMatrix = &m_worldTM;
     //rParams.pInstance = pChildRenderNode;

     pChildRenderNode->m_dwRndFlags = nRndFlags;
     pChildRenderNode->Render( rParams, passInfo );
     }

     rParams.pMaterial = pPrevMtl;
     rParams.dwFObjFlags = nOldObjectFlags;

     if ( !passInfo.IsShadowPass() ) // Should also ignore rendering into the recursion.
     {
     if (pFoliage)
     {
      pFoliage->SetFlags(pFoliage->GetFlags() & ~IFoliage::FLAG_FROZEN | -(int)(rParams.nMaterialLayers&MTL_LAYER_FROZEN) & IFoliage::FLAG_FROZEN);
      static ICVar *g_pWindActivationDist = gEnv->pConsole->GetCVar("e_FoliageWindActivationDist");
      float maxdist = g_pWindActivationDist ? g_pWindActivationDist->GetFVal() : 0.0f;
      Vec3 pos = m_worldTM.GetTranslation();
      if (pStatObj && (gEnv->pSystem->GetViewCamera().GetPosition()-pos).len2()<sqr(maxdist) && gEnv->p3DEngine->GetWind(AABB(pos),false).len2()>101.0f)
        pStatObj->PhysicalizeFoliage(m_pEntity->GetPhysics(),m_worldTM,pFoliage,0,4);
     }
     }
   }
 */

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetFlags(uint32 nFlags)
{
	bool bChange = m_flags != nFlags;
	uint32 prevFlags = m_flags;
	m_flags = nFlags;

	if (bChange)
	{
		if (!(m_flags & ENTITY_SLOT_RENDER_NEAREST) && (prevFlags & ENTITY_SLOT_RENDER_NEAREST))
		{
			// When turn off nearest also turn off camera space position calculation.
			// So that if this flag is turned on again and we don't have a camera space
			// position then it will default to calculate the camera space pos from the WS pos
			m_bCameraSpacePos = false;
		}
		UpdateRenderNode();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetRenderFlag(bool bEnable)
{
	if (bEnable)
		m_flags |= ENTITY_SLOT_RENDER;
	else
		m_flags &= ~ENTITY_SLOT_RENDER;
}

void CEntitySlot::SetHidemask(hidemask& mask)
{
	bool bChange = m_nSubObjHideMask != mask;
	m_nSubObjHideMask = mask;
	if (bChange)
	{
		UpdateRenderNode();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetMaterial(IMaterial* pMtl)
{
	bool bChange = m_pMaterial != pMtl;
	m_pMaterial = pMtl;
	if (bChange)
	{
		UpdateRenderNode();
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntitySlot::GetBoundsChanged() const
{
	if (GetCharacter())
	{
		int characterFlags = GetCharacter()->GetFlags();
		if ((GetFlags() & ENTITY_SLOT_RENDER) || (characterFlags & CS_FLAG_UPDATE_ALWAYS))
		{
			return true; // Lets bounds of character change constantly.
		}
	}
#if defined(USE_GEOM_CACHES)
	else
	{
		if (GetGeomCacheRenderNode())
		{
			return GetGeomCacheRenderNode()->DidBoundsChange();
		}
	}
#endif
	return false;
}

void CEntitySlot::GetSlotInfo(SEntitySlotInfo& slotInfo) const
{
	slotInfo.nFlags = GetFlags();
	slotInfo.nParentSlot = -1;

	slotInfo.pStatObj = GetStatObj();
	slotInfo.pCharacter = GetCharacter();
	slotInfo.pLight = (m_pRenderNode && m_pRenderNode->GetRenderNodeType() == eERType_Light) ? static_cast<ILightSource*>(m_pRenderNode) : nullptr;
	slotInfo.pMaterial = m_pMaterial;
	slotInfo.pLocalTM = &m_localTM;
	slotInfo.pWorldTM = &m_worldTM;
	slotInfo.pChildRenderNode = m_pRenderNode;
	slotInfo.pParticleEmitter = GetParticleEmitter();
#if defined(USE_GEOM_CACHES)
	slotInfo.pGeomCacheRenderNode =
	  (m_pRenderNode && m_pRenderNode->GetRenderNodeType() == eERType_GeomCache)
	  ? static_cast<IGeomCacheRenderNode*>(m_pRenderNode) : NULL;
#endif
	slotInfo.nSubObjHideMask = m_nSubObjHideMask;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetAsLight(const SRenderLight& lightData, uint16 layerId)
{
	if (m_pRenderNode && m_pRenderNode->GetRenderNodeType() != eERType_Light)
	{
		ReleaseObjects();
	}
	if (!m_pRenderNode)
	{
		m_pRenderNode = gEnv->p3DEngine->CreateLightSource();
		m_renderNodeType = m_pRenderNode->GetRenderNodeType();
	}
	ILightSource* pLightNode = static_cast<ILightSource*>(m_pRenderNode);

	pLightNode->SetLayerId(layerId);
	pLightNode->SetLightProperties(lightData);

	SRenderLight& newLightData = pLightNode->GetLightProperties();
	newLightData.m_sName = m_pEntity->GetName(); // For debugging only.
	newLightData.m_nEntityId = m_pEntity->GetId();

	auto lightNodeMatrix = pLightNode->GetMatrix();
	newLightData.SetPosition(lightNodeMatrix.GetTranslation());
	newLightData.SetMatrix(lightNodeMatrix);

	m_flags |= ENTITY_SLOT_RENDER;

	UpdateRenderNode();
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetRenderNode(IRenderNode* pRenderNode)
{
	if (pRenderNode != m_pRenderNode)
	{
		ReleaseObjects();
	}
	m_pRenderNode = pRenderNode;

	if (m_pRenderNode)
	{
		m_flags |= ENTITY_SLOT_RENDER;

		m_renderNodeType = m_pRenderNode->GetRenderNodeType();

		m_pRenderNode->SetOwnerEntity(m_pEntity);

		bool bSelected, bHighlighted;
		m_pEntity->GetEditorObjectInfo(bSelected, bHighlighted);
		m_pRenderNode->SetEditorObjectInfo(bSelected, bHighlighted);
		m_pRenderNode->SetEditorObjectId(m_pEntity->GetEditorObjectID());
	}
	else
	{
		m_renderNodeType = eERType_NotRenderNode;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::DebugDraw(const SGeometryDebugDrawInfo& info)
{
	if (GetStatObj())
	{
		SGeometryDebugDrawInfo subInfo = info;
		subInfo.tm = GetWorldTM();
		GetStatObj()->DebugDraw(subInfo);
	}
#if defined(USE_GEOM_CACHES)
	else if (IGeomCacheRenderNode* pGeomCacheRenderNode = GetGeomCacheRenderNode())
	{
		SGeometryDebugDrawInfo subInfo = info;
		subInfo.tm = GetWorldTM();
		pGeomCacheRenderNode->DebugDraw(info);
	}
#endif //defined(USE_GEOM_CACHES)
}

void CEntitySlot::ShallowCopyFrom(CEntitySlot* pSrcSlot)
{
	//--- Flush out any existing slot objects
	ReleaseObjects();

	//--- Shallow copy over slot
	SetStatObj(pSrcSlot->GetStatObj());
	pSrcSlot->m_pStatObj = nullptr;
	SetCharacter(pSrcSlot->GetCharacter());
	pSrcSlot->m_pCharacter = nullptr;

	m_pRenderNode = pSrcSlot->m_pRenderNode;
	pSrcSlot->m_pRenderNode = nullptr;

	m_pMaterial = pSrcSlot->m_pMaterial;

	m_renderNodeType = pSrcSlot->m_renderNodeType;

	SetFlags(pSrcSlot->GetFlags());
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetCharacter(ICharacterInstance* val)
{
	if (m_pCharacter != val)
	{
		ReleaseObjects();
	}
	m_pCharacter = val;
}

//////////////////////////////////////////////////////////////////////////
void CEntitySlot::SetStatObj(IStatObj* val)
{
	if (m_pStatObj != val)
	{
		ReleaseObjects();
	}
	m_pStatObj = val;
}
