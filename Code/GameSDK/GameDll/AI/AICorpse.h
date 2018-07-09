// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _AI_CORPSE_H_
#define _AI_CORPSE_H_

#include <IGameObject.h>
#include <CryCore/CryFlags.h>

#define AI_CORPSES_ENABLE_SERIALIZE 0

#if !defined(_RELEASE)
	#define AI_CORPSES_DEBUG_ENABLED 1
#else
	#define AI_CORPSES_DEBUG_ENABLED 0
#endif

struct IAttachment;
struct CEntityAttachment;

class CAICorpse : public CGameObjectExtensionHelper<CAICorpse, IGameObjectExtension>
{
	struct AttachedItem
	{
		enum
		{
			MaxWeapons = 2,
		};

		AttachedItem()
			: id(0) 
			, pClass(NULL)
		{

		}
		
		EntityId						id;
		IEntityClass*				pClass;
		string	            attachmentName;
	};

public:

	CAICorpse();
	virtual ~CAICorpse();

	// IGameObjectExtension
	virtual bool Init( IGameObject * pGameObject );
	virtual void InitClient( int channelId ) {};
	virtual void PostInit( IGameObject * pGameObject );
	virtual void PostInitClient( int channelId ) {};
	virtual bool ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params );
	virtual void PostReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params ) {}
	virtual bool GetEntityPoolSignature( TSerialize signature );
	virtual void Release();
	virtual void FullSerialize( TSerialize ser );
	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) { return false; };
	virtual void PostSerialize();
	virtual void SerializeSpawnInfo( TSerialize ser ) {}
	virtual ISerializableInfoPtr GetSpawnInfo() {return 0;}
	virtual void Update( SEntityUpdateContext& ctx, int slot ) {};
	virtual void HandleEvent( const SGameObjectEvent& gameObjectEvent );
	virtual void ProcessEvent( const SEntityEvent& entityEvent ) {};
	virtual uint64 GetEventMask() const { return 0; }
	virtual void SetChannelId( uint16 id ) {};
	virtual void PostUpdate( float frameTime ) { CRY_ASSERT(false); }
	virtual void PostRemoteSpawn() {};
	virtual void GetMemoryUsage( ICrySizer *pSizer ) const;
	// ~IGameObjectExtension

	void SetupFromSource( IEntity& sourceEntity, ICharacterInstance& characterInstance, const uint32 priority);
	void AboutToBeRemoved();

	uint32 GetPriority() const { return m_priority; }
private:

	EntityId CloneAttachedItem( const CAICorpse::AttachedItem& attachedItem, IAttachment* pAttachment);

#if AI_CORPSES_ENABLE_SERIALIZE
	string m_modelName;
#endif

	CryFixedArray<AttachedItem, AttachedItem::MaxWeapons> m_attachedItemsInfo;
	
	uint32 m_priority;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class CAICorpseManager
{
private:

	struct CorpseInfo
	{
		enum Flags
		{
			eFlag_PhysicsDisabled = BIT(0),
			eFlag_FarAway = BIT(1)
		};

		CorpseInfo( const EntityId _corpseId, const uint32 _priority )
			: corpseId(_corpseId)
		{
		}

		IEntity* GetCorpseEntity() 
		{
			return gEnv->pEntitySystem->GetEntity(corpseId);
		}

		CAICorpse* GetCorpse()
		{
			IGameObject* pGameObject = g_pGame->GetIGameFramework()->GetGameObject(corpseId);
			if(pGameObject != NULL)
			{
				return static_cast<CAICorpse*>(pGameObject->QueryExtension("AICorpse"));
			}

			return NULL;
		}

		EntityId   corpseId;
		CCryFlags<uint32>  flags;
	};

	typedef std::vector<CorpseInfo> TCorpseArray;

public:
	
	struct SCorpseParameters
	{
		enum Priority
		{
			ePriority_Normal = 0,
			ePriority_High   =  1,
			ePriority_VeryHight = 2,
		};

		Priority priority;
	};

	CAICorpseManager();
	~CAICorpseManager();
	
	void Reset();

	EntityId SpawnAICorpseFromEntity( IEntity& sourceEntity, const SCorpseParameters& corpseParams ); 

	void RegisterAICorpse( const EntityId corpseId, const uint32 priority );
	void UnregisterAICorpse( const EntityId corpseId );

	void Update( const float frameTime );

	void RemoveAllCorpses( const char* requester );

#if AI_CORPSES_DEBUG_ENABLED
	void DebugDraw( );
#else
	ILINE void DebugDraw() {}
#endif

	static CAICorpseManager* GetInstance() { return s_pThis; }

	static CAICorpseManager::SCorpseParameters::Priority GetPriorityForClass( const IEntityClass* pEntityClass );

private:

	void RemoveSomeCorpses();
	void RemoveCorpse( const EntityId corpseId );

	CorpseInfo* FindCorpseInfo( const EntityId corpseId );
	bool HasCorpseInfo( const EntityId corpseId ) const;

	TCorpseArray  m_corpsesArray;
	uint32				m_maxCorpses;
	uint32				m_lastUpdatedCorpseIdx;

	static CAICorpseManager* s_pThis;
};

#endif //_AI_CORPSE_H_