// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Handles management for loading of entities

   -------------------------------------------------------------------------
   History:
   - 15:03:2010: Created by Kevin Kirst

*************************************************************************/

#include "stdafx.h"
#include "EntityLoadManager.h"
#include "EntitySystem.h"
#include "Entity.h"
#include "EntityLayer.h"

#include <CryNetwork/INetwork.h>

//////////////////////////////////////////////////////////////////////////
CEntityLoadManager::CEntityLoadManager(CEntitySystem* pEntitySystem)
	: m_pEntitySystem(pEntitySystem)
	, m_bSWLoading(false)
{
	assert(m_pEntitySystem);
}

//////////////////////////////////////////////////////////////////////////
CEntityLoadManager::~CEntityLoadManager()
{
	stl::free_container(m_queuedAttachments);
	stl::free_container(m_queuedFlowgraphs);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::Reset()
{
	m_clonedLayerIds.clear();
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::LoadEntities(XmlNodeRef& entitiesNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset, std::vector<IEntity*>* outGlobalEntityIds, std::vector<IEntity*>* outLocalEntityIds)
{
	bool bResult = false;
	m_bSWLoading = gEnv->p3DEngine->IsSegmentOperationInProgress();

	if (entitiesNode && ReserveEntityIds(entitiesNode))
	{
		PrepareBatchCreation(entitiesNode->getChildCount());

		bResult = ParseEntities(entitiesNode, bIsLoadingLevelFile, segmentOffset, outGlobalEntityIds, outLocalEntityIds);

		OnBatchCreationCompleted();
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::SetupHeldLayer(const char* pLayerName)
{
	// Look up the layer with this name.  Note that normally only layers that can be
	// dynamically loaded will be in here, but we have a hack in ObjectLayerManager to make
	// one of these for every layer.
	const IEntityLayer* pLayer = m_pEntitySystem->FindLayer(pLayerName);

	// Walk up the layer tree until we find the top level parent.  That's what we group held
	// entities by.
	while (pLayer != NULL)
	{
		const string& parentName = pLayer->GetParentName();

		if (parentName.empty())
			break;

		pLayer = m_pEntitySystem->FindLayer(parentName.c_str());
	}

	if (!gEnv->IsEditor())
		CRY_ASSERT_MESSAGE(pLayer != NULL, "All layers should be in the entity system, level may need to be reexported");

	if (pLayer != NULL)
	{
		int heldLayerIdx = -1;

		// Go through our held layers, looking for one with the parent name
		for (size_t i = 0; i < m_heldLayers.size(); ++i)
		{
			if (m_heldLayers[i].m_layerName == pLayer->GetName())
			{
				heldLayerIdx = i;
				break;
			}
		}

		// Crc the original layer name and add it to the map.  If the layer is held we
		// store the index, or -1 if it isn't held.
		uint32 layerCrc = CCrc32::Compute(pLayerName);
		m_layerNameMap[layerCrc] = heldLayerIdx;
	}
}

bool CEntityLoadManager::IsHeldLayer(XmlNodeRef& entityNode)
{
	if (m_heldLayers.empty())
		return false;

	const char* pLayerName = entityNode->getAttr("Layer");

	uint32 layerCrc = CCrc32::Compute(pLayerName);

	std::map<uint32, int>::iterator it = m_layerNameMap.find(layerCrc);

	// First time we've seen this layer, cache off info for it
	if (it == m_layerNameMap.end())
	{
		SetupHeldLayer(pLayerName);
		it = m_layerNameMap.find(layerCrc);

		if (it == m_layerNameMap.end())
			return false;
	}

	int heldLayerIdx = it->second;

	// If this is a held layer, cache off the creation info and return true (don't add)
	if (heldLayerIdx != -1)
	{
		m_heldLayers[heldLayerIdx].m_entities.push_back(entityNode);
		return true;
	}
	else
	{
		return false;
	}
}

void CEntityLoadManager::HoldLayerEntities(const char* pLayerName)
{
	m_heldLayers.resize(m_heldLayers.size() + 1);
	SHeldLayer& heldLayer = m_heldLayers[m_heldLayers.size() - 1];

	heldLayer.m_layerName = pLayerName;
}

void CEntityLoadManager::CloneHeldLayerEntities(const char* pLayerName, const Vec3& localOffset, const Matrix34& l2w, const char** pIncludeLayers, int numIncludeLayers)
{
	int heldLayerIdx = -1;

	// Get the index of the held layer
	for (size_t i = 0; i < m_heldLayers.size(); ++i)
	{
		if (m_heldLayers[i].m_layerName == pLayerName)
		{
			heldLayerIdx = i;
			break;
		}
	}

	if (heldLayerIdx == -1)
	{
		CRY_ASSERT_MESSAGE(0, "Trying to add a layer that wasn't held");
		return;
	}

	// Allocate a map for storing the mapping from the original entity ids to the cloned ones
	int cloneIdx = (int)m_clonedLayerIds.size();
	m_clonedLayerIds.resize(cloneIdx + 1);
	TClonedIds& clonedIds = m_clonedLayerIds[cloneIdx];

	// Get all the held entities for this layer
	std::vector<XmlNodeRef>& entities = m_heldLayers[heldLayerIdx].m_entities;

	const int nSize = entities.size();
	for (int i = 0; i < nSize; i++)
	{
		XmlNodeRef entityNode = entities[i];

		const char* pLayerName = entityNode->getAttr("Layer");

		bool isIncluded = (numIncludeLayers > 0) ? false : true;

		// Check if this layer is in our include list
		for (int j = 0; j < numIncludeLayers; ++j)
		{
			if (strcmp(pLayerName, pIncludeLayers[j]) == 0)
			{
				isIncluded = true;
				break;
			}
		}

		if (!isIncluded)
			continue;

		//////////////////////////////////////////////////////////////////////////
		//
		// Copy/paste from CEntityLoadManager::ParseEntities
		//
		INDENT_LOG_DURING_SCOPE(true, "Parsing entity '%s'", entityNode->getAttr("Name"));

		bool bSuccess = false;
		SEntityLoadParams loadParams;
		if (ExtractEntityLoadParams(entityNode, loadParams, Vec3(0, 0, 0), true))
		{
			// ONLY REAL CHANGES ////////////////////////////////////////////////////
			Matrix34 local(Matrix33(loadParams.spawnParams.qRotation));
			local.SetTranslation(loadParams.spawnParams.vPosition - localOffset);
			Matrix34 world = l2w * local;

			// If this entity has a parent, keep the transform local
			EntityId parentId;
			if (entityNode->getAttr("ParentId", parentId))
			{
				local.SetTranslation(loadParams.spawnParams.vPosition);
				world = local;
			}

			loadParams.spawnParams.vPosition = world.GetTranslation();
			loadParams.spawnParams.qRotation = Quat(world);

			EntityId origId = loadParams.spawnParams.id;
			loadParams.spawnParams.id = m_pEntitySystem->GenerateEntityId(true);

			loadParams.clonedLayerId = cloneIdx;

			//////////////////////////////////////////////////////////////////////////

			// Default to just creating the entity
			if (!bSuccess)
			{
				EntityId usingId = 0;

				// if we just want to reload this entity's properties
				if (entityNode->haveAttr("ReloadProperties"))
				{
					EntityId id;

					entityNode->getAttr("EntityId", id);
					loadParams.pReuseEntity = m_pEntitySystem->GetEntityFromID(id);
				}

				bSuccess = CreateEntity(loadParams, usingId, true);
			}

			if (!bSuccess)
			{
				string sName = entityNode->getAttr("Name");
				EntityWarning("CEntityLoadManager::ParseEntities : Failed when parsing entity \'%s\'", sName.empty() ? "Unknown" : sName.c_str());
			}
			else
			{
				// If we successfully cloned the entity, save the mapping from original id to cloned
				clonedIds[origId] = loadParams.spawnParams.id;
				m_clonedEntitiesTemp.push_back(loadParams.spawnParams.id);
			}
		}
		//
		// End copy/paste
		//
		//////////////////////////////////////////////////////////////////////////
	}

	// All attachment parent ids will be for the source copy, so we need to remap that id
	// to the cloned one.
	TQueuedAttachments::iterator itQueuedAttachment = m_queuedAttachments.begin();
	TQueuedAttachments::iterator itQueuedAttachmentEnd = m_queuedAttachments.end();
	for (; itQueuedAttachment != itQueuedAttachmentEnd; ++itQueuedAttachment)
	{
		SEntityAttachment& entityAttachment = *itQueuedAttachment;
		entityAttachment.parent = m_pEntitySystem->GetClonedEntityId(entityAttachment.parent, entityAttachment.child);
	}

	// Now that all the cloned ids are ready, go though all the entities we just cloned
	// and update the ids for any entity links.
	for (std::vector<EntityId>::iterator it = m_clonedEntitiesTemp.begin(); it != m_clonedEntitiesTemp.end(); ++it)
	{
		IEntity* pEntity = m_pEntitySystem->GetEntity(*it);
		if (pEntity != NULL)
		{
			IEntityLink* pLink = pEntity->GetEntityLinks();
			while (pLink != NULL)
			{
				pLink->entityId = m_pEntitySystem->GetClonedEntityId(pLink->entityId, *it);
				pLink = pLink->next;
			}
		}
	}
	m_clonedEntitiesTemp.clear();

	OnBatchCreationCompleted();
}

void CEntityLoadManager::ReleaseHeldEntities()
{
	m_heldLayers.clear();
	m_layerNameMap.clear();
}

//////////////////////////////////////////////////////////////////////////
//bool CEntityLoadManager::CreateEntities(TEntityLoadParamsContainer &container)
//{
//	bool bResult = container.empty();
//
//	if (!bResult)
//	{
//		PrepareBatchCreation(container.size());
//
//		bResult = true;
//		TEntityLoadParamsContainer::iterator itLoadParams = container.begin();
//		TEntityLoadParamsContainer::iterator itLoadParamsEnd = container.end();
//		for (; itLoadParams != itLoadParamsEnd; ++itLoadParams)
//		{
//			bResult &= CreateEntity(*itLoadParams);
//		}
//
//		OnBatchCreationCompleted();
//	}
//
//	return bResult;
//}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ReserveEntityIds(XmlNodeRef& entitiesNode)
{
	assert(entitiesNode);

	bool bResult = false;

	// Reserve the Ids to coop with dynamic entity spawning that may happen during this stage
	const int iChildCount = (entitiesNode ? entitiesNode->getChildCount() : 0);
	for (int i = 0; i < iChildCount; ++i)
	{
		XmlNodeRef entityNode = entitiesNode->getChild(i);
		if (entityNode && entityNode->isTag("Entity"))
		{
			EntityId entityId;
			EntityGUID guid;
			if (entityNode->getAttr("EntityId", entityId))
			{
				m_pEntitySystem->ReserveEntityId(entityId);
				bResult = true;
			}
			else if (entityNode->getAttr("EntityGuid", guid))
			{
				bResult = true;
			}
			else
			{
				// entity has no ID assigned
				bResult = true;
			}
		}
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::CanParseEntity(XmlNodeRef& entityNode, std::vector<IEntity*>* outGlobalEntityIds)
{
	assert(entityNode);

	bool bResult = true;
	if (!entityNode)
		return bResult;

	int nMinSpec = -1;
	if (entityNode->getAttr("MinSpec", nMinSpec) && nMinSpec > 0)
	{
		static ICVar* e_obj_quality(gEnv->pConsole->GetCVar("e_ObjQuality"));
		int obj_quality = (e_obj_quality ? e_obj_quality->GetIVal() : 0);

		// If the entity minimal spec is higher then the current server object quality this entity will not be loaded.
		bResult = (obj_quality >= nMinSpec || obj_quality == 0);
	}

	int globalInSW = 0;
	if (m_bSWLoading && outGlobalEntityIds && entityNode && entityNode->getAttr("GlobalInSW", globalInSW) && globalInSW)
	{
		EntityGUID guid;
		if (entityNode->getAttr("EntityGuid", guid))
		{
			EntityId id = gEnv->pEntitySystem->FindEntityByGuid(guid);
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
			{
#ifdef SEG_WORLD
				pEntity->SetLocalSeg(false);
#endif
				outGlobalEntityIds->push_back(pEntity);
			}
		}

		// In segmented world, global entity will not be loaded while streaming each segment
		bResult &= false;
	}

	if (bResult)
	{
		const char* pLayerName = entityNode->getAttr("Layer");
		IEntityLayer* pLayer = m_pEntitySystem->FindLayer(pLayerName);

		if (pLayer)
			bResult = !pLayer->IsSkippedBySpec();
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ParseEntities(XmlNodeRef& entitiesNode, bool bIsLoadingLevelFile, const Vec3& segmentOffset, std::vector<IEntity*>* outGlobalEntityIds, std::vector<IEntity*>* outLocalEntityIds)
{
#if !defined(SYS_ENV_AS_STRUCT)
	assert(gEnv);
	PREFAST_ASSUME(gEnv);
#endif

	assert(entitiesNode);

	bool bResult = true;

	const int iChildCount = entitiesNode->getChildCount();

	CryLog("Parsing %d entities...", iChildCount);
	INDENT_LOG_DURING_SCOPE();

	for (int i = 0; i < iChildCount; ++i)
	{
		//Update loading screen and important tick functions
		SYNCHRONOUS_LOADING_TICK();

		XmlNodeRef entityNode = entitiesNode->getChild(i);
		if (entityNode && entityNode->isTag("Entity") && CanParseEntity(entityNode, outGlobalEntityIds))
		{

			// Create entities only if they are not in an held layer and we are not in editor game mode.
			if (!IsHeldLayer(entityNode) && !gEnv->IsEditorGameMode())
			{
				INDENT_LOG_DURING_SCOPE(true, "Parsing entity '%s'", entityNode->getAttr("Name"));

				bool bSuccess = false;
				SEntityLoadParams loadParams;
				if (ExtractEntityLoadParams(entityNode, loadParams, segmentOffset, true))
				{
					// Default to just creating the entity
					if (!bSuccess)
					{
						EntityId usingId = 0;

						// if we just want to reload this entity's properties
						if (entityNode->haveAttr("ReloadProperties"))
						{
							EntityId id;

							entityNode->getAttr("EntityId", id);
							loadParams.pReuseEntity = m_pEntitySystem->GetEntityFromID(id);
						}

						bSuccess = CreateEntity(loadParams, usingId, bIsLoadingLevelFile);

						if (m_bSWLoading && outLocalEntityIds && usingId)
						{
							if (IEntity* pEntity = m_pEntitySystem->GetEntity(usingId))
							{
#ifdef SEG_WORLD
								pEntity->SetLocalSeg(true);
#endif
								outLocalEntityIds->push_back(pEntity);
							}

						}
					}
				}

				if (!bSuccess)
				{
					string sName = entityNode->getAttr("Name");
					EntityWarning("CEntityLoadManager::ParseEntities : Failed when parsing entity \'%s\'", sName.empty() ? "Unknown" : sName.c_str());
				}
				bResult &= bSuccess;
			}
		}

		if (0 == (i & 7))
		{
			gEnv->pNetwork->SyncWithGame(eNGS_FrameStart);
			gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
			gEnv->pNetwork->SyncWithGame(eNGS_WakeNetwork);
		}
	}

	return bResult;
}

bool CEntityLoadManager::ExtractCommonEntityLoadParams(XmlNodeRef& entityNode, SEntityLoadParams& outLoadParams, const Vec3& segmentOffset, bool bWarningMsg, const char* szEntityClass, const char* szEntityName) const
{
	assert(entityNode);

	bool bResult = true;

	IEntityClass* const pClass = m_pEntitySystem->GetClassRegistry()->FindClass(szEntityClass);
	if (pClass)
	{
		SEntitySpawnParams& spawnParams = outLoadParams.spawnParams;
		outLoadParams.spawnParams.entityNode = entityNode;

		// Load spawn parameters from xml node.
		spawnParams.pClass = pClass;
		spawnParams.sName = szEntityName;
		spawnParams.sLayerName = entityNode->getAttr("Layer");

		// Entities loaded from the xml cannot be fully deleted in single player.
		if (!gEnv->bMultiplayer)
		{
			spawnParams.nFlags |= ENTITY_FLAG_UNREMOVABLE;
		}

		Vec3 pos(Vec3Constants<float>::fVec3_Zero);
		Quat rot(Quat::CreateIdentity());
		Vec3 scale(Vec3Constants<float>::fVec3_One);

		entityNode->getAttr("Pos", pos);
		entityNode->getAttr("Rotate", rot);
		entityNode->getAttr("Scale", scale);

		spawnParams.vPosition = pos;
		spawnParams.qRotation = rot;
		spawnParams.vScale = scale;

		spawnParams.id = 0;
		if (!gEnv->pEntitySystem->EntitiesUseGUIDs())
		{
			entityNode->getAttr("EntityId", spawnParams.id);
		}
		entityNode->getAttr("EntityGuid", spawnParams.guid);

		ISegmentsManager* const pSegmentsManager = gEnv->p3DEngine->GetSegmentsManager();
		if (pSegmentsManager)
		{
			Vec2 coordInSW(Vec2Constants<float>::fVec2_Zero);
			if (entityNode->getAttr("CoordInSW", coordInSW))
			{
				pSegmentsManager->GlobalSegVecToLocalSegVec(pos, coordInSW, spawnParams.vPosition);
			}

			EntityGUID parentGuid;
			if (!entityNode->getAttr("ParentGuid", parentGuid))
			{
				spawnParams.vPosition += segmentOffset;
			}
		}

		// Get flags.
		bool bGoodOccluder = false; // false by default (do not change, it must be coordinated with editor export)
		bool bOutdoorOnly = false;
		bool bNoDecals = false;
		bool bDynamicDistanceShadows = false;
		int castShadowMinSpec = CONFIG_LOW_SPEC;

		entityNode->getAttr("CastShadowMinSpec", castShadowMinSpec);
		entityNode->getAttr("DynamicDistanceShadows", bDynamicDistanceShadows);
		entityNode->getAttr("GoodOccluder", bGoodOccluder);
		entityNode->getAttr("OutdoorOnly", bOutdoorOnly);
		entityNode->getAttr("NoDecals", bNoDecals);

		static const ICVar* const pObjShadowCastSpec = gEnv->pConsole->GetCVar("e_ObjShadowCastSpec");
		if (castShadowMinSpec <= pObjShadowCastSpec->GetIVal())
		{
			spawnParams.nFlags |= ENTITY_FLAG_CASTSHADOW;
		}

		if (bDynamicDistanceShadows)
		{
			spawnParams.nFlagsExtended |= ENTITY_FLAG_EXTENDED_DYNAMIC_DISTANCE_SHADOWS;
		}
		if (bGoodOccluder)
		{
			spawnParams.nFlags |= ENTITY_FLAG_GOOD_OCCLUDER;
		}
		if (bOutdoorOnly)
		{
			spawnParams.nFlags |= ENTITY_FLAG_OUTDOORONLY;
		}
		if (bNoDecals)
		{
			spawnParams.nFlags |= ENTITY_FLAG_NO_DECALNODE_DECALS;
		}

		const char* szArchetypeName = entityNode->getAttr("Archetype");
		if (szArchetypeName && szArchetypeName[0])
		{
			MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "%s", szArchetypeName);
			spawnParams.pArchetype = m_pEntitySystem->LoadEntityArchetype(szArchetypeName);

			if (!spawnParams.pArchetype)
			{
				EntityWarning("Archetype %s used by entity %s cannot be found! Entity cannot be loaded.", szArchetypeName, spawnParams.sName);
				bResult = false;
			}
		}
	}
	else  // No entity class found!
	{
		if (bWarningMsg)
		{
			EntityWarning("Entity class %s used by entity %s cannot be found! Entity cannot be loaded.", szEntityClass, szEntityName);
		}
		bResult = false;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntityLoadParams& outLoadParams, const Vec3& segmentOffset, bool bWarningMsg) const
{
	string entityClass = entityNode->getAttr("PrototypeLibrary") + string(".") + entityNode->getAttr("Name");
	size_t len = entityClass.size();
	for (; len > 0 && isdigit(entityClass[len - 1]); --len)
		;
	entityClass = entityClass.substr(0, len);

	const char* szEntityName = entityNode->getAttr("Name");
	return ExtractCommonEntityLoadParams(entityNode, outLoadParams, segmentOffset, bWarningMsg, entityClass.c_str(), szEntityName);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ExtractArcheTypeLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const
{
	SEntityLoadParams loadParams;
	const bool bRes = ExtractArcheTypeLoadParams(entityNode, loadParams, Vec3Constants<float>::fVec3_Zero, false);
	spawnParams = loadParams.spawnParams;
	return bRes;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntityLoadParams& outLoadParams, const Vec3& segmentOffset, bool bWarningMsg) const
{
	const char* szEntityClass = entityNode->getAttr("EntityClass");
	const char* szEntityName = entityNode->getAttr("Name");
	return ExtractCommonEntityLoadParams(entityNode, outLoadParams, segmentOffset, bWarningMsg, szEntityClass, szEntityName);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::ExtractEntityLoadParams(XmlNodeRef& entityNode, SEntitySpawnParams& spawnParams) const
{
	SEntityLoadParams loadParams;
	bool bRes = ExtractEntityLoadParams(entityNode, loadParams, Vec3(0, 0, 0), false);
	spawnParams = loadParams.spawnParams;
	return(bRes);
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::CreateEntity(XmlNodeRef& entityNode, SEntitySpawnParams& pParams, EntityId& outUsingId)
{
	SEntityLoadParams loadParams;
	loadParams.spawnParams = pParams;
	loadParams.spawnParams.entityNode = entityNode;

	if ((loadParams.spawnParams.id == 0) &&
	    ((loadParams.spawnParams.pClass->GetFlags() & ECLF_DO_NOT_SPAWN_AS_STATIC) == 0))
	{
		// If ID is not set we generate a static ID.
		loadParams.spawnParams.id = m_pEntitySystem->GenerateEntityId(true);
	}
	return(CreateEntity(loadParams, outUsingId, true));
}

//////////////////////////////////////////////////////////////////////////
bool CEntityLoadManager::CreateEntity(SEntityLoadParams& loadParams, EntityId& outUsingId, bool bIsLoadingLevellFile)
{
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Entity, 0, "Entity %s", loadParams.spawnParams.pClass->GetName());

	bool bResult = true;
	outUsingId = 0;

	XmlNodeRef& entityNode = loadParams.spawnParams.entityNode;
	SEntitySpawnParams& spawnParams = loadParams.spawnParams;

	uint32 entityGuid = 0;
	if (entityNode)
	{
		// Only runtime prefabs should have GUID id's
		const char* entityGuidStr = entityNode->getAttr("Id");
		if (entityGuidStr[0] != '\0')
		{
			entityGuid = CCrc32::ComputeLowercase(entityGuidStr);
		}
	}

	IEntity* pSpawnedEntity = NULL;
	bool bWasSpawned = false;
	if (loadParams.pReuseEntity)
	{
		// Attempt to reload
		pSpawnedEntity = (loadParams.pReuseEntity->ReloadEntity(loadParams) ? loadParams.pReuseEntity : NULL);
	}
	else if (m_pEntitySystem->OnBeforeSpawn(spawnParams))
	{
		// Create a new one
		pSpawnedEntity = m_pEntitySystem->SpawnEntity(spawnParams, false);
		bWasSpawned = true;
	}

	if (bResult && pSpawnedEntity)
	{
		m_pEntitySystem->AddEntityToLayer(spawnParams.sLayerName, pSpawnedEntity->GetId());

		CEntity* pCSpawnedEntity = (CEntity*)pSpawnedEntity;
		pCSpawnedEntity->SetLoadedFromLevelFile(bIsLoadingLevellFile);
		pCSpawnedEntity->SetCloneLayerId(loadParams.clonedLayerId);

		const char* szMtlName(NULL);

		if (spawnParams.pArchetype)
		{
			IScriptTable* pArchetypeProperties = spawnParams.pArchetype->GetProperties();
			if (pArchetypeProperties)
			{
				pArchetypeProperties->GetValue("PrototypeMaterial", szMtlName);
			}
		}

		if (entityNode)
		{
			// Create needed proxies
			if (entityNode->findChild("Area"))
			{
				pSpawnedEntity->CreateProxy(ENTITY_PROXY_AREA);
			}
			if (entityNode->findChild("Rope"))
			{
				pSpawnedEntity->CreateProxy(ENTITY_PROXY_ROPE);
			}
			if (entityNode->findChild("ClipVolume"))
			{
				pSpawnedEntity->CreateProxy(ENTITY_PROXY_CLIPVOLUME);
			}

			if (spawnParams.pClass)
			{
				const char* pClassName = spawnParams.pClass->GetName();
				if (pClassName && !strcmp(pClassName, "Light"))
				{
					IEntityRenderProxyPtr pRP = crycomponent_cast<IEntityRenderProxyPtr>(pSpawnedEntity->CreateProxy(ENTITY_PROXY_RENDER));
					if (pRP)
					{
						pRP->SerializeXML(entityNode, true);

						int nMinSpec = -1;
						if (entityNode->getAttr("MinSpec", nMinSpec) && nMinSpec >= 0)
							pRP->GetRenderNode()->SetMinSpec(nMinSpec);
					}
				}
			}

			// If we have an instance material, we use it...
			if (entityNode->haveAttr("Material"))
			{
				szMtlName = entityNode->getAttr("Material");
			}

			// Prepare the entity from Xml if it was just spawned
			if (pCSpawnedEntity && bWasSpawned)
			{
				if (IEntityPropertyHandler* pPropertyHandler = pCSpawnedEntity->GetClass()->GetPropertyHandler())
					pPropertyHandler->LoadEntityXMLProperties(pCSpawnedEntity, entityNode);

				if (IEntityEventHandler* pEventHandler = pCSpawnedEntity->GetClass()->GetEventHandler())
					pEventHandler->LoadEntityXMLEvents(pCSpawnedEntity, entityNode);

				// Serialize script proxy.
				CScriptProxy* pScriptProxy = pCSpawnedEntity->GetScriptProxy();
				if (pScriptProxy)
					pScriptProxy->SerializeXML(entityNode, true);
			}
		}

		// If any material has to be set...
		if (szMtlName && *szMtlName != 0)
		{
			// ... we load it...
			IMaterial* pMtl = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(szMtlName);
			if (pMtl)
			{
				// ... and set it...
				pSpawnedEntity->SetMaterial(pMtl);
			}
		}

		if (bWasSpawned)
		{
			const bool bInited = m_pEntitySystem->InitEntity(pSpawnedEntity, spawnParams);
			if (!bInited)
			{
				// Failed to initialise an entity, need to bail or we'll crash
				return true;
			}
		}
		else
		{
			m_pEntitySystem->OnEntityReused(pSpawnedEntity, spawnParams);

			if (pCSpawnedEntity && loadParams.bCallInit)
			{
				CScriptProxy* pScriptProxy = pCSpawnedEntity->GetScriptProxy();
				if (pScriptProxy)
					pScriptProxy->CallInitEvent(true);
			}
		}

		if (entityNode)
		{
			//////////////////////////////////////////////////////////////////////////
			// Load geom entity (Must be before serializing proxies.
			//////////////////////////////////////////////////////////////////////////
			if (spawnParams.pClass->GetFlags() & ECLF_DEFAULT)
			{
				// Check if it have geometry.
				const char* sGeom = entityNode->getAttr("Geometry");
				if (sGeom[0] != 0)
				{
					// check if character.
					const char* ext = PathUtil::GetExt(sGeom);
					if (stricmp(ext, CRY_SKEL_FILE_EXT) == 0 || stricmp(ext, CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 || stricmp(ext, CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
					{
						pSpawnedEntity->LoadCharacter(0, sGeom, IEntity::EF_AUTO_PHYSICALIZE);
					}
					else
					{
						pSpawnedEntity->LoadGeometry(0, sGeom, 0, IEntity::EF_AUTO_PHYSICALIZE);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

			// Serialize all entity proxies except Script proxy after initialization.
			if (pCSpawnedEntity)
			{
				CScriptProxy* pScriptProxy = pCSpawnedEntity->GetScriptProxy();

				pCSpawnedEntity->SerializeXML_ExceptScriptProxy(entityNode, true);
			}

			const char* attachmentType = entityNode->getAttr("AttachmentType");
			const char* attachmentTarget = entityNode->getAttr("AttachmentTarget");

			int flags = 0;
			if (strcmp(attachmentType, "GeomCacheNode") == 0)
			{
				flags |= IEntity::ATTACHMENT_GEOMCACHENODE;
			}
			else if (strcmp(attachmentType, "CharacterBone") == 0)
			{
				flags |= IEntity::ATTACHMENT_CHARACTERBONE;
			}

			// Add attachment to parent.
			if (m_pEntitySystem->EntitiesUseGUIDs())
			{
				EntityGUID nParentGuid = 0;
				if (entityNode->getAttr("ParentGuid", nParentGuid))
				{
					AddQueuedAttachment(0, nParentGuid, spawnParams.id, spawnParams.vPosition, spawnParams.qRotation, spawnParams.vScale, false, flags, attachmentTarget);
				}
			}
			else if (entityGuid == 0)
			{
				EntityId nParentId = 0;
				if (entityNode->getAttr("ParentId", nParentId))
				{
					AddQueuedAttachment(nParentId, 0, spawnParams.id, spawnParams.vPosition, spawnParams.qRotation, spawnParams.vScale, false, flags, attachmentTarget);
				}
			}
			else
			{
				const char* pParentGuid = entityNode->getAttr("Parent");
				if (pParentGuid[0] != '\0')
				{
					uint32 parentGuid = CCrc32::ComputeLowercase(pParentGuid);
					AddQueuedAttachment((EntityId)parentGuid, 0, spawnParams.id, spawnParams.vPosition, spawnParams.qRotation, spawnParams.vScale, true, flags, attachmentTarget);
				}
			}

			// check for a flow graph
			// only store them for later serialization as the FG proxy relies
			// on all EntityGUIDs already loaded
			if (entityNode->findChild("FlowGraph"))
			{
				AddQueuedFlowgraph(pSpawnedEntity, entityNode);
			}

			// Load entity links.
			XmlNodeRef linksNode = entityNode->findChild("EntityLinks");
			if (linksNode)
			{
				const int iChildCount = linksNode->getChildCount();
				for (int i = 0; i < iChildCount; ++i)
				{
					XmlNodeRef linkNode = linksNode->getChild(i);
					if (linkNode)
					{
						if (entityGuid == 0)
						{
							EntityId targetId = 0;
							EntityGUID targetGuid = 0;
							if (gEnv->pEntitySystem->EntitiesUseGUIDs())
								linkNode->getAttr("TargetGuid", targetGuid);
							else
								linkNode->getAttr("TargetId", targetId);

							const char* sLinkName = linkNode->getAttr("Name");
							Quat relRot(IDENTITY);
							Vec3 relPos(IDENTITY);

							pSpawnedEntity->AddEntityLink(sLinkName, targetId, targetGuid);
						}
						else
						{
							// If this is a runtime prefab we're spawning, queue the entity
							// link for later, since it has a guid target id we need to look up.
							AddQueuedEntityLink(pSpawnedEntity, linkNode);
						}
					}
				}
			}

			// Hide entity in game. Done after potential RenderProxy is created, so it catches the Hide
			if (bWasSpawned)
			{
				bool bHiddenInGame = false;
				entityNode->getAttr("HiddenInGame", bHiddenInGame);
				if (bHiddenInGame)
					pSpawnedEntity->Hide(true);
			}

			int nMinSpec = -1;
			if (entityNode->getAttr("MinSpec", nMinSpec) && nMinSpec >= 0)
			{
				if (IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pSpawnedEntity->GetProxy(ENTITY_PROXY_RENDER))
					pRenderProxy->GetRenderNode()->SetMinSpec(nMinSpec);
			}
		}
	}

	if (!bResult)
	{
		EntityWarning("[CEntityLoadManager::CreateEntity] Entity Load Failed: %s (%s)", spawnParams.sName, spawnParams.pClass->GetName());
	}

	outUsingId = (pSpawnedEntity ? pSpawnedEntity->GetId() : 0);

	if (outUsingId != 0 && entityGuid != 0)
	{
		m_guidToId[entityGuid] = outUsingId;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::PrepareBatchCreation(int nSize)
{
	m_queuedAttachments.clear();
	m_queuedFlowgraphs.clear();

	m_queuedAttachments.reserve(nSize);
	m_queuedFlowgraphs.reserve(nSize);
	m_guidToId.clear();
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::AddQueuedAttachment(EntityId nParent, EntityGUID nParentGuid, EntityId nChild, const Vec3& pos, const Quat& rot, const Vec3& scale, bool guid, const int flags, const char* target)
{
	SEntityAttachment entityAttachment;
	entityAttachment.child = nChild;
	entityAttachment.parent = nParent;
	entityAttachment.parentGuid = nParentGuid;
	entityAttachment.pos = pos;
	entityAttachment.rot = rot;
	entityAttachment.scale = scale;
	entityAttachment.guid = guid;
	entityAttachment.flags = flags;
	entityAttachment.target = target;

	m_queuedAttachments.push_back(entityAttachment);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::AddQueuedFlowgraph(IEntity* pEntity, XmlNodeRef& pNode)
{
	SQueuedFlowGraph f;
	f.pEntity = pEntity;
	f.pNode = pNode;

	m_queuedFlowgraphs.push_back(f);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::AddQueuedEntityLink(IEntity* pEntity, XmlNodeRef& pNode)
{
	SQueuedFlowGraph f;
	f.pEntity = pEntity;
	f.pNode = pNode;

	m_queuedEntityLinks.push_back(f);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::OnBatchCreationCompleted()
{
	// Load attachments
	TQueuedAttachments::iterator itQueuedAttachment = m_queuedAttachments.begin();
	TQueuedAttachments::iterator itQueuedAttachmentEnd = m_queuedAttachments.end();
	for (; itQueuedAttachment != itQueuedAttachmentEnd; ++itQueuedAttachment)
	{
		const SEntityAttachment& entityAttachment = *itQueuedAttachment;

		IEntity* pChild = m_pEntitySystem->GetEntity(entityAttachment.child);
		if (pChild)
		{
			EntityId parentId = entityAttachment.parent;
			if (m_pEntitySystem->EntitiesUseGUIDs())
				parentId = m_pEntitySystem->FindEntityByGuid(entityAttachment.parentGuid);
			else if (entityAttachment.guid)
				parentId = m_guidToId[(uint32)entityAttachment.parent];
			IEntity* pParent = m_pEntitySystem->GetEntity(parentId);
			if (pParent)
			{
				SChildAttachParams attachParams(entityAttachment.flags, entityAttachment.target.c_str());
				pParent->AttachChild(pChild, attachParams);
				pChild->SetLocalTM(Matrix34::Create(entityAttachment.scale, entityAttachment.rot, entityAttachment.pos));
			}
		}
	}
	m_queuedAttachments.clear();

	// Load flowgraphs
	TQueuedFlowgraphs::iterator itQueuedFlowgraph = m_queuedFlowgraphs.begin();
	TQueuedFlowgraphs::iterator itQueuedFlowgraphEnd = m_queuedFlowgraphs.end();
	for (; itQueuedFlowgraph != itQueuedFlowgraphEnd; ++itQueuedFlowgraph)
	{
		SQueuedFlowGraph& f = *itQueuedFlowgraph;

		if (f.pEntity)
		{
			IEntityProxyPtr pProxy = f.pEntity->CreateProxy(ENTITY_PROXY_FLOWGRAPH);
			if (pProxy)
				pProxy->SerializeXML(f.pNode, true);
		}
	}
	m_queuedFlowgraphs.clear();

	// Load entity links
	TQueuedFlowgraphs::iterator itQueuedEntityLink = m_queuedEntityLinks.begin();
	TQueuedFlowgraphs::iterator itQueuedEntityLinkEnd = m_queuedEntityLinks.end();
	for (; itQueuedEntityLink != itQueuedEntityLinkEnd; ++itQueuedEntityLink)
	{
		SQueuedFlowGraph& f = *itQueuedEntityLink;

		if (f.pEntity)
		{
			const char* targetGuidStr = f.pNode->getAttr("TargetId");
			if (targetGuidStr[0] != '\0')
			{
				EntityId targetId = FindEntityByEditorGuid(targetGuidStr);

				const char* sLinkName = f.pNode->getAttr("Name");
				Quat relRot(IDENTITY);
				Vec3 relPos(IDENTITY);

				f.pEntity->AddEntityLink(sLinkName, targetId, 0);
			}
		}
	}
	stl::free_container(m_queuedEntityLinks);
	stl::free_container(m_guidToId);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::ResolveLinks()
{
	if (!m_pEntitySystem->EntitiesUseGUIDs())
		return;

	IEntityItPtr pIt = m_pEntitySystem->GetEntityIterator();
	pIt->MoveFirst();
	while (IEntity* pEntity = pIt->Next())
	{
		IEntityLink* pLink = pEntity->GetEntityLinks();
		while (pLink)
		{
			if (pLink->entityId == 0)
				pLink->entityId = m_pEntitySystem->FindEntityByGuid(pLink->entityGuid);
			pLink = pLink->next;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
EntityId CEntityLoadManager::GetClonedId(int clonedLayerId, EntityId originalId)
{
	if (clonedLayerId >= 0 && clonedLayerId < (int)m_clonedLayerIds.size())
	{
		TClonedIds& clonedIds = m_clonedLayerIds[clonedLayerId];
		return clonedIds[originalId];
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////
EntityId CEntityLoadManager::FindEntityByEditorGuid(const char* pGuid) const
{
	uint32 guidCrc = CCrc32::ComputeLowercase(pGuid);
	TGuidToId::const_iterator it = m_guidToId.find(guidCrc);
	if (it != m_guidToId.end())
		return it->second;

	return INVALID_ENTITYID;
}
