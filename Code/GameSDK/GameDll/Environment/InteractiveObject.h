// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Crysis2 interactive object, for playing co-operative animations with player

-------------------------------------------------------------------------
History:
- 10:12:2009: Created by Benito G.R.

*************************************************************************/

#pragma once

#ifndef __CRYSIS2_INTERACTIVE_OBJECT_H__
#define __CRYSIS2_INTERACTIVE_OBJECT_H__

#include <IGameObject.h>
#include "InteractiveObjectEnums.h"
#include "ItemString.h"

#include <ICryMannequinDefs.h>

#define INTERACTIVE_OBJECT_EX_ANIMATIONS_DEBUG	0

class CInteractiveObjectEx : public CGameObjectExtensionHelper<CInteractiveObjectEx, IGameObjectExtension>
{

private:

	enum EState
	{
		eState_NotUsed = 0,
		eState_InUse,
		eState_Used, //Has been used at least one time but can still be used
		eState_Done
	};

	struct SInteractionDataSet
	{
		SInteractionDataSet();
		~SInteractionDataSet(){};

		QuatT			m_alignmentHelperLocation;
		float			m_interactionRadius;
		float			m_interactionAngle;
		TagID			m_targetTagID;
		ItemString m_helperName;
	};

public:
	CInteractiveObjectEx();
	virtual ~CInteractiveObjectEx();

	// IGameObjectExtension
	virtual bool Init(IGameObject *pGameObject);
	virtual void InitClient(int channelId);
	virtual void PostInit(IGameObject *pGameObject);
	virtual void PostInitClient(int channelId);
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser );
	virtual ISerializableInfoPtr GetSpawnInfo();
	virtual void Update( SEntityUpdateContext &ctx, int updateSlot);
	virtual void PostUpdate(float frameTime );
	virtual void PostRemoteSpawn();
	virtual void HandleEvent( const SGameObjectEvent &goEvent);
	virtual void ProcessEvent(const SEntityEvent& entityEvent);
	virtual uint64 GetEventMask() const;
	virtual void SetChannelId(uint16 id);
	virtual void GetMemoryUsage(ICrySizer *pSizer) const;
	//~IGameObjectExtension

	//Script callbacks
	int CanUse(EntityId entityId) const;
	virtual void Use(EntityId entityId);	
	virtual void StopUse(EntityId entityId);
	virtual void AbortUse();
	//~Script callbacks

	void StartUseSpecific(EntityId entityId, int interactionIndex);
	void OnExploded(const Vec3& explosionSource);

private:

	bool Reset();

	void StartUse(EntityId entityId);
	void ForceInstantStandaloneUse(const int interactionIndex);
	void Done(EntityId entityId);
	void CalculateHelperLocation(const char* helperName, QuatT& helper) const;
	void Physicalize(pe_type physicsType, bool forcePhysicalization = false);
	void ParseAllInteractions(const SmartScriptTable& interactionProperties, std::vector<char*>& interactionNames); 
	void InitAllInteractionsFromMannequin();
	int  CalculateSelectedInteractionIndex( const EntityId entityId ) const;

	// returns -1 if not interaction constraints satsified, else returns index of first interaction the user can perform
	int  CanUserPerformAnyInteraction(EntityId userId) const;
	bool InteractionConstraintsSatisfied(const IEntity* pUserEntity, const SInteractionDataSet& interactionDataSet) const; 
	bool CanStillBeUsed();
	void HighlightObject(bool highlight);

	void ForceSkeletonUpdate( bool on );

#ifndef _RELEASE
	void DebugRender() const; 
#endif //#ifndef _RELEASE

protected:
	bool IsValidName(const char* name) const;

	uint32 GetCrcForName(const char* name) const;

private:
	typedef std::vector<SInteractionDataSet> SInteractionDataSetArray;
	SInteractionDataSetArray m_interactionDataSets;
	SInteractionDataSet m_loadedInteractionData;

	EState	m_state;
	int		m_physicalizationAfterAnimation;
	int		m_activeInteractionIndex; // Index into m_InteractionDataSets to indicate active interaction. 

	TagID m_interactionTagID;
	TagID m_stateTagID;

	uint32	m_currentLoadedCharacterCrc;
	bool		m_bHighlighted;
	bool		m_bRemoveDecalsOnUse;
	bool		m_bStartInteractionOnExplosion;
};


class CDeployableBarrier : public CInteractiveObjectEx
{
};


#endif //__CRYSIS2_INTERACTIVE_OBJECT_H__
