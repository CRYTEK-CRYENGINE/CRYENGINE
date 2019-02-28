// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "RenderProxy.h"
#include "Entity.h"
#include <CryEntitySystem/IEntity.h>
#include "EntitySlot.h"
#include <CryNetwork/ISerialize.h>
#include <CryParticleSystem/ParticleParams.h>

#include <CryRenderer/IRenderer.h>
#include <Cry3DEngine/IRenderNode.h>
#include <CrySystem/ISystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryMemory/AddressHelpers.h>

inline void CheckIfBBoxValid(const AABB& box, CEntity* pEntity)
{
#ifndef _RELEASE
	float v = box.min.x + box.min.y + box.min.z + box.max.x + box.max.y + box.max.z;
	if (!_finite(v))
	{
		CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_WARNING, "BBox contain invalid floating values for Entity: %s", pEntity->GetEntityTextDescription().c_str());
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
CEntityRender::CEntityRender()
{
	m_fLastSeenTime = gEnv->pTimer->GetCurrTime();
}

//////////////////////////////////////////////////////////////////////////
CEntityRender::~CEntityRender()
{
	FreeAllSlots();
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::PostInit()
{
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot)
		{
			pSlot->PostInit();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::RegisterEventListeners(IEntityComponent::ComponentEventPriority eventPriority)
{
	EntityEventMask events = ENTITY_EVENT_BIT(ENTITY_EVENT_XFORM) | ENTITY_EVENT_BIT(ENTITY_EVENT_ATTACH_THIS) | ENTITY_EVENT_BIT(ENTITY_EVENT_DETACH_THIS) | ENTITY_EVENT_BIT(ENTITY_EVENT_LINK)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_DELINK) | ENTITY_EVENT_BIT(ENTITY_EVENT_ENABLE_PHYSICS) | ENTITY_EVENT_BIT(ENTITY_EVENT_PHYSICS_CHANGE_STATE) | ENTITY_EVENT_BIT(ENTITY_EVENT_HIDE)
		| ENTITY_EVENT_BIT(ENTITY_EVENT_INVISIBLE) | ENTITY_EVENT_BIT(ENTITY_EVENT_UNHIDE) | ENTITY_EVENT_BIT(ENTITY_EVENT_VISIBLE) | ENTITY_EVENT_BIT(ENTITY_EVENT_ANIM_EVENT);

	GetEntity()->AddSimpleEventListeners(events, this, eventPriority);
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::AnimEventCallback(ICharacterInstance* pCharacter, void* userdata)
{
	if (CEntityRender* pInstance = static_cast<CEntityRender*>(userdata))
	{
		pInstance->AnimationEvent(pCharacter, pCharacter->GetISkeletonAnim()->GetLastAnimEvent());
	}

	return 1;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityRender::IsRendered() const
{
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot && pSlot->IsRendered())
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::PreviewRender(SEntityPreviewContext& context)
{
	if (context.bRenderSlots)
	{
		for (CEntitySlot* pSlot : m_slots)
		{
			if (pSlot && (pSlot->GetFlags() & ENTITY_SLOT_RENDER))
			{
				pSlot->PreviewRender(context);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::AnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& animEvent)
{
	// If the event is a sound event, make sure we have an audio component before sending the event
	if (animEvent.m_EventName != nullptr && stricmp(animEvent.m_EventName, "sound") == 0)
	{
		GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
	}

	// Send an entity event.
	SEntityEvent event(ENTITY_EVENT_ANIM_EVENT);
	event.nParam[0] = (INT_PTR)&animEvent;
	event.nParam[1] = (INT_PTR)pCharacter;
	GetEntity()->SendEvent(event);
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot* CEntityRender::AllocSlot(int nIndex)
{
	if (nIndex >= (int)m_slots.size())
	{
		m_slots.resize(nIndex + 1);
		m_slots[nIndex] = new CEntitySlot(GetEntity());
	}
	else
	{
		if (m_slots[nIndex] == NULL)
		{
			m_slots[nIndex] = new CEntitySlot(GetEntity());
		}
	}

	return m_slots[nIndex];
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::FreeSlot(int nIndex)
{
	//	assert(nIndex>=0 && nIndex < (int)m_slots.size());
	if ((nIndex >= 0 && nIndex < (int)m_slots.size()) && (m_slots[nIndex] != NULL))
	{
		CEntitySlot* pSlot = m_slots[nIndex];

		m_slots[nIndex] = NULL;

		if (nIndex == m_slots.size() - 1)
		{
			// If Last slot, then cut away all empty slots at the end of the array.
			for (int i = nIndex; i >= 0 && m_slots[i] == NULL; i--)
				m_slots.pop_back();
		}
		delete pSlot;

		InvalidateLocalBounds();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::FreeAllSlots()
{
	// Free all slots.
	for (uint32 i = 0; i < m_slots.size(); i++)
	{
		if (m_slots[i])
		{
			FreeSlot(i);
		}
	}
	m_slots.clear();
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot* CEntityRender::GetOrMakeSlot(int& nSlot)
{
	if (nSlot < 0)
	{
		// find free slot
		for (size_t i = 0, num = m_slots.size(); i < num; ++i)
		{
			if (!m_slots[i])
			{
				nSlot = i;
				break;
			}
		}
		if (nSlot < 0)
		{
			nSlot = (int)m_slots.size();
		}
	}
	return AllocSlot(nSlot);
}

//////////////////////////////////////////////////////////////////////////
CEntitySlot* CEntityRender::GetParentSlot(int nIndex) const
{
	CEntitySlot* pSlot = GetSlot(nIndex);
	if (pSlot)
	{
		return GetSlot(pSlot->GetParent());
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityRender::SetParentSlot(int nParentIndex, int nChildIndex)
{
	// Not implemented
	CEntitySlot* pSlot = GetSlot(nChildIndex);
	if (pSlot)
	{
		pSlot->SetParent(nParentIndex);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CEntityRender::GetCompoundObj() const
{
	return m_slots.size() == 1 && Slot(0)->GetStatObj() && Slot(0)->GetStatObj()->GetFlags() & STATIC_OBJECT_COMPOUND ? Slot(0)->GetStatObj() : 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetSlotLocalTM(int nIndex, const Matrix34& localTM, EntityTransformationFlagsMask transformReasons)
{
	if (IsSlotValid(nIndex))
	{
		Slot(nIndex)->SetLocalTM(localTM);

		if (!(transformReasons & ENTITY_XFORM_NOT_REREGISTER))   // A special optimization for characters
		{
			// Invalidate bounding box.
			ComputeLocalBounds(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetSlotCameraSpacePos(int nIndex, const Vec3& cameraSpacePos)
{
	if (IsSlotValid(nIndex))
	{
		CEntitySlot* pSlot = Slot(nIndex);
		if (pSlot)
		{
			pSlot->SetCameraSpacePos(cameraSpacePos);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::GetSlotCameraSpacePos(int nIndex, Vec3& cameraSpacePos) const
{
	if (IsSlotValid(nIndex))
	{
		CEntitySlot* pSlot = Slot(nIndex);
		if (pSlot)
		{
			pSlot->GetCameraSpacePos(cameraSpacePos);
			return;
		}
	}
	cameraSpacePos = ZERO;
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CEntityRender::GetSlotWorldTM(int nIndex) const
{
	IStatObj* pCompObj;
	if (!(nIndex & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
	{
		if (IStatObj::SSubObject* pSubObj = pCompObj->GetSubObject(nIndex))
		{
			return Slot(0)->GetWorldTM() * pSubObj->tm;
		}
		else
		{
			return Slot(0)->GetWorldTM();
		}
	}
	nIndex &= ~ENTITY_SLOT_ACTUAL;
	if (IsSlotValid(nIndex))
		return Slot(nIndex)->GetWorldTM();
	return GetEntity()->GetWorldTM();
}

//////////////////////////////////////////////////////////////////////////
Matrix34 CEntityRender::GetSlotLocalTM(int nIndex, bool bRelativeToParent) const
{
	IStatObj* pCompObj;

	if (!(nIndex & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
	{
		if (IStatObj::SSubObject* pSubObj = pCompObj->GetSubObject(nIndex))
		{
			return Slot(0)->GetLocalTM() * pSubObj->tm;
		}
		else
		{
			return Slot(0)->GetLocalTM();
		}
	}

	if (IsSlotValid(nIndex &= ~ENTITY_SLOT_ACTUAL))
	{
		// Check if this slot has a parent
		return Slot(nIndex)->GetLocalTM();
	}

	return IDENTITY;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetLocalBounds(const AABB& bounds, bool doNotRecalculate)
{
	m_localBBox = bounds;

	GetEntity()->SetInternalFlag(CEntity::EInternalFlag::FixedBounds, doNotRecalculate);
	GetEntity()->SetInternalFlag(CEntity::EInternalFlag::ValidBounds, true);

	if (!doNotRecalculate)
	{
		ComputeLocalBounds(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::InvalidateLocalBounds()
{
	ComputeLocalBounds(true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::ComputeLocalBounds(bool bForce)
{
	// If local bounding box is forced from outside, do not calculate it automatically.
	if (GetEntity()->HasInternalFlag(CEntity::EInternalFlag::FixedBounds) || (GetEntity()->HasInternalFlag(CEntity::EInternalFlag::ValidBounds) && !bForce))
		return;

	bool bBBoxInitialized = false;
	AABB box;
	m_localBBox.Reset();

	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot && (pSlot->GetFlags() & ENTITY_SLOT_RENDER))   // Only renderable slots contribute to the bounding box.
		{
			box.Reset();
			if (pSlot->GetLocalBounds(box))
			{
				CheckIfBBoxValid(box, GetEntity());

				// If offset matrix is used transform slot bounds by slot local offset matrix.
				box.SetTransformedAABB(pSlot->GetLocalTM(), box);

				// Add local slot bounds to local rendering bounds.
				m_localBBox.Add(box.min);
				m_localBBox.Add(box.max);
				bBBoxInitialized = true;
			}
		}
	}

	if (!bBBoxInitialized)
	{
		m_localBBox.min.Set(0, 0, 0);
		m_localBBox.max.Set(0, 0, 0);
	}

	GetEntity()->SetInternalFlag(CEntity::EInternalFlag::ValidBounds, true);
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::GetLocalBounds(AABB& bbox) const
{
	const_cast<CEntityRender*>(this)->ComputeLocalBounds();
	bbox = m_localBBox;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::GetWorldBounds(AABB& bbox) const
{
	const_cast<CEntityRender*>(this)->ComputeLocalBounds();
	bbox = m_localBBox;
	bbox.SetTransformedAABB(GetEntity()->GetWorldTM(), bbox);

	CheckIfBBoxValid(bbox, GetEntity());
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::ProcessEvent(const SEntityEvent& event)
{
	// See AddSimpleEventListener calls in CEntityRender::PostInit
	switch (event.event)
	{
	case ENTITY_EVENT_XFORM:
		OnEntityXForm((int)event.nParam[0]);
		break;

	case ENTITY_EVENT_ATTACH_THIS:
	case ENTITY_EVENT_DETACH_THIS:
		OnEntityXForm(0);
		break;

	case ENTITY_EVENT_LINK:
	case ENTITY_EVENT_DELINK:
	case ENTITY_EVENT_ENABLE_PHYSICS:
	case ENTITY_EVENT_PHYSICS_CHANGE_STATE:
		// Needs to inform particle system.
		if (GetEntity()->HasInternalFlag(CEntity::EInternalFlag::HasParticles))
		{
			// Only particles need this to be processed
			for (CEntitySlot* pSlot : m_slots)
			{
				if (pSlot && pSlot->GetRenderNodeType() == eERType_ParticleEmitter)
				{
					pSlot->InvalidateParticleEmitter();
				}
			}
		}
		break;

	case ENTITY_EVENT_HIDE:
	case ENTITY_EVENT_INVISIBLE:
	case ENTITY_EVENT_UNHIDE:
	case ENTITY_EVENT_VISIBLE:
		UpdateRenderNodes();
		break;
	case ENTITY_EVENT_ANIM_EVENT:
		{
			const AnimEventInstance* pAnimEvent = reinterpret_cast<const AnimEventInstance*>(event.nParam[0]);
			if (pAnimEvent)
			{
				ICharacterInstance* pCharacter = reinterpret_cast<ICharacterInstance*>(event.nParam[1]);
				ISkeletonAnim* pSkeletonAnim = (pCharacter ? pCharacter->GetISkeletonAnim() : 0);

				// Spawn the event
				if (!GetEntity()->IsInvisible() && !GetEntity()->IsHidden() && pSkeletonAnim && pAnimEvent->m_EventName && pAnimEvent->m_CustomParameter)
				{
					for (auto && pSlot : m_slots)
					{
						if (pSlot && pSlot->GetCharacter() == pCharacter)
						{
							pSlot->GetCharacter()->SpawnSkeletonEffect(*pAnimEvent, QuatTS(pSlot->GetWorldTM()));
						}
					}
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::OnEntityXForm(int nWhyFlags)
{
	// Invalidate cached world matrices for all slots.
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot)
		{
			pSlot->OnXForm(nWhyFlags);
		}
	}
}

//////////////////////////////////////////////////////////////////
void CEntityRender::UpdateRenderNodes()
{
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot)
		{
			pSlot->UpdateRenderNode();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::CheckLocalBoundsChanged()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	bool bBoundsChanged = false;
	//////////////////////////////////////////////////////////////////////////
	// Update all slots.
	for (auto& pSlot : m_slots)
	{
		if (pSlot)
		{
			if (pSlot->GetBoundsChanged())
			{
				bBoundsChanged = true;
				break;
			}
		}
	}
	if (bBoundsChanged)
	{
		ComputeLocalBounds(true);
	}
}

//////////////////////////////////////////////////////////////////////////
IRenderNode* CEntityRender::GetRenderNode(int nSlot) const
{
	if (nSlot >= 0)
	{
		if (IsSlotValid(nSlot))
			return Slot(nSlot)->GetRenderNode();
	}
	else
	{
		for (CEntitySlot* pSlot : m_slots)
		{
			if (pSlot && pSlot->GetRenderNode())
			{
				return pSlot->GetRenderNode();
			}
		}
	}
	return nullptr;
}

//#TODO Implement  ENTITY_FLAG_SEND_RENDER_EVENT, ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT, FLAG_POST_3D_RENDER
/*
   //////////////////////////////////////////////////////////////////////////
   // cppcheck-suppress passedByValue
   void CRenderProxy::Render_JobEntry( const SRendParams inRenderParams, const SRenderingPassInfo passInfo )
   {
   CRY_PROFILE_FUNCTION(PROFILE_ENTITY );

   // Not draw if invalid bounding box.
   if (m_nFlags&(FLAG_HIDDEN|FLAG_BBOX_INVALID))
   return; //false;

   // Make a local copy of render params as some of the parameters will be modified.
   SRendParams rParams(inRenderParams);

   if (m_Callbacks.size() > 0)
   {
   // if we have callbacks be sure the shader params are properly initialized
   IShaderPublicParams* pParams = GetShaderPublicParams(true);

   // get the current active material
   IMaterial* pMaterial = GetEntity()->GetMaterial();
   if (pMaterial == NULL)
    pMaterial = GetSlotMaterial(0);

   TCallbackVector::iterator itEnd = m_Callbacks.end();
   for (TCallbackVector::iterator it = m_Callbacks.begin(); it != itEnd; ++it)
   {
    (*it)->SetupShaderParams(pParams, pMaterial);
   }
   }

   if (m_pShaderPublicParams)
   m_pShaderPublicParams->AssignToRenderParams( rParams );

   rParams.nVisionParams = m_nVisionParams;
   rParams.nHUDSilhouettesParams = m_nHUDSilhouettesParams;

   UpdateEffectLayersParams( rParams );

   rParams.fAlpha = GetOpacity();
   rParams.pRenderNode = this;

   if(rParams.nMaterialLayersBlend & MTL_LAYER_BLEND_CLOAK)
   {
   rParams.nCustomData = m_nCloakColorChannel;
   }

   if(CheckFlags(FLAG_DISABLE_MOTIONBLUR))
   {
   rParams.nCustomFlags |= COB_DISABLE_MOTIONBLUR;
   }

   if(CheckFlags(FLAG_CUSTOM_POST_EFFECT))
   {
   rParams.nCustomFlags |= COB_CUSTOM_POST_EFFECT;
   rParams.nCustomData = m_nCustomData;
   }

   if(CheckFlags(FLAG_IGNORE_HUD_INTERFERENCE_FILTER))
   {
   rParams.nCustomFlags |= COB_IGNORE_HUD_INTERFERENCE_FILTER;
   }

   if(CheckFlags(FLAG_IGNORE_HEAT_VALUE))
   {
   rParams.nCustomFlags |= COB_IGNORE_HEAT_AMOUNT;
   }

   if(CheckFlags(FLAG_POST_3D_RENDER))
   {
   rParams.nCustomFlags |= COB_POST_3D_RENDER;
   rParams.nCustomData = m_nCustomData;
   memcpy(rParams.fCustomData,m_fCustomData,sizeof(float)*4);
   }

   if(CheckFlags(FLAG_HUD_REQUIRE_DEPTHTEST))
   {
   rParams.nCustomFlags |= COB_HUD_REQUIRE_DEPTHTEST;
   }

   if(CheckFlags(FLAG_HUD_DISABLEBLOOM))
   {
   rParams.nCustomFlags |= COB_HUD_DISABLEBLOOM;
   }

   if (m_nEntityFlags & ENTITY_FLAG_SEND_RENDER_EVENT)
   {
   SEntityEvent event(ENTITY_EVENT_RENDER);
   event.nParam[0] = (INT_PTR)&inRenderParams;
   GetEntity()->SendEvent(event);
   m_fLastSeenTime=gEnv->pTimer->GetCurrTime();
   }
   if (s_pTimeoutList && (m_nEntityFlags & ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT))
   s_pTimeoutList->ResetTimeout(m_entityId);

   if(GetEntityVisArea())
   {
   rParams.dwFObjFlags |= FOB_IN_DOORS;
   }

   //////////////////////////////////////////////////////////////////////////
   // Draw all slots.
   for (Slots::iterator it = m_slots.begin(),endit = m_slots.end(); it != endit; ++it)
   {
   CEntityObject *pSlot = *it;
   if (pSlot && (pSlot->m_flags & ENTITY_SLOT_RENDER))
   {
    static ICVar *s_pSkipVertexAnimationLOD = gEnv->pConsole->GetCVar("ca_vaSkipVertexAnimationLOD");
    if (pSlot->pCharacter && pSlot->pCharacter->HasVertexAnimation() && s_pSkipVertexAnimationLOD->GetIVal())
    {
      // do not use LOD dissolve in this case, see CCharInstance::ComputeLod()
      if (inRenderParams.lodValue.LodA() == -1)
        continue;
      rParams.lodValue = pSlot->ComputeLod(inRenderParams.lodValue.LodA(), passInfo);
    }
    pSlot->Render( GetEntity(), rParams, m_dwRndFlags,this, passInfo );
   }
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
void CEntityRender::DebugDraw(const SGeometryDebugDrawInfo& info)
{
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot)
		{
			pSlot->DebugDraw(info);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityRender::GetSlotInfo(int nIndex, SEntitySlotInfo& slotInfo) const
{
	if (!IsSlotValid(nIndex))
		return false;
	Slot(nIndex)->GetSlotInfo(slotInfo);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetSlotFlags(int nSlot, uint32 nFlags)
{
	if (nSlot < 0)
	{
		// All slots.
		for (CEntitySlot* pSlot : m_slots)
		{
			if (pSlot)
				pSlot->SetFlags(nFlags);
		}
	}
	else if (IsSlotValid(nSlot))
	{
		Slot(nSlot)->SetFlags(nFlags);
	}
	ComputeLocalBounds(true);
}

//////////////////////////////////////////////////////////////////////////
uint32 CEntityRender::GetSlotFlags(int nSlot) const
{
	if (IsSlotValid(nSlot))
	{
		return Slot(nSlot)->GetFlags();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetSlotMaterial(int nSlot, IMaterial* pMaterial)
{
	if (IsSlotValid(nSlot))
	{
		Slot(nSlot)->SetMaterial(pMaterial);
	}
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CEntityRender::GetSlotMaterial(int nSlot) const
{
	if (IsSlotValid(nSlot))
	{
		return Slot(nSlot)->GetMaterial();
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::SetSlotGeometry(int nSlot, IStatObj* pStatObj)
{
	IStatObj* pCompObj = 0;
	if (!(nSlot & ENTITY_SLOT_ACTUAL))
		pCompObj = GetCompoundObj();
	else if (nSlot != -1)
		nSlot &= ~ENTITY_SLOT_ACTUAL;
	if (pCompObj && (!pStatObj || !(pStatObj->GetFlags() & STATIC_OBJECT_COMPOUND)))
	{
		if (nSlot < 0)
			nSlot = pCompObj->GetSubObjectCount();
		if (!(pCompObj->GetFlags() & STATIC_OBJECT_GENERATED))
		{
			IStatObj* pNewCompObj = pCompObj->Clone(false, false, true);
			pNewCompObj->SetFlags(pNewCompObj->GetFlags() | STATIC_OBJECT_GENERATED);
			pNewCompObj->AddRef();
			Slot(0)->SetStatObj(pCompObj = pNewCompObj);
		}
		IStatObj::SSubObject* pSubObj;
		if (!(pSubObj = pCompObj->GetSubObject(nSlot)))
		{
			pCompObj->SetSubObjectCount(nSlot + 1);
			pSubObj = pCompObj->GetSubObject(nSlot);
		}
		else if (pSubObj->pStatObj == pStatObj)
		{
			pCompObj->Invalidate(false);
			return nSlot;
		}
		if (!(pSubObj->bHidden = pStatObj == 0))
			pStatObj->AddRef();
		if (pSubObj->pStatObj)
			pSubObj->pStatObj->Release();
		pSubObj->pStatObj = pStatObj;
		pCompObj->Invalidate(false);

		UpdateRenderNodes();
	}
	else
	{
		CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

		// Check if loading the same object.
		if (pSlot->GetStatObj() == pStatObj)
		{
			ComputeLocalBounds(true);
			return nSlot;
		}

		if (pStatObj)
			pStatObj->AddRef();

		if (pStatObj)
			pSlot->SetRenderFlag(true);
		else
			pSlot->SetRenderFlag(false);

		pSlot->SetStatObj(pStatObj);
		pSlot->UpdateRenderNode();
	}

	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadGeometry(int nSlot, const char* szFilename, const char* szGeomName, int nLoadFlags)
{
	if (szFilename == nullptr || szFilename[0] == '\0')
	{
		EntityWarning("[RenderProxy::LoadGeometry] Called with empty filename, Entity: %s", GetEntity()->GetEntityTextDescription().c_str());
		return -1;
	}

	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	// Check if loading the same object.
	if ((pSlot->GetStatObj()) && !(pSlot->GetStatObj()->GetFlags() & (STATIC_OBJECT_CLONE | STATIC_OBJECT_GENERATED)) && (pSlot->GetStatObj()->IsSameObject(szFilename, szGeomName)))
	{
		return nSlot;
	}

	IStatObj* pStatObj;
	if (szGeomName != nullptr && szGeomName[0] != '\0')
	{
		IStatObj::SSubObject* pSubObject = nullptr;
		pStatObj = GetI3DEngine()->LoadStatObj(szFilename, szGeomName, &pSubObject);
		if (pStatObj == nullptr)
		{
			//EntityFileWarning( sFilename,"Failed to load sub-object geometry (%s)", sGeomName );
			return -1;
		}
		if (pSubObject != nullptr && !pSubObject->bIdentityMatrix)
		{
			// Set sub object matrix into the slot transformation matrix.
			pSlot->SetLocalTM(pSubObject->tm);
		}
	}
	else
	{
		IRenderNode::EGIMode usageMode = (IRenderNode::EGIMode)((GetEntity()->m_flagsExtended & ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK) >> ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET);

		if (usageMode == IRenderNode::eGM_IntegrateIntoTerrain)
		{
			// disable asynchronous mesh streaming because this mesh could be required immediately
			nLoadFlags |= IEntity::EF_NO_STREAMING;
		}

		pStatObj = GetI3DEngine()->LoadStatObj(szFilename, nullptr, nullptr, (nLoadFlags& IEntity::EF_NO_STREAMING) == 0);
	}

	SetSlotGeometry(nSlot, pStatObj);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::SetSlotCharacter(int nSlot, ICharacterInstance* pCharacter)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	// Check if loading the same character
	if (pSlot->GetCharacter() == pCharacter)
	{
		return nSlot;
	}

	if (pCharacter)
		pCharacter->AddRef();

	if (pCharacter)
	{
		pSlot->SetCharacter(pCharacter);
		pSlot->SetRenderFlag(true);

		pCharacter->SetFlags(pCharacter->GetFlags() | CS_FLAG_UPDATE);

		// Register ourselves to listen for animation events coming from the character.
		if (ISkeletonAnim* pSkeletonAnim = pCharacter->GetISkeletonAnim())
			pSkeletonAnim->SetEventCallback(AnimEventCallback, this);
	}
	else
	{
		pSlot->ReleaseObjects();
		pSlot->SetRenderFlag(false);
	}
	pSlot->UpdateRenderNode();
	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return (pCharacter) ? nSlot : -1;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadCharacter(int nSlot, const char* sFilename, int nLoadFlags)
{
	if (!sFilename || (sFilename[0] == 0))
	{
		EntityWarning("[RenderProxy::LoadCharacter] Called with empty filename, Entity: %s", GetEntity()->GetEntityTextDescription().c_str());
		return -1;
	}

	//check the file extension to use the correct function: MakeCharacter for cgf/chr, LoadCharacterDefinition for cdf, AttachParts for cpt
	ICharacterInstance* pCharacter = gEnv->pCharacterManager->CreateInstance(sFilename);

	return SetSlotCharacter(nSlot, pCharacter);
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::SetParticleEmitter(int nSlot, IParticleEmitter* pEmitter, bool bSerialize)
{
	assert(pEmitter);

	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	if (pSlot->GetRenderNode() != pEmitter)
	{
		pEmitter->AddRef();
		pSlot->SetRenderNode(pEmitter);
	}

	pSlot->SetNeedSerialize(bSerialize);
	pSlot->SetRenderFlag(true);
	pEmitter->SetEntity(GetEntity(), nSlot);

	GetEntity()->SetInternalFlag(CEntity::EInternalFlag::HasParticles, true);

	pSlot->UpdateRenderNode();
	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}

int CEntityRender::LoadParticleEmitter(int nSlot, IParticleEffect* pEffect, SpawnParams const* pParams, bool bPrime, bool bSerialize)
{
	assert(pEffect);

	// Spawn/attach params.
	SpawnParams params;
	if (pParams)
	{
		params = *pParams;
	}
	else
	{
		params.eAttachType = pEffect->GetParticleParams().eAttachType;
		params.eAttachForm = pEffect->GetParticleParams().eAttachForm;
	}
	if (bPrime)
		params.bPrime = bPrime;

	Matrix34 const& TM = nSlot < 0 ? GetEntity()->GetWorldTM() : GetOrMakeSlot(nSlot)->GetWorldTM();

	// Create emitter, or update existing.
	IParticleEmitter* pEmitter = GetParticleEmitter(nSlot);
	if (!pEmitter || pEmitter->GetVersion() != pEffect->GetVersion())
	{
		if (pEmitter)
			pEmitter->Kill();
		pEmitter = pEffect->Spawn(TM, &params);
		if (!pEmitter)
			return -1;
		nSlot = SetParticleEmitter(nSlot, pEmitter, bSerialize);
	}
	else
	{
		pEmitter->SetMatrix(TM);
		pEmitter->SetEffect(pEffect);
		pEmitter->SetSpawnParams(params);
	}

	GetEntity()->SetInternalFlag(CEntity::EInternalFlag::HasParticles, true);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadLight(int nSlot, SRenderLight* pLight, uint16 layerId)
{
	assert(pLight);
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	pSlot->SetAsLight(*pLight, layerId);

	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadCloudBlocker(int nSlot, const SCloudBlockerProperties& properties)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);
	IRenderNode* pRenderNode = pSlot->GetRenderNode();
	if (!pRenderNode || pRenderNode->GetRenderNodeType() != eERType_CloudBlocker)
	{
		pRenderNode = GetI3DEngine()->CreateRenderNode(eERType_CloudBlocker);
		pSlot->SetRenderNode(pRenderNode);
	}
	pSlot->SetRenderFlag(true);

	CRY_ASSERT(pRenderNode && pRenderNode->GetRenderNodeType() == eERType_CloudBlocker);
	ICloudBlockerRenderNode* pCloudBlocker = static_cast<ICloudBlockerRenderNode*>(pRenderNode);

	pCloudBlocker->SetProperties(properties);

	// Update slot position
	pSlot->UpdateRenderNode();

	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadFogVolume(int nSlot, const SFogVolumeProperties& properties)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	IFogVolumeRenderNode* pFogVolume((IFogVolumeRenderNode*) pSlot->GetRenderNode());
	if (0 == pFogVolume)
	{
		pFogVolume = (IFogVolumeRenderNode*) GetI3DEngine()->CreateRenderNode(eERType_FogVolume);
		pSlot->SetRenderNode(pFogVolume);
	}
	pSlot->SetRenderFlag(true);

	assert(0 != pFogVolume);
	pFogVolume->SetFogVolumeProperties(properties);

	// Update slot position.
	pSlot->UpdateRenderNode();

	SetLocalBounds(AABB(-properties.m_size * 0.5f, properties.m_size * 0.5f), true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::FadeGlobalDensity(int nSlot, float fadeTime, float newGlobalDensity)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);
	IRenderNode* pRenderNode(pSlot->GetRenderNode());
	if (pRenderNode && pRenderNode->GetRenderNodeType() == eERType_FogVolume)
	{
		IFogVolumeRenderNode* pFogVolume((IFogVolumeRenderNode*)pRenderNode);
		pFogVolume->FadeGlobalDensity(fadeTime, newGlobalDensity);
	}
	return nSlot;
}

#if defined(USE_GEOM_CACHES)
//////////////////////////////////////////////////////////////////////////
int CEntityRender::LoadGeomCache(int nSlot, const char* sFilename)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	IGeomCacheRenderNode* pGeomCacheRenderNode = static_cast<IGeomCacheRenderNode*>(GetI3DEngine()->CreateRenderNode(eERType_GeomCache));
	pGeomCacheRenderNode->LoadGeomCache(sFilename);

	pSlot->SetRenderNode(pGeomCacheRenderNode);
	pSlot->SetRenderFlag(true);

	// Update slot position.
	pSlot->UpdateRenderNode();
	ComputeLocalBounds(true);

	SEntityEvent event(ENTITY_EVENT_SLOT_CHANGED);
	event.nParam[0] = nSlot;
	GetEntity()->SendEvent(event);

	return nSlot;
}
#endif

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CEntityRender::GetCharacter(int nSlot)
{
	nSlot = nSlot & (~ENTITY_SLOT_ACTUAL);
	if (!IsSlotValid(nSlot))
		return NULL;
	return Slot(nSlot)->GetCharacter();
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CEntityRender::GetStatObj(int nSlot)
{
	IStatObj* pCompObj;
	if (!(nSlot & ENTITY_SLOT_ACTUAL) && (pCompObj = GetCompoundObj()))
		return (unsigned int)pCompObj->GetSubObjectCount() > (unsigned int)nSlot ?
		       pCompObj->GetSubObject(nSlot)->pStatObj : 0;
	nSlot &= ~ENTITY_SLOT_ACTUAL;
	if (!IsSlotValid(nSlot))
		return NULL;
	return Slot(nSlot)->GetStatObj();
}

//////////////////////////////////////////////////////////////////////////
IParticleEmitter* CEntityRender::GetParticleEmitter(int nSlot)
{
	if (!IsSlotValid(nSlot))
		return NULL;

	return Slot(nSlot)->GetParticleEmitter();
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::SetSlotRenderNode(int nSlot, IRenderNode* pRenderNode)
{
	CEntitySlot* pSlot = GetOrMakeSlot(nSlot);

	pSlot->SetRenderNode(pRenderNode);
	pSlot->UpdateRenderNode();
	return nSlot;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CEntityRender::GetRenderMaterial(int nSlot) const
{
	CEntitySlot* pSlot = NULL;

	if (nSlot >= 0)
		pSlot = GetSlot(EntityPhysicsUtils::GetSlotIdx(nSlot));
	else
	{
		for (uint32 i = 0; i < m_slots.size(); i++)
		{
			if (m_slots[i] && (m_slots[i]->GetFlags() & ENTITY_SLOT_RENDER))
			{
				pSlot = m_slots[i];
				break;
			}
		}
	}

	if (!pSlot)
		return NULL;

	if (pSlot->GetMaterial())
		return pSlot->GetMaterial();

	if (GetEntity()->GetMaterial())
		return GetEntity()->GetMaterial();

	if (pSlot->GetStatObj())
		return pSlot->GetStatObj()->GetMaterial();

	if (pSlot->GetCharacter())
		return pSlot->GetCharacter()->GetIMaterial();

	if (pSlot->GetRenderNode())
		return pSlot->GetRenderNode()->GetMaterial(NULL);

	return NULL;
}

#define DEBUG_PARTICLES 0

//////////////////////////////////////////////////////////////////////////
bool CEntityRender::NeedNetworkSerialize()
{
	for (CEntitySlot* pSlot : m_slots)
	{
		if (pSlot && pSlot->NeedSerialize())
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::Serialize(TSerialize ser)
{
	// Particles.
	if (ser.GetSerializationTarget() != eST_Network)
	{
		if (ser.BeginOptionalGroup("RenderProxy", true))
		{
			if (ser.IsWriting())
			{
				//////////////////////////////////////////////////////////////////////////
				// Saving.
				//////////////////////////////////////////////////////////////////////////
				{

				}

				bool bParticles = false;
				for (auto pSlot : m_slots)
				{
					if (pSlot && pSlot->GetParticleEmitter())
					{
						bParticles = true;
						break;
					}
				}

				if (ser.BeginOptionalGroup("Particles", bParticles))
				{
					// Count serializable
					int nSerSlots = 0;
					for (uint32 i = 0; i < m_slots.size(); ++i)
					{
						IParticleEmitter* pEmitter = GetParticleEmitter(i);
						if (pEmitter && pEmitter->IsAlive())
						{
							if (m_slots[i]->NeedSerialize())
							{
								ser.BeginGroup("Slot");
								++nSerSlots;
								ser.Value("slot", i);
								gEnv->pParticleManager->SerializeEmitter(ser, pEmitter);
								ser.EndGroup();
							}
						}
					}
					ser.Value("count", nSerSlots);
					ser.EndGroup(); //Particles
				}
			}
			else
			{
				//////////////////////////////////////////////////////////////////////////
				// Loading.
				//////////////////////////////////////////////////////////////////////////
				{
				}

				if (ser.BeginOptionalGroup("Particles", true))
				{
					int nSerSlots = 0;
					ser.Value("count", nSerSlots);
					for (int i = 0; i < nSerSlots; ++i)
					{
						ser.BeginGroup("Slot");
						int nSlot = -1;
						ser.Value("slot", nSlot);
						if (nSlot >= 0)
						{
							if (IParticleEmitter* pEmitter = gEnv->pParticleManager->SerializeEmitter(ser, GetParticleEmitter(i)))
							{
								SetParticleEmitter(nSlot, pEmitter, true);
							}
						}
						ser.EndGroup();
					}
					ser.EndGroup();//Particles
				}
			}

			ser.EndGroup(); // RenderProxy
		}
	}

#if DEBUG_PARTICLES
	for (int i = 0; i < m_slots.size(); ++i)
	{
		IParticleEmitter* pEmitter = GetParticleEmitter(i);
		if (pEmitter && pEmitter->IsAlive())
			CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_COMMENT,
			           "Entity Particle: %s, slot %d, serialize %d: %s",
			           GetEntity()->GetName(), i, m_slots[i]->NeedSerialize(), pEmitter->GetDebugString('s').c_str());
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
int CEntityRender::GetSlotCount() const
{
	return m_slots.size();
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetRenderNodeParams(const IEntity::SRenderNodeParams& params)
{
	m_renderNodeParams = params;
	UpdateRenderNodes();
}

//////////////////////////////////////////////////////////////////////////
void CEntityRender::SetSubObjHideMask(int nSlot, hidemask nSubObjHideMask)
{
	if (IsSlotValid(nSlot))
	{
		CEntitySlot* pSlot = Slot(nSlot);
		pSlot->SetHidemask(nSubObjHideMask);
	}
}

//////////////////////////////////////////////////////////////////////////
hidemask CEntityRender::GetSubObjHideMask(int nSlot) const
{
	if (IsSlotValid(nSlot))
	{
		CEntitySlot* pSlot = Slot(nSlot);
		return pSlot->GetHidemask();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityRender::GetEntity() const
{
	const ptrdiff_t offsetFromEntity = Cry::Memory::GetMemberOffset(&CEntity::m_render);

	return reinterpret_cast<CEntity*>(reinterpret_cast<uintptr_t>(const_cast<CEntityRender*>(this)) - offsetFromEntity);
}
