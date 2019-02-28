// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description:
Handles player interaction...
* Sometimes prompts and responds to inputs
* Sometimes automatic
**************************************************************************/

#ifndef __PLAYERPLUGIN_INTERACTION_H__
#define __PLAYERPLUGIN_INTERACTION_H__

#include "PlayerPlugin.h"
#include "UI/UITypes.h"

struct SInteractionLookAt
{
	SInteractionLookAt()
		: lookAtTargetId(0)
		, interactorPreviousLockedId(0)
    , forceLookAt(false)
    , interpolationTime(1.f)
	{

	}

	void Reset()
	{
		lookAtTargetId = 0;
		interactorPreviousLockedId = 0;
		interpolationTime = 1.f;
    forceLookAt = false;
	}

	EntityId lookAtTargetId;
	EntityId interactorPreviousLockedId;
	float interpolationTime;
  bool forceLookAt;
};

struct SInteractionInfo
{
	SInteractionInfo()
		: interactionCustomMsg("")
		, interactiveEntityId(INVALID_ENTITYID)
		, swapEntityId(0)
		, interactionType(eInteraction_None)
    , displayTime(-1.0f)
	{

	}

	void Reset()
	{
		interactionCustomMsg = "";
		interactiveEntityId = INVALID_ENTITYID;
		interactionType = eInteraction_None;
		lookAtInfo.Reset();
		displayTime = -1.0f;
	}

	CryFixedStringT<128> interactionCustomMsg; // Custom msg to override interaction type msg (Currently used with property UseMessage)
	EntityId interactiveEntityId;
	EntityId swapEntityId;
	EInteractionType interactionType;

	SInteractionLookAt  lookAtInfo;
	
	float displayTime;
};

class CItem;
class CActor;
struct IEntity;
struct IScriptTable;
class SmartScriptTable;

class CPlayerPlugin_Interaction : public CPlayerPlugin
{
	public:
		SET_PLAYER_PLUGIN_NAME(CPlayerPlugin_Interaction);

		CPlayerPlugin_Interaction();

		void Reset();
		void SetLookAtTargetId(EntityId targetId);
		void SetLookAtInterpolationTime(float interpolationTime);
    void EnableForceLookAt();
    void DisableForceLookAt();

		ILINE const SInteractionInfo& GetCurrentInteractionInfo() const { return m_interactionInfo; }

	private:
		virtual void Enter(CPlayerPluginEventDistributor* pEventDist);
		virtual void Update(const float dt);
		virtual void HandleEvent(EPlayerPlugInEvent theEvent, void * data);
		virtual void UpdateAutoPickup();

		void UpdateInteractionInfo();
		EInteractionType GetInteractionTypeForEntity(EntityId entityId, IEntity* pEntity, IScriptTable *pEntityScript, const SmartScriptTable& propertiesTable,	 bool bHasPropertyTable, bool withinProximity);
		EInteractionType GetInteractionTypeWithItem(CItem* pItem);
		EInteractionType GetInteractionWithConstrainedBuddy(IEntity* pEntity, EntityId* pOutputBuddyEntityId, const bool withinProximity);
		bool CanAutoPickupAmmo(CItem* pItem) const;

		bool CanInteractGivenType(EntityId entity, EInteractionType type, bool withinProximity) const;
		bool CanGrabEnemy( const CActor& actor ) const;
		bool HasObjectGrabHelper(EntityId entityId) const;

		bool CanLocalPlayerSee(IEntity *pEntity) const;

		SInteractionInfo m_interactionInfo;
		
		IItemSystem*	m_pItemSystem;
		
		EntityId m_currentlyLookingAtEntityId;
		EntityId m_previousIgnoreEntityId; 
#if defined(_DEBUG)
		EntityId m_lastNearestEntityId;
#endif

		bool m_lastInventoryFull;
};

#endif // __PLAYERPLUGIN_INTERACTION_H__

