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

	static ICVar* pAsyncLoad = gEnv->pConsole->GetCVar("g_asynclevelload");
	const bool bAsyncLoad = pAsyncLoad && pAsyncLoad->GetIVal() > 0;

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

			// Create entities only if we are not in editor game mode.
			if (!gEnv->IsEditorGameMode())
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

		if ((!bAsyncLoad) && (0 == (i & 7)))
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

	IEntityClass* pClass = nullptr;

	CryGUID classGUID;
	if (entityNode->getAttr("ClassGUID", classGUID))
	{
		// Class GUID exist, so use it instead of the class name
		pClass = m_pEntitySystem->GetClassRegistry()->FindClassByGUID(classGUID);
	}

	if (!pClass)
	{
		// (Legacy) If class not found by GUID try to find it by class name
		pClass = m_pEntitySystem->GetClassRegistry()->FindClass(szEntityClass);
	}

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
		entityNode->getAttr("EntityId", spawnParams.id);
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

		int castShadowMinSpec = CONFIG_LOW_SPEC;
		if (entityNode->getAttr("CastShadowMinSpec", castShadowMinSpec))
		{
			static const ICVar* const pObjShadowCastSpec = gEnv->pConsole->GetCVar("e_ObjShadowCastSpec");
			if (castShadowMinSpec <= pObjShadowCastSpec->GetIVal())
			{
				spawnParams.nFlags |= ENTITY_FLAG_CASTSHADOW;
			}
		}

		int giMode = 0;
		if (entityNode->getAttr("GIMode", giMode))
		{
			spawnParams.nFlagsExtended = (spawnParams.nFlagsExtended & ~ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK) | ((giMode << ENTITY_FLAG_EXTENDED_GI_MODE_BIT_OFFSET) & ENTITY_FLAG_EXTENDED_GI_MODE_BIT_MASK);
		}

		bool bDynamicDistanceShadows = false;
		if (entityNode->getAttr("DynamicDistanceShadows", bDynamicDistanceShadows) && bDynamicDistanceShadows)
		{
			spawnParams.nFlagsExtended |= ENTITY_FLAG_EXTENDED_DYNAMIC_DISTANCE_SHADOWS;
		}

		bool bGoodOccluder = false; // false by default (do not change, it must be coordinated with editor export)
		if (entityNode->getAttr("GoodOccluder", bGoodOccluder) && bGoodOccluder)
		{
			spawnParams.nFlags |= ENTITY_FLAG_GOOD_OCCLUDER;
		}

		bool bOutdoorOnly = false;
		if (entityNode->getAttr("OutdoorOnly", bOutdoorOnly) && bOutdoorOnly)
		{
			spawnParams.nFlags |= ENTITY_FLAG_OUTDOORONLY;
		}

		bool bNoDecals = false;
		if (entityNode->getAttr("NoDecals", bNoDecals) && bNoDecals)
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

	CryGUID entityGuid = spawnParams.guid;
	if (entityNode)
	{
		// Only runtime prefabs should have GUID id's
		const char* entityGuidStr = entityNode->getAttr("Id");
		if (entityGuidStr[0] != '\0')
		{
			entityGuid = CryGUID::FromString(entityGuidStr);
		}
	}

	IEntity* pSpawnedEntity = NULL;
	bool bWasSpawned = false;
	if (m_pEntitySystem->OnBeforeSpawn(spawnParams))
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
			if (XmlNodeRef componentNode = entityNode->findChild("Area"))
			{
				IEntityComponent* pComponent = pSpawnedEntity->GetOrCreateComponent<IEntityAreaComponent>();
				pComponent->LegacySerializeXML(entityNode, componentNode, true);
			}
			if (XmlNodeRef componentNode = entityNode->findChild("Rope"))
			{
				IEntityComponent* pComponent = pSpawnedEntity->GetOrCreateComponent<IEntityRopeComponent>();
				pComponent->LegacySerializeXML(entityNode, componentNode, true);
			}
			if (XmlNodeRef componentNode = entityNode->findChild("ClipVolume"))
			{
				IEntityComponent* pComponent = pSpawnedEntity->GetOrCreateComponent<IClipVolumeComponent>();
				pComponent->LegacySerializeXML(entityNode, componentNode, true);
			}

			// Load RenderNodeParams
			{
				auto& renderNodeParams = pCSpawnedEntity->GetEntityRender()->GetRenderNodeParams();
				int nLodRatio = 0;
				int nViewDistRatio = 0;
				if (entityNode->getAttr("LodRatio", nLodRatio))
				{
					// Change LOD ratio.
					renderNodeParams.lodRatio = nLodRatio;
				}
				if (entityNode->getAttr("ViewDistRatio", nViewDistRatio))
				{
					// Change ViewDistRatio ratio.
					renderNodeParams.viewDistRatio = nViewDistRatio;
				}
				int nMinSpec = -1;
				if (entityNode->getAttr("MinSpec", nMinSpec) && nMinSpec >= 0)
				{
					renderNodeParams.minSpec = (ESystemConfigSpec)nMinSpec;
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
				if (IEntityEventHandler* pEventHandler = pCSpawnedEntity->GetClass()->GetEventHandler())
					pEventHandler->LoadEntityXMLEvents(pCSpawnedEntity, entityNode);

				// Serialize script proxy.
				CEntityComponentLuaScript* pScriptProxy = pCSpawnedEntity->GetScriptProxy();
				if (pScriptProxy)
				{
					XmlNodeRef componentNode;
					if (XmlNodeRef componentsNode = entityNode->findChild("Components"))
					{
						for (int i = 0, n = componentsNode->getChildCount(); i < n; ++i)
						{
							CryInterfaceID componentTypeId;
							componentNode = componentsNode->getChild(i);
							if (!componentNode->getAttr("typeId", componentTypeId))
								continue;
							
							if (componentTypeId == pScriptProxy->GetCID())
								break;
						}
					}

					pScriptProxy->LegacySerializeXML(entityNode, componentNode, true);
				}
					
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
				CEntityComponentLuaScript* pScriptProxy = pCSpawnedEntity->GetScriptProxy();
				if (pScriptProxy)
					pScriptProxy->CallInitEvent(true);
			}
		}

		if (entityNode)
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
			EntityGUID parentGuid;
			if (entityNode->getAttr("ParentGuid", parentGuid))
			{
				AddQueuedAttachment(0, parentGuid, spawnParams.id, spawnParams.vPosition, spawnParams.qRotation, spawnParams.vScale, flags, attachmentTarget);
			}
			else
			{
				// Legacy maps loading.
				EntityId nParentId = 0;
				if (entityNode->getAttr("ParentId", nParentId))
				{
					AddQueuedAttachment(nParentId, CryGUID::Null(), spawnParams.id, spawnParams.vPosition, spawnParams.qRotation, spawnParams.vScale, flags, attachmentTarget);
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
						// If this is a runtime prefab we're spawning, queue the entity
						// link for later, since it has a guid target id we need to look up.
						AddQueuedEntityLink(pSpawnedEntity, linkNode);
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
		}
	}

	if (!bResult)
	{
		EntityWarning("[CEntityLoadManager::CreateEntity] Entity Load Failed: %s (%s)", spawnParams.sName, spawnParams.pClass->GetName());
	}

	outUsingId = (pSpawnedEntity ? pSpawnedEntity->GetId() : 0);

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::PrepareBatchCreation(int nSize)
{
	m_queuedAttachments.clear();
	m_queuedFlowgraphs.clear();

	m_queuedAttachments.reserve(nSize);
	m_queuedFlowgraphs.reserve(nSize);
}

//////////////////////////////////////////////////////////////////////////
void CEntityLoadManager::AddQueuedAttachment(EntityId nParent, EntityGUID parentGuid, EntityId nChild, const Vec3& pos, const Quat& rot, const Vec3& scale, const int flags, const char* target)
{
	SEntityAttachment entityAttachment;
	entityAttachment.child = nChild;
	entityAttachment.parent = nParent;
	entityAttachment.parentGuid = parentGuid;
	entityAttachment.pos = pos;
	entityAttachment.rot = rot;
	entityAttachment.scale = scale;
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
			if (!entityAttachment.parentGuid.IsNull())
			{
				parentId = m_pEntitySystem->FindEntityByGuid(entityAttachment.parentGuid);
			}
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
			IEntityComponent* pProxy = f.pEntity->CreateProxy(ENTITY_PROXY_FLOWGRAPH);
			if (pProxy)
				pProxy->LegacySerializeXML(f.pNode, f.pNode, true);
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
			EntityGUID targetGuid;
			EntityId targetId = 0;
			f.pNode->getAttr("TargetId",targetId);
			if (f.pNode->getAttr("TargetGuid", targetGuid))
			{
				targetId = gEnv->pEntitySystem->FindEntityByGuid(targetGuid);
			}
			const char* sLinkName = f.pNode->getAttr("Name");
			Quat relRot(IDENTITY);
			Vec3 relPos(IDENTITY);
			f.pEntity->AddEntityLink(sLinkName, targetId, targetGuid);
		}
	}

	stl::free_container(m_queuedEntityLinks);

	// Resume loading by calling POST SERIALZIE event
	SEntityEvent event;
	event.event = ENTITY_EVENT_POST_SERIALIZE;
	gEnv->pEntitySystem->SendEventToAll(event);
}
