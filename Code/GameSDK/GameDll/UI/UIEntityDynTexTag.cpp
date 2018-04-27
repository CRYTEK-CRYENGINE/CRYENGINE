// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIEntityDynTexTag.cpp
//  Version:     v1.00
//  Created:     22/11/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "UIEntityDynTexTag.h"

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::InitEventSystem()
{
	if (!gEnv->pFlashUI)
		return;

	// event system to receive events from UI
	m_pUIOFct = gEnv->pFlashUI->CreateEventSystem( "UIEntityTagsDynTex", IUIEventSystem::eEST_UI_TO_SYSTEM );
	s_EventDispatcher.Init(m_pUIOFct, this, "UIEntityDynTexTag");

	{
		SUIEventDesc evtDesc( "AddEntityTag", "Adds a 3D entity Tag" );
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("EntityID", "Entity ID of tagged entity");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("uiElements_UIElement", "UIElement that is used for this tag (Instance with EntityId as instanceId will be created)");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("EntityClass", "EntityClass of the spawned entity");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("Material", "Material template that is used for the dyn texture");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Vec3>("Offset", "Offset in camera space relative to entity pos");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("TagIDX", "Custom IDX to identify entity tag.");
		s_EventDispatcher.RegisterEvent( evtDesc, &CUIEntityDynTexTag::OnAddTaggedEntity );
	}

	{
		SUIEventDesc evtDesc( "UpdateEntityTag", "Updates a 3D entity Tag" );
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("EntityID", "Entity ID of tagged entity");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("TagIDX", "Custom IDX to identify entity tag.");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Vec3>("Offset", "Offset in camera space relative to entity pos");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Float>("LerpSpeed", "Define speed of lerp between old and new offset, 0=instant");
	s_EventDispatcher.RegisterEvent( evtDesc, &CUIEntityDynTexTag::OnUpdateTaggedEntity );
	}

	{
		SUIEventDesc evtDesc( "RemoveEntityTag", "Removes a 3D entity Tag" );
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("EntityID", "Entity ID of tagged entity");
		evtDesc.AddParam<SUIParameterDesc::eUIPT_String>("TagIDX", "Custom IDX to identify entity tag.");
		s_EventDispatcher.RegisterEvent( evtDesc, &CUIEntityDynTexTag::OnRemoveTaggedEntity );
	}

	{
		SUIEventDesc evtDesc( "RemoveAllEntityTag", "Removes all 3D entity Tags for given entity" );
		evtDesc.AddParam<SUIParameterDesc::eUIPT_Int>("EntityID", "Entity ID of tagged entity");
		s_EventDispatcher.RegisterEvent( evtDesc, &CUIEntityDynTexTag::OnRemoveAllTaggedEntity );
	}

	gEnv->pFlashUI->RegisterModule(this, "CUIEntityDynTexTag");
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::UnloadEventSystem()
{
	ClearAllTags();

	if (gEnv->pFlashUI)
		gEnv->pFlashUI->UnregisterModule(this);
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnUpdate(float fDeltaTime)
{
	const CCamera& cam = GetISystem()->GetViewCamera();
	const Matrix34& camMat = cam.GetMatrix();

	static const Quat rot90Deg = Quat::CreateRotationXYZ( Ang3(gf_PI * 0.5f, 0, 0) );
	const Vec3 vSafeVec = camMat.GetColumn1();

	for (TTags::iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		IEntity* pOwner = gEnv->pEntitySystem->GetEntity(it->OwnerId);
		IEntity* pTagEntity = gEnv->pEntitySystem->GetEntity(it->TagEntityId);
		if (pOwner && pTagEntity)
		{
			const Vec3 offset = it->fLerp < 1 ? Vec3::CreateLerp(it->vOffset, it->vNewOffset, it->fLerp) : it->vOffset;
			const Vec3& vPos = pOwner->GetWorldPos();
			const Vec3 vFaceingPos = camMat.GetTranslation() - vSafeVec * 1000.f;
			const Vec3 vDir = (vPos - vFaceingPos).GetNormalizedSafe(vSafeVec);
			const Vec3 vOffsetX = vDir.Cross(Vec3Constants<float>::fVec3_OneZ).GetNormalized() * offset.x;
			const Vec3 vOffsetY = vDir * offset.y;
			const Vec3 vOffsetZ = Vec3(0, 0, offset.z);
			const Vec3 vOffset = vOffsetX + vOffsetY + vOffsetZ;


			const Vec3 vNewPos = vPos + vOffset;
			const Vec3 vNewDir = (vNewPos - vFaceingPos).GetNormalizedSafe(vSafeVec);

			const Quat qTagRot = Quat::CreateRotationVDir(vNewDir) * rot90Deg; // rotate 90 degrees around X-Axis
			pTagEntity->SetPos(vNewPos);
			pTagEntity->SetRotation(qTagRot);

			if (it->fLerp < 1)
			{
				assert(it->fSpeed > 0);
				it->fLerp += fDeltaTime * it->fSpeed;
				it->vOffset = offset;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::Reset()
{
	ClearAllTags();
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::Reload()
{
	if (gEnv->IsEditor())
	{
		ClearAllTags();
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnInstanceDestroyed( IUIElement* pSender, IUIElement* pDeletedInstance )
{
	for (TTags::iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		if (it->pInstance == pDeletedInstance)
			it->pInstance = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	assert(event.event == ENTITY_EVENT_DONE);
	RemoveAllEntityTags( pEntity->GetId(), false );
}


////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnAddTaggedEntity( EntityId entityId, const char* uiElementName, const char* entityClass, const char* materialTemplate, const Vec3& offset, const char* idx)
{
	OnRemoveTaggedEntity(entityId, idx);

	IEntityClass* pEntClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass( entityClass );
	if (pEntClass)
	{
		SEntitySpawnParams params;
		params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
		params.pClass = pEntClass;

		IEntity* pTagEntity = gEnv->pEntitySystem->SpawnEntity(params);
		IUIElement* pElement = gEnv->pFlashUI->GetUIElement(uiElementName);
		if (pTagEntity && pElement)
		{
			IMaterial* pMatTemplate = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialTemplate, false);

			// Hide the template (otherwise it will render somewhere - usually full screen if not assigned to an object)
			pElement->SetVisible(false);

			if (pMatTemplate && pMatTemplate->GetShaderItem().m_pShaderResources->GetTexture(EEfResTextures(0)))
			{
				pMatTemplate->GetShaderItem().m_pShaderResources->GetTexture(EEfResTextures(0))->m_Name.Format("%s@%d.ui", uiElementName, entityId);
				IMaterial* pMat = gEnv->p3DEngine->GetMaterialManager()->CloneMaterial(pMatTemplate);
				pTagEntity->SetMaterial(pMat);
			}
			pTagEntity->SetViewDistRatio(256);
			IUIElement* pElementInst = pElement->GetInstance((uint)entityId);
			pElementInst->RemoveEventListener(this); // first remove to avoid assert if already registered!
			pElementInst->AddEventListener(this, "CUIEntityDynTexTag");

			gEnv->pEntitySystem->AddEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
			m_Tags.push_back( STagInfo(entityId, pTagEntity->GetId(), idx, offset, pElementInst) );
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnUpdateTaggedEntity( EntityId entityId, const string& idx, const Vec3& offset, float speed )
{
	for (TTags::iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		if (it->OwnerId == entityId && it->Idx == idx)
		{
			it->fSpeed = speed;
			if (speed > 0)
			{
				it->fLerp = 0;
				it->vNewOffset = offset;
			}
			else
			{
				it->fLerp = 1;
				it->vOffset = offset;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnRemoveTaggedEntity( EntityId entityId, const string& idx )
{
	for (TTags::iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		if (it->OwnerId == entityId && it->Idx == idx)
		{
			gEnv->pEntitySystem->RemoveEntity(it->TagEntityId);
			if (it->pInstance)
				it->pInstance->DestroyThis();
			m_Tags.erase(it);
			break;
		}
	}
	if (!HasEntityTag(entityId))
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::OnRemoveAllTaggedEntity( EntityId entityId )
{
	RemoveAllEntityTags(entityId);
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::RemoveAllEntityTags( EntityId entityId, bool bUnregisterListener )
{
	for (TTags::iterator it = m_Tags.begin(); it != m_Tags.end();)
	{
		if (it->OwnerId == entityId)
		{
			gEnv->pEntitySystem->RemoveEntity(it->TagEntityId);
			if (it->pInstance)
				it->pInstance->DestroyThis();
			it = m_Tags.erase(it);
		}
		else
		{
			++it;
		}
	}
	if (bUnregisterListener)
		gEnv->pEntitySystem->RemoveEntityEventListener(entityId, ENTITY_EVENT_DONE, this);
}

////////////////////////////////////////////////////////////////////////////
void CUIEntityDynTexTag::ClearAllTags()
{
	for (TTags::const_iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		gEnv->pEntitySystem->RemoveEntity(it->TagEntityId);
		if (it->pInstance)
		{
			it->pInstance->RemoveEventListener(this);
			it->pInstance->DestroyThis();
		}
		gEnv->pEntitySystem->RemoveEntityEventListener(it->OwnerId, ENTITY_EVENT_DONE, this);
	}
	m_Tags.clear();
}

////////////////////////////////////////////////////////////////////////////
bool CUIEntityDynTexTag::HasEntityTag( EntityId entityId ) const
{
	for (TTags::const_iterator it = m_Tags.begin(); it != m_Tags.end(); ++it)
	{
		if (it->OwnerId == entityId)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////
REGISTER_UI_EVENTSYSTEM( CUIEntityDynTexTag );
