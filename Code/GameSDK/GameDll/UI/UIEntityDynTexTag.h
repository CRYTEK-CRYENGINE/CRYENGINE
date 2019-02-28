// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UIEntityDynTexTag.h
//  Version:     v1.00
//  Created:     07/12/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UIEntityDynTexTag_H__
#define __UIEntityDynTexTag_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryEntitySystem/IEntitySystem.h>

class CUIEntityDynTexTag 
	: public IUIGameEventSystem
	, public IUIModule
	, public IUIElementEventListener
	, public IEntityEventListener
{
public:
	// IUIGameEventSystem
	UIEVENTSYSTEM( "UIEntityDynTexTag" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;
	virtual void OnUpdate(float fDeltaTime) override;

	// IUIModule
	virtual void Reset() override;
	virtual void Reload() override;
	// ~IUIModule

	// IUIElementEventListener
	virtual void OnInstanceDestroyed( IUIElement* pSender, IUIElement* pDeletedInstance ) override;
	// ~IUIElementEventListener

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event ) override;
	// ~IEntityEventListener

private:
	void OnAddTaggedEntity( EntityId entityId, const char* uiElementName, const char* entityClass, const char* materialTemplate, const Vec3& offset, const char* idx);
	void OnUpdateTaggedEntity( EntityId entityId, const string& idx, const Vec3& offset, float speed );
	void OnRemoveTaggedEntity( EntityId entityId, const string& idx );
	void OnRemoveAllTaggedEntity( EntityId entityId );

	void RemoveAllEntityTags( EntityId entityId, bool bUnregisterListener = true );
	void ClearAllTags();
	inline bool HasEntityTag( EntityId entityId ) const;

private:
	SUIEventReceiverDispatcher<CUIEntityDynTexTag> s_EventDispatcher;
	IUIEventSystem* m_pUIOFct;

	struct STagInfo
	{
		STagInfo(EntityId ownerId, EntityId tagEntityId, const string& idx, const Vec3& offset, IUIElement* pInst) : OwnerId(ownerId), TagEntityId(tagEntityId), Idx(idx), vOffset(offset), vNewOffset(offset), pInstance(pInst), fLerp(2), fSpeed(0) {}

		EntityId OwnerId;
		EntityId TagEntityId;
		string Idx;
		Vec3 vOffset;
		Vec3 vNewOffset;
		IUIElement* pInstance;
		float fLerp;
		float fSpeed;
	};

	typedef std::vector< STagInfo > TTags;
	TTags m_Tags;
};

#endif // __UIEntityDynTexTag_H__

