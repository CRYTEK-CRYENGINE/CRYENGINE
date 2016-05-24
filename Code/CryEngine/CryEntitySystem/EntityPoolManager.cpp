// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Handles a pool of entities and references to potential
        entities derived from level loading

   -------------------------------------------------------------------------
   History:
   - 15:03:2010: Created by Kevin Kirst

*************************************************************************/

#include "stdafx.h"
#include "EntityPoolManager.h"
#include "EntityPoolDefinition.h"
#include "EntityPool.h"

#include "EntityLoadManager.h"
#include "Entity.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActorProxy.h>

#include <CryGame/IGameFramework.h>
#include <CryNetwork/ISerializeHelper.h>

namespace
{
static char const* g_szPoolDefinitionsXml = "Scripts/Entities/EntityPoolDefinitions.xml";
static char const* g_szLevelDefinitionsXml = "EntityPools.xml";
static char const* g_szBookmarkLastState = "BookmarkLastState";
static char const* g_szReturnToPoolLua = "GetReturnToPoolWeight";
static char const* g_szBookmarkCreatedLua = "OnEntityBookmarkCreated";
};

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::SEntityBookmark::SEntityBookmark(bool bIsDynamicEntity, ISerializeHelper* pSerializeHelper)
	: pLastState()
	, inUseId(0)
	, savedPoolId(0)
	, owningPoolId(INVALID_ENTITY_POOL)
	, aiObjectId(INVALID_AIOBJECTID)
	, bIsDynamic(bIsDynamicEntity)
{
	assert(pSerializeHelper);
	pLastState = pSerializeHelper->CreateSerializedObject(g_szBookmarkLastState);

	assert(pLastState);
}

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::SEntityBookmark::SEntityBookmark(const SEntityLoadParams& _loadParams, bool bIsDynamicEntity, ISerializeHelper* pSerializeHelper)
	: loadParams(_loadParams)
	, sName(_loadParams.spawnParams.sName)
	, sLayerName(_loadParams.spawnParams.sLayerName)
	, pLastState()
	, inUseId(0)
	, savedPoolId(0)
	, owningPoolId(INVALID_ENTITY_POOL)
	, aiObjectId(INVALID_AIOBJECTID)
	, bIsDynamic(bIsDynamicEntity)
{
	assert(pSerializeHelper);
	pLastState = pSerializeHelper->CreateSerializedObject(g_szBookmarkLastState);

	assert(pLastState);
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SEntityBookmark::Serialize(TSerialize ser)
{
	assert(pLastState);
	if (pLastState)
	{
		pLastState->Serialize(ser);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SEntityPrepareRequest::Serialize(TSerialize ser)
{
	ser.BeginGroup("Request");
	{
		ser.Value("entityId", entityId);
		ser.Value("forcePoolId", forcePoolId);
		ser.Value("bCallInit", bCallInit);
		ser.Value("bIsDynamic", bIsDynamic);
	}
	ser.EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SEntityBookmark::PreSerialize(bool bIsReading)
{
	// The savedPoolId parameter will be updated on saving later, or read in on loading later
	savedPoolId = 0;

	if (bIsReading)
	{
		inUseId = 0;
		owningPoolId = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::SDynamicSerializeHelper::SDynamicSerializeHelper(const SEntityBookmark& bookmark)
	: sName(bookmark.sName)
	, sLayerName(bookmark.sLayerName)
	, sClass(bookmark.loadParams.spawnParams.pClass->GetName())
	, flags(bookmark.loadParams.spawnParams.nFlags)
	, guid(bookmark.loadParams.spawnParams.guid)
{
	if (bookmark.loadParams.spawnParams.pArchetype)
	{
		sArchetype = bookmark.loadParams.spawnParams.pArchetype->GetName();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SDynamicSerializeHelper::Serialize(TSerialize ser)
{
	ser.BeginGroup("DynamicInfo");
	{
		ser.Value("sName", sName);
		ser.Value("sLayerName", sLayerName);
		ser.Value("sClass", sClass);
		ser.Value("sArchetype", sArchetype);
		ser.Value("flags", flags);
		ser.Value("guid", guid);
	}
	ser.EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SDynamicSerializeHelper::InitLoadParams(SEntityLoadParams& outLoadParams, CEntitySystem* pEntitySystem) const
{
	assert(pEntitySystem);

	SEntitySpawnParams& outSpawnParams = outLoadParams.spawnParams;

	// Generate a new dynamic Id to use
	outSpawnParams.id = pEntitySystem->GenerateEntityId(false);

	outSpawnParams.guid = guid;
	outSpawnParams.nFlags = flags;
	outSpawnParams.sName = sName;
	outSpawnParams.sLayerName = sLayerName;

	// Get class
	outSpawnParams.pClass = pEntitySystem->GetClassRegistry()->FindClass(sClass.c_str());
	assert(outSpawnParams.pClass);

	// Get archetype
	if (!sArchetype.empty())
	{
		outSpawnParams.pArchetype = pEntitySystem->LoadEntityArchetype(sArchetype.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::CEntityPoolManager(CEntitySystem* pEntitySystem)
	: m_bEnabled(false)
	, m_pRootSID()
	, m_pEntitySystem(pEntitySystem)
	, m_pSerializeHelper()
	, m_uFramePrepareRequests(0)
	, m_preparingFromPool(false)
{
	assert(m_pEntitySystem);

	m_Listeners.resize(IEntityPoolListener::COUNT_SUBSCRIPTIONS, 1);

	for (size_t i = 0; i < IEntityPoolListener::COUNT_SUBSCRIPTIONS; ++i)
	{
		m_Listeners[i].Reserve(100);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::~CEntityPoolManager()
{
	ReleasePools();
	ReleaseDefinitions();
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddObject(m_pRootSID);
	pSizer->AddContainer(m_EntityPools);
	pSizer->AddContainer(m_PoolDefinitions);
	pSizer->AddContainer(m_PoolBookmarks);
	pSizer->AddContainer(m_Listeners);

	{
		SIZER_COMPONENT_NAME(pSizer, "Pools");
		TEntityPools::const_iterator itPool = m_EntityPools.begin();
		TEntityPools::const_iterator itPoolEnd = m_EntityPools.end();
		for (; itPool != itPoolEnd; ++itPool)
		{
			const CEntityPool* pPool = itPool->second;
			assert(pPool);

			pPool->GetMemoryStatistics(pSizer);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "Definitions");
		TPoolDefinitions::const_iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::const_iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; itDefinition != itDefinitionEnd; ++itDefinition)
		{
			const CEntityPoolDefinition* pDefinition = itDefinition->second;
			assert(pDefinition);

			pDefinition->GetMemoryStatistics(pSizer);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const CEntityPoolDefinition* CEntityPoolManager::GetEntityPoolDefinition(TEntityPoolDefinitionId id) const
{
	const CEntityPoolDefinition* pResult = NULL;

	TPoolDefinitions::const_iterator itFind = m_PoolDefinitions.find(id);
	if (itFind != m_PoolDefinitions.end())
		pResult = itFind->second;

	return pResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::Reset()
{
	ReleasePools();
	m_PoolBookmarks.clear();

	m_pRootSID = XmlNodeRef();

	stl::free_container(m_PrepareRequestQueue);

	m_uFramePrepareRequests = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::Update()
{
	if (m_uFramePrepareRequests == 0 && !m_PrepareRequestQueue.empty())
	{
		const SEntityPrepareRequest& request = m_PrepareRequestQueue.front();
		bool processed = ProcessPrepareRequest(request);

		if (processed)
			NotifyEntityPreparedFromPool(request.entityId);

		m_PrepareRequestQueue.pop();
	}

	m_uFramePrepareRequests = 0;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::Serialize(TSerialize ser)
{
	const bool bIsReading = ser.IsReading();

	// Prepare all bookmarks for saving
	{
		TPoolBookmarks::iterator itBookmark = m_PoolBookmarks.begin();
		TPoolBookmarks::iterator itBookmarkEnd = m_PoolBookmarks.end();
		for (; itBookmark != itBookmarkEnd; ++itBookmark)
		{
			SEntityBookmark& bookmark = itBookmark->second;
			bookmark.PreSerialize(bIsReading);
		}
	}

	if (bIsReading)
	{
		// Make all active entities inactive
		ResetPools(false);

		// Serialize bookmarks
		ser.BeginGroup("EntityPoolManager_Bookmarks");
		{
			const int iSerType = gEnv->pSystem->IsSerializingFile();
			const bool bIsQuickLoad = (iSerType == 1); // Set by CCryAction::LoadGame

			// Serialize the bookmarks
			int numBookmarksStatic = 0, numBookmarksDynamic = 0;
			ser.Value("numBookmarksStatic", numBookmarksStatic);
			ser.Value("numBookmarksDynamic", numBookmarksDynamic);
			CRY_ASSERT_MESSAGE(m_PoolBookmarks.size() == numBookmarksStatic, "Different number of Bookmark entries compared to saved Static count");

			const int numBookmarks = numBookmarksStatic + numBookmarksDynamic;
			for (int iBookmark = 0; iBookmark < numBookmarks; ++iBookmark)
			{
				SSerializeScopedBeginGroup bookmark_group(ser, "Bookmark");

				bool bIsDynamic = false;
				EntityId bookmarkId = 0;
				EntityId savedPoolId = 0;
				tAIObjectID aiObjectId = INVALID_AIOBJECTID;
				ser.Value("bookmarkId", bookmarkId);
				ser.Value("savedPoolId", savedPoolId);
				ser.Value("bIsDynamic", bIsDynamic);
				ser.Value("aiObjectId", aiObjectId);

				if (bIsDynamic)
				{
					SDynamicSerializeHelper helper;
					helper.Serialize(ser);

					// Reconstruct the spawn params for it
					SEntityLoadParams dynamicParams;
					helper.InitLoadParams(dynamicParams, m_pEntitySystem);

					// Add bookmark for the saved dynamic entity
					const bool bAdded = AddPoolBookmark(dynamicParams, true);
					CRY_ASSERT_MESSAGE(bAdded, "Failed to create Dynamic Bookmark entry from save");

					if (bAdded)
					{
						TPoolBookmarks::iterator itBookMark = m_PoolBookmarks.find(dynamicParams.spawnParams.id);
						assert(itBookMark != m_PoolBookmarks.end());

						SEntityBookmark& bookmark = itBookMark->second;
						bookmark.Serialize(ser);
						bookmark.aiObjectId = aiObjectId;

						// Reload if we were active before (have a saved pool id)
						if (savedPoolId)
						{
							PrepareDynamicFromPool(bookmark, bIsQuickLoad ? savedPoolId : 0, false);
						}
					}
				}
				else // Static bookmarks should always exist
				{
					bool bFound = false;
					TPoolBookmarks::iterator itBookmark = m_PoolBookmarks.begin();
					TPoolBookmarks::iterator itBookmarkEnd = m_PoolBookmarks.end();
					for (; itBookmark != itBookmarkEnd; ++itBookmark)
					{
						SEntityBookmark& bookmark = itBookmark->second;
						if (bookmarkId == bookmark.loadParams.spawnParams.id)
						{
							bookmark.Serialize(ser);
							bookmark.aiObjectId = aiObjectId;
							bFound = true;
							break;
						}
					}
					CRY_ASSERT_MESSAGE(bFound, "Missing Bookmark entry from save");

					// Reload if we were active before (have a saved pool id)
					if (bFound && savedPoolId)
					{
						PrepareFromPool(bookmarkId, bIsQuickLoad ? savedPoolId : 0, false, true);
					}
				}
			}
		}
		ser.EndGroup();

		if (ser.BeginOptionalGroup("m_PrepareRequestQueue", true))
		{
			while (!m_PrepareRequestQueue.empty())
				m_PrepareRequestQueue.pop();

			uint32 uSize = 0;
			ser.Value("uSize", uSize);

			for (uint32 i = 0; i < uSize; ++i)
			{
				SEntityPrepareRequest request;
				request.Serialize(ser);

				m_PrepareRequestQueue.push(request);
			}

			ser.EndGroup();
		}
	}
	else
	{
		// Update all active entity bookmarks in the pools before we serialize them out
		TEntityPools::iterator itPool = m_EntityPools.begin();
		TEntityPools::iterator itPoolEnd = m_EntityPools.end();
		for (; itPool != itPoolEnd; ++itPool)
		{
			CEntityPool* pPool = itPool->second;
			assert(pPool);

			pPool->UpdateActiveEntityBookmarks();
		}

		// Serialize bookmarks
		ser.BeginGroup("EntityPoolManager_Bookmarks");
		{
			int numBookmarksStatic = 0, numBookmarksDynamic = 0;

			TPoolBookmarks::iterator itBookmark = m_PoolBookmarks.begin();
			TPoolBookmarks::iterator itBookmarkEnd = m_PoolBookmarks.end();
			for (; itBookmark != itBookmarkEnd; ++itBookmark)
			{
				SEntityBookmark& bookmark = itBookmark->second;
				EntityId bookmarkId = bookmark.loadParams.spawnParams.id;

				ser.BeginGroup("Bookmark");
				{
					ser.Value("bookmarkId", bookmarkId);
					ser.Value("savedPoolId", bookmark.savedPoolId);
					ser.Value("bIsDynamic", bookmark.bIsDynamic);
					ser.Value("aiObjectId", bookmark.aiObjectId);

					if (bookmark.bIsDynamic)
					{
						SDynamicSerializeHelper helper(bookmark);
						helper.Serialize(ser);

						++numBookmarksDynamic;
					}
					else
					{
						++numBookmarksStatic;
					}

					bookmark.Serialize(ser);
				}
				ser.EndGroup();
			}

			ser.Value("numBookmarksStatic", numBookmarksStatic);
			ser.Value("numBookmarksDynamic", numBookmarksDynamic);
		}
		ser.EndGroup();

		if (ser.BeginOptionalGroup("m_PrepareRequestQueue", !m_PrepareRequestQueue.empty()))
		{
			TPrepareRequestQueue copyQueue = m_PrepareRequestQueue;

			uint32 uSize = copyQueue.size();
			ser.Value("uSize", uSize);

			while (!copyQueue.empty())
			{
				SEntityPrepareRequest& request = copyQueue.front();
				request.Serialize(ser);
				copyQueue.pop();
			}

			ser.EndGroup();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::OnLoadLevel(const char* szLevelPath)
{
	bool bResult = true;

	if (!gEnv->IsEditor() && IsUsingPools())
	{
		assert(gEnv->pSystem);
		const string sLevelPoolInfo = PathUtil::Make(szLevelPath, g_szLevelDefinitionsXml);

		if (gEnv->pCryPak->IsFileExist(sLevelPoolInfo.c_str()))
		{
			XmlNodeRef pLevelPoolInfo = gEnv->pSystem->LoadXmlFromFile(sLevelPoolInfo.c_str());

			if (pLevelPoolInfo)
			{
				TPoolDefinitions::iterator itDefinition = m_PoolDefinitions.begin();
				TPoolDefinitions::iterator itDefinitionEnd = m_PoolDefinitions.end();
				for (; itDefinition != itDefinitionEnd; ++itDefinition)
				{
					CEntityPoolDefinition* pDefinition = itDefinition->second;
					assert(pDefinition);

					XmlNodeRef pLevelPoolDefInfo = pLevelPoolInfo->findChild(pDefinition->GetName().c_str());
					if (pLevelPoolDefInfo)
					{
						pDefinition->ParseLevelXml(pLevelPoolDefInfo);
					}
				}
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::OnLevelLoadStart()
{
	// No bookmarks should exist by this point, as we're just starting to load a fresh level
	//	This will catch the case where we have retained info from a previous level on chainload
	assert(m_PoolBookmarks.empty());

	if (IsUsingPools())
	{
		// Prepare definitions for the loading level
		TPoolDefinitions::iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; itDefinition != itDefinitionEnd; ++itDefinition)
		{
			CEntityPoolDefinition* pDefinition = itDefinition->second;
			assert(pDefinition);

			pDefinition->OnLevelLoadStart();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::CreatePools()
{
	bool bResult = true;

	if (!gEnv->IsEditor() && IsUsingPools())
	{
		bResult = PreparePools();
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::OnPoolEntityInUse(CEntity* pEntity, EntityId prevId)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	TPoolBookmarks::const_iterator itBookMark = m_PoolBookmarks.find(pEntity->GetId());
	if (itBookMark != m_PoolBookmarks.end())
	{
		const SEntityBookmark& bookmark = itBookMark->second;
		const TEntityPoolId poolId = bookmark.owningPoolId;

		TEntityPools::iterator itPool = m_EntityPools.find(poolId);
		if (itPool != m_EntityPools.end())
		{
			CEntityPool* pPool = itPool->second;
			assert(pPool);

			pPool->OnPoolEntityInUse(pEntity, prevId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::LoadBookmarkedFromPool(EntityId entityId, EntityId forcedPoolId /*= 0*/, bool bCallInit /*= true*/)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	bool bResult = false;

	CEntityLoadManager* pEntityLoadManager = m_pEntitySystem->GetEntityLoadManager();
	assert(pEntityLoadManager);

	if (!m_pEntitySystem->HasEntity(entityId))
	{
		TPoolBookmarks::iterator itBookMark = m_PoolBookmarks.find(entityId);
		if (itBookMark != m_PoolBookmarks.end())
		{
			SEntityBookmark& bookmark = itBookMark->second;

			m_preparingFromPool = true;
			m_currentParams.aiObjectId = bookmark.aiObjectId;
			m_currentParams.entityId = bookmark.loadParams.spawnParams.id;

			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Entity, EMemStatContextFlags::MSF_Instance, "Prepare Static Bookmark %s", bookmark.loadParams.spawnParams.pClass->GetName());

			bool outIsActive = false;
			bool outHasAI = false;
			TEntityPoolId owningPoolId = INVALID_ENTITY_POOL;
			CEntity* pPoolEntity = GetPoolEntity(bookmark.loadParams.spawnParams.pClass, forcedPoolId, owningPoolId, outHasAI, outIsActive);
			if (pPoolEntity)
			{
				if (outIsActive)
				{
					// This occurs when the level designer has requested (via flowgraph) more entities
					//	 than we have space for. In this case an active entity is recycled instead.
					// Send return to pool event.
					SEntityEvent entevent;
					entevent.event = ENTITY_EVENT_RETURNING_TO_POOL;
					entevent.nParam[0] = entityId;
					pPoolEntity->SendEvent(entevent);

					// Notify listeners before entity is serialized, so they can do anything with it
					for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityReturningToPool)); notifier.IsValid(); notifier.Next())
					{
						notifier->OnEntityReturningToPool(entityId, pPoolEntity);
					}

					// First save the current entity into the bookmark, before we reuse it.
					// This ensures the state is correct if we reactivate it later.
					TPoolBookmarks::iterator activeEntityBookmarkIt = m_PoolBookmarks.find(pPoolEntity->GetId());
					if (activeEntityBookmarkIt != m_PoolBookmarks.end())
					{
						SEntityBookmark& activeEntityBookmark = activeEntityBookmarkIt->second;

						SerializeBookmarkEntity(activeEntityBookmark, pPoolEntity, false);
					}

					// Notify listeners if we're reloading an active AI
					for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityReturnedToPool)); notifier.IsValid(); notifier.Next())
					{
						notifier->OnEntityReturnedToPool(pPoolEntity->GetId(), pPoolEntity);
					}
				}

				bookmark.loadParams.pReuseEntity = pPoolEntity;
				bookmark.loadParams.bCallInit = bCallInit;
				bookmark.owningPoolId = owningPoolId;

				if (outHasAI)
					bookmark.loadParams.spawnParams.nFlags |= ENTITY_FLAG_HAS_AI;
				else
					bookmark.loadParams.spawnParams.nFlags &= ~ENTITY_FLAG_HAS_AI;

				bResult = pEntityLoadManager->CreateEntity(bookmark.loadParams, bookmark.inUseId, false);
				if (bResult)
				{
					// Load queued attachments
					SEntityBookmark::TQueuedAttachments::const_iterator itQueudAttachment = bookmark.queuedAttachments.begin();
					SEntityBookmark::TQueuedAttachments::const_iterator itQueudAttachmentEnd = bookmark.queuedAttachments.end();
					for (; itQueudAttachment != itQueudAttachmentEnd; ++itQueudAttachment)
					{
						const SEntityAttachment& entityAttachment = *itQueudAttachment;

						IEntity* pChild = m_pEntitySystem->GetEntity(entityAttachment.child);
						if (pChild)
						{
							SChildAttachParams attachParams(entityAttachment.flags, entityAttachment.target.c_str());
							pPoolEntity->AttachChild(pChild, attachParams);
							pChild->SetLocalTM(Matrix34::Create(entityAttachment.scale, entityAttachment.rot, entityAttachment.pos));
						}
					}

					// Load serialized state
					SerializeBookmarkEntity(bookmark, pPoolEntity, true);

					IAIObject* aiObject = pPoolEntity->GetAI();
					IAIActorProxy* aiActorProxy = aiObject ? aiObject->GetProxy() : 0;

					if (aiActorProxy)
					{
						if (!aiActorProxy->IsAutoDeactivated())
							pPoolEntity->Hide(!aiActorProxy->IsEnabled());
						else
						{
							pPoolEntity->Hide(false);
							aiObject->Event(AIEVENT_ENABLE, 0);
						}
					}
				}
			}
		}
	}

	m_preparingFromPool = false;
	m_currentParams.aiObjectId = INVALID_AIOBJECTID;
	m_currentParams.entityId = 0;

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityPoolManager::LoadDynamicFromPool(SEntityBookmark& bookmark, EntityId forcedPoolId /*= 0*/, bool bCallInit /*= true*/)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Entity, EMemStatContextFlags::MSF_Instance, "Prepare Dynamic Bookmark %s", bookmark.loadParams.spawnParams.pClass->GetName());

	CEntity* pReturnEntity = NULL;

	bool outIsActive = false;
	bool outHasAI = false;
	TEntityPoolId owningPoolId = INVALID_ENTITY_POOL;
	CEntity* pPoolEntity = GetPoolEntity(bookmark.loadParams.spawnParams.pClass, forcedPoolId, owningPoolId, outHasAI, outIsActive);
	if (pPoolEntity)
	{
		// If it was active, we need to drop the bookmark state for it
		if (outIsActive)
		{
			const EntityId prevEntityId = pPoolEntity->GetId();
			TPoolBookmarks::iterator itPrevBookMark = m_PoolBookmarks.find(prevEntityId);
			if (itPrevBookMark != m_PoolBookmarks.end())
			{
				SEntityBookmark& prevBookmark = itPrevBookMark->second;
				if (prevBookmark.bIsDynamic)
				{
					RemovePoolBookmark(prevEntityId);
				}
			}
		}

		bookmark.loadParams.pReuseEntity = pPoolEntity;
		bookmark.loadParams.bCallInit = bCallInit;
		bookmark.owningPoolId = owningPoolId;

		if (outHasAI)
			bookmark.loadParams.spawnParams.nFlags |= ENTITY_FLAG_HAS_AI;
		else
			bookmark.loadParams.spawnParams.nFlags &= ~ENTITY_FLAG_HAS_AI;

		// Attempt to reload
		pReturnEntity = (bookmark.loadParams.pReuseEntity->ReloadEntity(bookmark.loadParams) ? bookmark.loadParams.pReuseEntity : NULL);
		if (pReturnEntity)
		{
			m_pEntitySystem->OnEntityReused(pReturnEntity, bookmark.loadParams.spawnParams);

			if (bCallInit)
			{
				CScriptProxy* pScriptProxy = pReturnEntity->GetScriptProxy();
				if (pScriptProxy)
					pScriptProxy->CallInitEvent(true);
			}

			// Load serialized state
			SerializeBookmarkEntity(bookmark, pReturnEntity, true);

			IAIObject* aiObject = pReturnEntity->GetAI();
			IAIActorProxy* aiActorProxy = aiObject ? aiObject->GetProxy() : 0;

			if (aiActorProxy)
			{
				if (!aiActorProxy->IsAutoDeactivated())
					pReturnEntity->Hide(!aiActorProxy->IsEnabled());
				else
				{
					pReturnEntity->Hide(false);
					aiObject->Event(AIEVENT_ENABLE, 0);
				}
			}
		}
	}

	return pReturnEntity;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::PrepareFromPool(EntityId entityId, bool bPrepareNow)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	return PrepareFromPool(entityId, 0, true, bPrepareNow);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::PrepareFromPool(EntityId entityId, EntityId forcePoolId, bool bCallInit, bool bPrepareNow)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	bool bProcessed = false;

	if (!gEnv->IsEditor())
	{
		SEntityPrepareRequest request(entityId, forcePoolId, bCallInit, false);
		if (m_uFramePrepareRequests > 0 && !bPrepareNow)
		{
			// Queue the request for handling later
			m_PrepareRequestQueue.push(request);
		}
		else
		{
			// Process the request now, as none have been done this frame
			bProcessed = ProcessPrepareRequest(request);
		}

		++m_uFramePrepareRequests;
	}
	else if (IEntity* pEntity = m_pEntitySystem->GetEntity(entityId))
	{
		IEntityScriptProxy* pScriptProxy = (IEntityScriptProxy*)pEntity->GetProxy(ENTITY_PROXY_SCRIPT);
		if (pScriptProxy)
			pScriptProxy->CallEvent("Enable");

		bProcessed = true;
	}

	if (bProcessed)
	{
		NotifyEntityPreparedFromPool(entityId);
	}

	return true;
}

void CEntityPoolManager::NotifyEntityPreparedFromPool(EntityId entityId)
{
	// Notify listeners
	IEntity* pEntity = m_pEntitySystem->GetEntity(entityId);
	assert(pEntity);

	for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityPreparedFromPool)); notifier.IsValid(); notifier.Next())
	{
		notifier->OnEntityPreparedFromPool(entityId, pEntity);
	}
	CScriptProxy* pScriptProxy = (CScriptProxy*)(pEntity->GetProxy(ENTITY_PROXY_SCRIPT));
	if (pScriptProxy)
		pScriptProxy->OnPreparedFromPool();
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityPoolManager::PrepareDynamicFromPool(SEntitySpawnParams& params, bool bCallInit)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	CEntity* pReturnEntity = NULL;

	if (!gEnv->IsEditor() && IsUsingPools())
	{
		if (!m_pEntitySystem->HasEntity(params.id))
		{
			// Create bookmark entry for it if needed
			SEntityLoadParams loadParams;
			loadParams.spawnParams = params;
			AddPoolBookmark(loadParams, true);
		}

		TPoolBookmarks::iterator itBookMark = m_PoolBookmarks.find(params.id);
		if (itBookMark != m_PoolBookmarks.end())
		{
			SEntityBookmark& bookmark = itBookMark->second;
			pReturnEntity = PrepareDynamicFromPool(bookmark, 0, bCallInit);
		}
	}

	return pReturnEntity;
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityPoolManager::PrepareDynamicFromPool(SEntityBookmark& bookmark, EntityId forcePoolId, bool bCallInit)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	CEntity* pReturnEntity = LoadDynamicFromPool(bookmark, forcePoolId, bCallInit);

	// Notify listeners
	if (pReturnEntity)
	{
		NotifyEntityPreparedFromPool(pReturnEntity->GetId());
	}

	return pReturnEntity;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::ProcessPrepareRequest(const SEntityPrepareRequest& request)
{
	bool bProcessed = false;

	if (!request.bIsDynamic)
	{
		if (CEntityLoadManager* pEntityLoadManager = m_pEntitySystem->GetEntityLoadManager())
		{
			pEntityLoadManager->PrepareBatchCreation(1);
			bProcessed = LoadBookmarkedFromPool(request.entityId, request.forcePoolId, request.bCallInit);
			pEntityLoadManager->OnBatchCreationCompleted();
		}
	}
	else
	{
		// TODO Dynamic prepare requests. Currently the API expects the CEntity* to be returned on the same frame!
		CRY_ASSERT_MESSAGE(false, "CEntityPoolManager::ProcessPrepareRequest Dynamic requests are not yet handled");
	}

	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::ReturnToPool(EntityId entityId, bool bSaveState /*= true*/)
{
	return ReturnToPool(entityId, bSaveState, true);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::ReturnToPool(EntityId entityId, bool bSaveState, bool bCallInit)
{
	bool bResult = false;

	CEntity* pReturnEntity = m_pEntitySystem->GetEntityFromID(entityId);
	if (!gEnv->IsEditor() && pReturnEntity)
	{
		TPoolBookmarks::iterator itBookMark = m_PoolBookmarks.find(entityId);
		if (itBookMark != m_PoolBookmarks.end())
		{
			SEntityBookmark& bookmark = itBookMark->second;

			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Entity, EMemStatContextFlags::MSF_Instance, "Return Bookmark %s", bookmark.loadParams.spawnParams.pClass->GetName());

			TEntityPools::iterator itEntityPool = m_EntityPools.begin();
			TEntityPools::iterator itEntityPoolEnd = m_EntityPools.end();
			for (; itEntityPool != itEntityPoolEnd; ++itEntityPool)
			{
				CEntityPool* pPool = itEntityPool->second;
				assert(pPool);

				if (pPool && pPool->IsInActiveList(entityId))
				{
					// Send return to pool event.
					SEntityEvent entevent;
					entevent.event = ENTITY_EVENT_RETURNING_TO_POOL;
					entevent.nParam[0] = entityId;
					pReturnEntity->SendEvent(entevent);

					// Notify listeners before entity is serialized, so they can do anything with it
					for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityReturningToPool)); notifier.IsValid(); notifier.Next())
					{
						notifier->OnEntityReturningToPool(entityId, pReturnEntity);
					}

					// Serialize state for later use on static entities
					if (bSaveState && !bookmark.bIsDynamic)
					{
						SerializeBookmarkEntity(bookmark, pReturnEntity, false);
						bookmark.aiObjectId = pReturnEntity->GetAIObjectID();
					}

					// Notify listeners before entity is removed, so they can do anything with it
					for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityReturnedToPool)); notifier.IsValid(); notifier.Next())
					{
						notifier->OnEntityReturnedToPool(entityId, pReturnEntity);
					}

					CEntityLoadManager* pEntityLoadManager = m_pEntitySystem->GetEntityLoadManager();
					assert(pEntityLoadManager);

					EntityId poolId = pPool->GetPoolId(entityId);

					// Reload it back to a pool entity
					SEntityLoadParams params;
					pPool->CreateSpawnParams(params.spawnParams, poolId);
					params.pReuseEntity = pReturnEntity;
					params.bCallInit = bCallInit;
					if (pEntityLoadManager->CreateEntity(params, bookmark.inUseId, false))
					{
						pPool->OnPoolEntityReturned(pReturnEntity, entityId);
						bResult = true;
					}

					break;
				}
			}

			// Dynamic entities drop their bookmarks when returned
			if (bResult && bookmark.bIsDynamic)
			{
				RemovePoolBookmark(entityId);
			}
		}
	}
	else if (pReturnEntity)
	{
		// Notify listeners first so they can do anything with it
		for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::EntityReturnedToPool)); notifier.IsValid(); notifier.Next())
		{
			notifier->OnEntityReturnedToPool(entityId, pReturnEntity);
		}

		IEntityScriptProxy* pScriptProxy = (IEntityScriptProxy*)pReturnEntity->GetProxy(ENTITY_PROXY_SCRIPT);
		if (pScriptProxy)
			pScriptProxy->CallEvent("Disable");

		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::ResetBookmark(EntityId entityId)
{
	TPoolBookmarks::iterator itBookMark = m_PoolBookmarks.find(entityId);
	if (itBookMark != m_PoolBookmarks.end())
	{
		SEntityBookmark& entityBookmark = itBookMark->second;
		entityBookmark.pLastState->Reset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::SerializeBookmarkEntity(SEntityBookmark& bookmark, CEntity* pEntity, bool bIsLoading) const
{
	assert(pEntity);
	assert(bookmark.pLastState);
	assert(m_pSerializeHelper);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_Instance, "Serialize Bookmark %s", bookmark.loadParams.spawnParams.pClass->GetName());

	if (m_pSerializeHelper)
	{
		if (bIsLoading && !bookmark.pLastState->IsEmpty())
		{
			const bool bResult = m_pSerializeHelper->Read(bookmark.pLastState, CEntityPoolManager::OnBookmarkEntitySerialize, pEntity);
			if (!bResult)
			{
				EntityWarning("SerializeBookmarkEntity: Failed when reading last state from bookmark with entity \'%s\' (%u)", pEntity->GetName(), pEntity->GetId());
			}

			// We have to manually generate a post serialize event here for entities that
			// are transfered back and forth from the entity pool when waves are
			// enabled/disabled.
			SEntityEvent event(ENTITY_EVENT_POST_SERIALIZE);
			pEntity->SendEvent(event);
		}
		else if (!bIsLoading)
		{
			const bool bResult = m_pSerializeHelper->Write(bookmark.pLastState, CEntityPoolManager::OnBookmarkEntitySerialize, pEntity);
			if (!bResult)
			{
				EntityWarning("SerializeBookmarkEntity: Failed when writing last state from bookmark with entity \'%s\' (%u)", pEntity->GetName(), pEntity->GetId());
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::OnBookmarkEntitySerialize(TSerialize serialize, void* pVEntity)
{
	CEntity* pEntity = (CEntity*)pVEntity;
	assert(pEntity);

	serialize.BeginGroup("General");
	pEntity->Serialize(serialize, ENTITY_SERIALIZE_POSITION);
	pEntity->Serialize(serialize, ENTITY_SERIALIZE_ROTATION);
	pEntity->Serialize(serialize, ENTITY_SERIALIZE_SCALE);
	bool isHidden = pEntity->IsHidden();
	serialize.Value("Hidden", isHidden);
	if (serialize.IsReading())
	{
		pEntity->Hide(isHidden);
		pEntity->InvalidateTM(ENTITY_XFORM_POS | ENTITY_XFORM_ROT | ENTITY_XFORM_SCL);
	}
	serialize.EndGroup();

	serialize.BeginGroup("Properties");
	pEntity->Serialize(serialize, ENTITY_SERIALIZE_PROPERTIES);
	serialize.EndGroup();

	/*serialize.BeginGroup("Geometries");
	   pEntity->Serialize(serialize, ENTITY_SERIALIZE_GEOMETRIES);
	   serialize.EndGroup();*/

	serialize.BeginGroup("Proxies");
	pEntity->Serialize(serialize, ENTITY_SERIALIZE_PROXIES);
	serialize.EndGroup();

	// allow listeners to serialize relevant information into/out of the bookmark too
	CEntitySystem* pEntitySystem = (CEntitySystem*)gEnv->pEntitySystem;
	TListenerSet listeners = pEntitySystem->GetEntityPoolManager()->GetListenerSet(IEntityPoolListener::BookmarkEntitySerialize);
	for (TListeners::value_type::Notifier notifier(listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnBookmarkEntitySerialize(serialize, pVEntity);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::IsClassDefaultBookmarked(const char* szClassName) const
{
	assert(szClassName && szClassName[0]);

	bool bResult = false;

	IEntityClass* pClass = m_pEntitySystem->GetClassRegistry()->FindClass(szClassName);
	if (pClass)
	{
		TPoolDefinitions::const_iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::const_iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; itDefinition != itDefinitionEnd; ++itDefinition)
		{
			const CEntityPoolDefinition* pDefinition = itDefinition->second;
			assert(pDefinition);

			if (pDefinition->ContainsEntityClass(pClass))
			{
				bResult = pDefinition->IsDefaultBookmarked();
				break;
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::IsEntityBookmarked(EntityId entityId) const
{
	TPoolBookmarks::const_iterator itEntry = m_PoolBookmarks.find(entityId);
	return (itEntry != m_PoolBookmarks.end());
}

//////////////////////////////////////////////////////////////////////////
const char* CEntityPoolManager::GetBookmarkedClassName(EntityId entityId) const
{
	TPoolBookmarks::const_iterator itEntry = m_PoolBookmarks.find(entityId);
	if (itEntry != m_PoolBookmarks.end())
	{
		SEntitySpawnParams spawnParams = itEntry->second.loadParams.spawnParams;
		IEntityClass* pEntityClass = spawnParams.pClass;
		return pEntityClass->GetName();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
const char* CEntityPoolManager::GetBookmarkedEntityName(EntityId entityId) const
{
	TPoolBookmarks::const_iterator itEntry = m_PoolBookmarks.find(entityId);
	if (itEntry != m_PoolBookmarks.end())
		return itEntry->second.sName.c_str();

	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::IsClassUsingPools(IEntityClass* pClass) const
{
	assert(pClass);

	bool bResult = false;

	if (pClass)
	{
		TPoolDefinitions::const_iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::const_iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; !bResult && itDefinition != itDefinitionEnd; ++itDefinition)
		{
			const CEntityPoolDefinition* pDefinition = itDefinition->second;
			assert(pDefinition);

			bResult = pDefinition->ContainsEntityClass(pClass);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::IsClassForcedBookmarked(IEntityClass* pClass) const
{
	assert(pClass);

	bool bResult = false;

	if (pClass)
	{
		TPoolDefinitions::const_iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::const_iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; itDefinition != itDefinitionEnd; ++itDefinition)
		{
			const CEntityPoolDefinition* pDefinition = itDefinition->second;
			assert(pDefinition);

			if (pDefinition->ContainsEntityClass(pClass))
			{
				bResult = pDefinition->IsForcedBookmarked();
				break;
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::Enable(bool bEnable)
{
	m_bEnabled = bEnable;

	if (bEnable)
	{
		// (Re)load the definitions if enabled
		if (!LoadDefinitions())
		{
			EntityWarning("[Entity Pool] Failed to load Pool Definitions at \'%s\'", g_szPoolDefinitionsXml);
		}

		m_pSerializeHelper = gEnv->pGame->GetIGameFramework()->GetSerializeHelper();
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::ResetPools(bool bSaveState)
{
	TEntityPools::iterator itPoolReset = m_EntityPools.begin();
	TEntityPools::iterator itPoolResetEnd = m_EntityPools.end();
	for (; itPoolReset != itPoolResetEnd; ++itPoolReset)
	{
		CEntityPool* pPool = itPoolReset->second;
		assert(pPool);

		pPool->ResetPool(bSaveState);
	}
}

//////////////////////////////////////////////////////////////////////////
CEntityPoolManager::TListenerSet& CEntityPoolManager::GetListenerSet(IEntityPoolListener::ESubscriptions subscription)
{
	uint32 uIndex = 0;
	for (uint32 uSubscription = (uint32)subscription >> 1; uSubscription != 0; ++uIndex)
		uSubscription >>= 1;

	assert(uIndex < m_Listeners.size());
	return m_Listeners[uIndex];
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::AddListener(IEntityPoolListener* pListener, const char* szWho, uint32 uSubscriptions)
{
	assert(pListener);

	for (uint32 uSubscriptionIndex = 0; uSubscriptionIndex < IEntityPoolListener::COUNT_SUBSCRIPTIONS; ++uSubscriptionIndex)
	{
		if ((uSubscriptions & (1 << uSubscriptionIndex)))
		{
			m_Listeners[uSubscriptionIndex].Add(pListener, szWho);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::RemoveListener(IEntityPoolListener* pListener)
{
	assert(pListener);

	for (uint32 uSubscriptionIndex = 0; uSubscriptionIndex < IEntityPoolListener::COUNT_SUBSCRIPTIONS; ++uSubscriptionIndex)
	{
		m_Listeners[uSubscriptionIndex].Remove(pListener);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::IsUsingPools() const
{
	bool bResult = (CVar::es_EnablePoolUse != 0);
	bResult &= (m_bEnabled || CVar::es_EnablePoolUse == 1);

	if (bResult && gEnv->bMultiplayer)
	{
		CRY_ASSERT_MESSAGE(false, "Entity Pools enabled but this is a Multiplayer session. Disabling pools for now.");
		bResult = false;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
uint32 CEntityPoolManager::GetPoolSizeByClass(const char* szClassName) const
{
	uint32 uCount = 0;

	TEntityPools::const_iterator itPool = m_EntityPools.begin();
	TEntityPools::const_iterator itPoolEnd = m_EntityPools.end();
	for (; itPool != itPoolEnd; ++itPool)
	{
		const CEntityPool* pPool = itPool->second;
		assert(pPool);

		if (pPool && 0 == pPool->GetDefaultClass().compareNoCase(szClassName))
		{
			uCount = pPool->GetSize();
			break;
		}
	}

	return uCount;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::LoadDefinitions()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	bool bResult = false;

	m_PoolDefinitions.clear();
	//m_PoolSignatureLookup.clear();

	XmlNodeRef pRoot = gEnv->pSystem->LoadXmlFromFile(g_szPoolDefinitionsXml);
	if (pRoot && pRoot->isTag("EntityPoolDefinitions"))
	{
		bResult = true;

		TEntityPoolDefinitionId idGen = 0;

		size_t numAIObjects = 0;

		const int iChildCount = pRoot->getChildCount();
		for (int iChild = 0; bResult && iChild < iChildCount; ++iChild)
		{
			XmlNodeRef pDefinitionNode = pRoot->getChild(iChild);
			if (!pDefinitionNode || !pDefinitionNode->isTag("Definition"))
				continue;

			TEntityPoolDefinitionId newDefinitionId = ++idGen;
			CEntityPoolDefinition* pNewDefinition = new CEntityPoolDefinition(newDefinitionId);
			const bool bSuccess = pNewDefinition->LoadFromXml(m_pEntitySystem, pDefinitionNode);
			if (bSuccess)
			{
				m_PoolDefinitions[newDefinitionId] = pNewDefinition;

				if (pNewDefinition->HasAI())
					numAIObjects += pNewDefinition->GetMaxSize();
			}
			bResult &= bSuccess;
		}

		for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::PoolDefinitionsLoaded)); notifier.IsValid(); notifier.Next())
		{
			notifier->OnPoolDefinitionsLoaded(numAIObjects);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::ReleaseDefinitions()
{
	TPoolDefinitions::iterator itDefinition = m_PoolDefinitions.begin();
	TPoolDefinitions::iterator itDefinitionEnd = m_PoolDefinitions.end();
	for (; itDefinition != itDefinitionEnd; ++itDefinition)
	{
		CEntityPoolDefinition* pDefinition = itDefinition->second;
		SAFE_DELETE(pDefinition);
	}

	m_PoolDefinitions.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::AddPoolBookmark(SEntityLoadParams& loadParams, bool bIsDynamicEntity)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, EMemStatContextFlags::MSF_Instance, "Add Bookmark %s", loadParams.spawnParams.pClass->GetName());

	bool bResult = false;

	if (IsClassUsingPools(loadParams.spawnParams.pClass))
	{
		const EntityId& entityId = loadParams.spawnParams.id;
		TPoolBookmarks::iterator itEntry = m_PoolBookmarks.find(entityId);
		if (itEntry == m_PoolBookmarks.end())
		{
			// Lazily initialise the root node
			if (!m_pRootSID)
			{
				m_pRootSID = gEnv->pSystem->CreateXmlNode("RootSID", true);
			}

			// Bookmarks clone the entity node to avoid keeping a reference alive for the level Xml
			SEntityBookmark entityBookmark(loadParams, bIsDynamicEntity, m_pSerializeHelper);
			entityBookmark.loadParams.UseClonedEntityNode(loadParams.spawnParams.entityNode, m_pRootSID);

			TPoolBookmarks::value_type bookmarkPair(entityId, entityBookmark);
			m_PoolBookmarks.insert(bookmarkPair);

			// Register GUID to Id
			m_pEntitySystem->RegisterEntityGuid(loadParams.spawnParams.guid, loadParams.spawnParams.id);

			// Notify Lua for logic-driven helpers
			bool bAllowAutoCreate = false;
			IScriptSystem* pSS = gEnv->pScriptSystem;
			if (pSS && pSS->BeginCall(g_szBookmarkCreatedLua))
			{
				ScriptHandle hdl;
				hdl.n = loadParams.spawnParams.id;
				pSS->PushFuncParam(hdl);

				if (loadParams.spawnParams.entityNode)
				{
					XmlNodeRef pProperties = loadParams.spawnParams.entityNode->findChild("Properties2");
					if (pProperties)
					{
						SmartScriptTable propertiesTable(pSS);
						CreateTableFromXml(pProperties, propertiesTable);
						pSS->PushFuncParam(propertiesTable);
					}
				}

				pSS->EndCall(bAllowAutoCreate);
			}

			bResult = true;

			// Notify listeners
			for (TListeners::value_type::Notifier notifier(GetListenerSet(IEntityPoolListener::PoolBookmarkCreated)); notifier.IsValid(); notifier.Next())
			{
				notifier->OnPoolBookmarkCreated(entityId, loadParams.spawnParams, loadParams.spawnParams.entityNode);
			}

			// Check hidden in game property to auto-prepare if necessary
			if (bAllowAutoCreate && loadParams.spawnParams.entityNode)
			{
				bool bHiddenInGame = false;
				loadParams.spawnParams.entityNode->getAttr("HiddenInGame", bHiddenInGame);
				if (!bHiddenInGame)
				{
					//bResult = PrepareFromPool(entityId);
					bResult = LoadBookmarkedFromPool(entityId);
				}
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::RemovePoolBookmark(EntityId entityId)
{
	bool bResult = false;

	TPoolBookmarks::iterator itEntry = m_PoolBookmarks.find(entityId);
	if (itEntry != m_PoolBookmarks.end())
	{
		SEntityBookmark& entityBookmark = itEntry->second;

		// Unload GUID from Id
		m_pEntitySystem->UnregisterEntityGuid(entityBookmark.loadParams.spawnParams.guid);

		// Remove from map
		m_PoolBookmarks.erase(itEntry);
		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::UpdatePoolBookmark(EntityId entityId, EntityId poolId)
{
	assert(m_pEntitySystem);

	bool bResult = false;

	TPoolBookmarks::iterator itEntry = m_PoolBookmarks.find(entityId);
	if (itEntry != m_PoolBookmarks.end())
	{
		SEntityBookmark& entityBookmark = itEntry->second;

		CEntity* pEntity = m_pEntitySystem->GetEntityFromID(entityId);
		assert(pEntity);

		// Save the bookmark state if the entity flags allow for it
		if (pEntity && (pEntity->GetFlags() & ENTITY_FLAG_NO_SAVE) != ENTITY_FLAG_NO_SAVE)
		{
			entityBookmark.savedPoolId = poolId;

			SerializeBookmarkEntity(entityBookmark, pEntity, false);
			bResult = true;
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::AddAttachmentToBookmark(EntityId entityId, const SEntityAttachment& entityAttachment)
{
	bool bResult = false;

	TPoolBookmarks::iterator itEntry = m_PoolBookmarks.find(entityId);
	if (itEntry != m_PoolBookmarks.end())
	{
		SEntityBookmark& entityBookmark = itEntry->second;
		entityBookmark.queuedAttachments.push_back(entityAttachment);

		bResult = true;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
float CEntityPoolManager::GetReturnToPoolWeight(CEntity* pEntity, bool bIsUrgent) const
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	IScriptSystem* pSS = gEnv->pScriptSystem;
	assert(pSS);

	float fWeight = 0.0f;
	IScriptTable* pEntityTable = pEntity->GetScriptTable();
	if (pEntityTable && pSS->BeginCall(pEntityTable, g_szReturnToPoolLua))
	{
		pSS->PushFuncParam(pEntityTable);
		pSS->PushFuncParam(bIsUrgent);
		pSS->EndCall(fWeight);
	}

	return fWeight;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::PreparePools()
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	if (IsUsingPools())
	{
		INDENT_LOG_DURING_SCOPE(true, "Preparing entity pools...");

		assert(m_EntityPools.empty());

		TEntityPoolId idGen = 0;

		TPoolDefinitions::const_iterator itDefinition = m_PoolDefinitions.begin();
		TPoolDefinitions::const_iterator itDefinitionEnd = m_PoolDefinitions.end();
		for (; itDefinition != itDefinitionEnd; ++itDefinition)
		{
			const CEntityPoolDefinition* pDefinition = itDefinition->second;
			const uint32 uDesiredCount = pDefinition ? pDefinition->GetDesiredPoolCount() : 0;

			// Don't bother if we don't desire any for this one
			if (uDesiredCount == 0)
				continue;

			CryLog("Creating entity pool for '%s' (default class '%s') desiredCount=%u maxSize=%u", pDefinition->GetName().c_str(), pDefinition->GetDefaultClass().c_str(), uDesiredCount, pDefinition->GetMaxSize());
			INDENT_LOG_DURING_SCOPE();

			TEntityPoolId newPoolId = ++idGen;
			CEntityPool* pNewPool = new CEntityPool(this, newPoolId);
			bool bSuccess = pNewPool->CreatePool(*pDefinition);
			if (bSuccess)
			{
				bool bAddPool = true;
				const CEntityPoolSignature& mySignature = pNewPool->GetSignature();

				// Check for similar signature and merge if possible
				TEntityPools::iterator itPool = m_EntityPools.begin();
				TEntityPools::iterator itPoolEnd = m_EntityPools.end();
				for (; itPool != itPoolEnd; ++itPool)
				{
					CEntityPool* pOtherPool = itPool->second;
					assert(pOtherPool);

					const CEntityPoolSignature& otherSignature = pOtherPool->GetSignature();

					if (mySignature == otherSignature)
					{
						if (pOtherPool->ConsumePool(*pNewPool))
						{
							bAddPool = false;
						}
						else
						{
							CRY_ASSERT_MESSAGE(false, "Pool consumption failed on a matching signature!");
						}

						break;
					}
				}

				if (bAddPool)
				{
					m_EntityPools[newPoolId] = pNewPool;
				}
			}

			if (!bSuccess)
			{
				EntityWarning("[Entity Pool] Failed when creating the Pool  \'%s\'", pDefinition->GetName().c_str());
				continue;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::ReleasePools()
{
	TEntityPools::iterator itEntityPool = m_EntityPools.begin();
	TEntityPools::iterator itEntityPoolEnd = m_EntityPools.end();
	for (; itEntityPool != itEntityPoolEnd; ++itEntityPool)
	{
		CEntityPool* pPool = itEntityPool->second;
		SAFE_DELETE(pPool);
	}

	m_EntityPools.clear();
}

//////////////////////////////////////////////////////////////////////////
CEntity* CEntityPoolManager::GetPoolEntity(IEntityClass* pClass, EntityId forcedPoolId, TEntityPoolId& outOwningPoolId, bool& outHasAI, bool& outIsActive)
{
	FUNCTION_PROFILER(GetISystem(), PROFILE_ENTITY);

	assert(!gEnv->IsEditor());

	CEntity* pPoolEntity = NULL;
	outOwningPoolId = INVALID_ENTITY_POOL;
	outHasAI = false;
	outIsActive = false;

	TEntityPools::iterator itPool = m_EntityPools.begin();
	TEntityPools::iterator itPoolEnd = m_EntityPools.end();
	for (; itPool != itPoolEnd; ++itPool)
	{
		CEntityPool* pPool = itPool->second;
		assert(pPool);

		if (pPool->ContainsEntityClass(pClass))
		{
			pPoolEntity = pPool->GetEntityFromPool(outIsActive, forcedPoolId);
			outHasAI = pPool->HasAI();
			outOwningPoolId = (pPoolEntity ? pPool->GetId() : INVALID_ENTITY_POOL);
			break;
		}
	}

	if (!pPoolEntity)
	{
		CRY_ASSERT_MESSAGE(false, "Failed creating new entity for pool usage at run-time. About to return NULL.");
	}

	return pPoolEntity;
}

//////////////////////////////////////////////////////////////////////////
namespace
{
bool RecursiveParseTableFromXml(XmlNodeRef pXml, SmartScriptTable& outTable)
{
	assert(pXml);
	assert(outTable.GetPtr());

	bool bResult = (pXml && outTable.GetPtr());
	if (bResult)
	{
		// Do attributes first
		const int iAttrCount = pXml->getNumAttributes();
		for (int iAttr = 0; bResult && iAttr < iAttrCount; ++iAttr)
		{
			const char* key, * value;
			bResult &= (bResult && pXml->getAttributeByIndex(iAttr, &key, &value));
			if (bResult)
			{
				ScriptVarType varType = outTable->GetValueType(key);
				if (varType == svtBool || varType == svtNumber)
				{
					float fValue = 0.0f;
					if (stricmp(value, "true") == 0)
						fValue = 1.0f;
					else if (stricmp(value, "false") != 0)
						fValue = (float)atof(value);
					outTable->SetValue(key, fValue);
				}
				else
				{
					outTable->SetValue(key, value);
				}
			}
		}

		// For each child element, create a new table and load from it
		if (bResult)
		{
			const int iChildCount = pXml->getChildCount();
			for (int iChild = 0; bResult && iChild < iChildCount; ++iChild)
			{
				XmlNodeRef pChild = pXml->getChild(iChild);
				if (!pChild)
					continue;

				SmartScriptTable pChildTable(outTable->GetScriptSystem());
				bResult &= (bResult && RecursiveParseTableFromXml(pChild, pChildTable));
				if (bResult)
					outTable->SetValue(pChild->getTag(), pChildTable);
			}
		}
	}

	return bResult;
}
}

//////////////////////////////////////////////////////////////////////////
bool CEntityPoolManager::CreateTableFromXml(XmlNodeRef pXml, SmartScriptTable& outTable)
{
	assert(pXml);

	bool bResult = false;

	if (pXml)
	{
		// Clear table or create one if doesn't exist
		if (outTable.GetPtr())
		{
			outTable->Clear();
		}
		else
		{
			outTable.Create(gEnv->pScriptSystem);
			CRY_ASSERT(outTable.GetPtr());
		}

		if (outTable.GetPtr())
		{
			bResult = RecursiveParseTableFromXml(pXml, outTable);
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::DebugDraw() const
{
#ifdef ENTITY_POOL_DEBUGGING

	IRenderer* pRenderer = gEnv->pRenderer;
	assert(pRenderer);

	float fColumnX = 1.0f;
	float fColumnY = 11.0f;
	const float colWhite[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	pRenderer->Draw2dLabel(fColumnX, fColumnY, 1.3f, colWhite, false, "Total Pool Count: %" PRISIZE_T, m_EntityPools.size());
	fColumnY += 18.0f;

	pRenderer->Draw2dLabel(fColumnX, fColumnY, 1.3f, colWhite, false, "Bookmarked Entities: %" PRISIZE_T, m_PoolBookmarks.size());
	fColumnY += 18.0f;

	string sFilter = CVar::es_DebugPoolFilter;
	sFilter.MakeLower();

	TEntityPools::const_iterator itPool = m_EntityPools.begin();
	TEntityPools::const_iterator itPoolEnd = m_EntityPools.end();
	for (; itPool != itPoolEnd; ++itPool)
	{
		CEntityPool* pPool = itPool->second;
		assert(pPool);

		bool bDebugDraw = (sFilter.empty());
		if (!bDebugDraw)
		{
			string sName = pPool->GetName();
			sName.MakeLower();

			bDebugDraw = (sName.find(sFilter) != string::npos);
		}

		if (bDebugDraw)
			pPool->DebugDraw(pRenderer, fColumnX, fColumnY, !sFilter.empty());
	}

#endif //ENTITY_POOL_DEBUGGING
}

//////////////////////////////////////////////////////////////////////////
void CEntityPoolManager::DumpEntityBookmarks(const char* szFilterName, bool bWriteSIDToDisk) const
{
#ifdef ENTITY_POOL_DEBUGGING

	if (!(szFilterName && szFilterName[0]))
		CryLogAlways("Dumping all entity bookmarks...");
	else
		CryLogAlways("Dumping entity bookmarks that contain \'%s\'...", szFilterName);

	uint32 loggedCount = 0;

	{
		INDENT_LOG_DURING_SCOPE();

		for (TPoolBookmarks::const_iterator itBookmark = m_PoolBookmarks.begin(), itBookmarkEnd = m_PoolBookmarks.end();
		     itBookmark != itBookmarkEnd; ++itBookmark)
		{
			const SEntityBookmark& bookmark = itBookmark->second;
			const char* szName = bookmark.loadParams.spawnParams.sName;
			const char* szLayer = bookmark.loadParams.spawnParams.sLayerName;
			const char* szClass = (bookmark.loadParams.spawnParams.pClass ? bookmark.loadParams.spawnParams.pClass->GetName() : "Default");

			if (!(szFilterName && szFilterName[0] && szName && szName[0]) || 0 == stricmp(szName, szFilterName))
			{
				CryLogAlways("Name: %s", (szName && szName[0] ? szName : "_No Name_"));

				INDENT_LOG_DURING_SCOPE();

				CryLogAlways("Layer: %s", (szLayer && szLayer[0] ? szLayer : "_No Layer_"));
				CryLogAlways("Class: %s", szClass);
				CryLogAlways("Reserved Id: %u", itBookmark->first);
				CryLogAlways("Has \"No Save\" flag: %s", (bookmark.loadParams.spawnParams.nFlags & ENTITY_FLAG_NO_SAVE) ? "Yes" : "No");
				CryLogAlways("Static Entity: %s", bookmark.bIsDynamic ? "No" : "Yes");

				CryLogAlways("Has Serialized State: %s", bookmark.pLastState->IsEmpty() ? "No" : "Yes");
				if (false == bookmark.pLastState->IsEmpty())
				{
					INDENT_LOG_DURING_SCOPE();

					ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
					bookmark.pLastState->GetMemoryUsage(pSizer);
					CryLogAlways("Memory Footprint: %" PRISIZE_T, pSizer->GetTotalSize());
					pSizer->Release();
				}

				CryLogAlways("Has Static Instanced Data: %s", bookmark.loadParams.spawnParams.entityNode ? "Yes" : "No");

				if (bookmark.loadParams.spawnParams.entityNode)
				{
					INDENT_LOG_DURING_SCOPE();

					ICrySizer* pSizer = gEnv->pSystem->CreateSizer();
					bookmark.loadParams.spawnParams.entityNode->GetMemoryUsage(pSizer);

					CryLogAlways("Memory Footprint: %" PRISIZE_T, pSizer->GetTotalSize());
					pSizer->Release();

					if (bWriteSIDToDisk)
					{
						const char* szLevelName = gEnv->pGame->GetIGameFramework()->GetLevelName();

						stack_string sXmlFileName;
						sXmlFileName.Format("%s/Bookmarks/%s/%s.xml", gEnv->pCryPak->GetAlias("%USER%"), (szLevelName && szLevelName[0] ? szLevelName : "NoLevel"), szName);
						bookmark.loadParams.spawnParams.entityNode->saveToFile(sXmlFileName.c_str());
					}
				}

				++loggedCount;
			}
		}
	}

	CryLogAlways("Logged \'%u\' bookmarks.", loggedCount);

#endif //ENTITY_POOL_DEBUGGING
}

bool CEntityPoolManager::IsPreparingEntity(SPreparingParams& outParams) const
{
	if (m_preparingFromPool)
	{
		outParams = m_currentParams;
	}
	return m_preparingFromPool;
}
