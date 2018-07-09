// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "EntityContainerMgr.h"

#include <CryFlowGraph/IFlowGraphModuleManager.h>


CEntityContainerMgr::CEntityContainerMgr()
	: m_listeners(16) // reasonable starting point
{
	REGISTER_CVAR2("es_debugContainers", &CEntityContainer::CVars::ms_es_debugContainers, CEntityContainer::CVars::ms_es_debugContainers,
	               VF_DEV_ONLY, "Entity Container Debugger");
}


CEntityContainerMgr::~CEntityContainerMgr()
{
	if (gEnv->pConsole)
		gEnv->pConsole->UnregisterVariable("es_debugContainers");
}


size_t CEntityContainerMgr::AddEntity(EntityId containerId, EntityId entityId)
{
	CEntityContainer* pContainer = GetContainer(containerId);
	return pContainer && entityId ? pContainer->AddEntity(entityId) : 0;
}


size_t CEntityContainerMgr::RemoveEntity(EntityId containerId, EntityId entityId)
{
	CEntityContainer* pContainer = GetContainer(containerId);
	return pContainer ? pContainer->RemoveEntity(entityId) : 0;
}


size_t CEntityContainerMgr::AddContainerOfEntitiesToContainer(EntityId tgtContainerId, EntityId srcContainerId)
{
	size_t tgtContainerSize = 0;

	CEntityContainer* pTgtContainer = GetContainer(tgtContainerId);
	if (pTgtContainer)
	{
		if (srcContainerId != tgtContainerId)
		{
			CEntityContainer::TEntitiesInContainer entitiesToAdd;
			GetEntitiesInContainer(srcContainerId, entitiesToAdd);
			for (CEntityContainer::TSentityInfoParam& entityInfo : entitiesToAdd)
			{
				pTgtContainer->AddEntity(entityInfo);
			}
		}
		else
		{
			ENTITY_CONTAINER_LOG("CEntityContainerMgr::AddContainerOfEntitiesToContainer (ERROR): trying to add container [%s] to itself", pTgtContainer->GetName());
		}
		tgtContainerSize = pTgtContainer->GetSize();
	}

	return tgtContainerSize;
}


size_t CEntityContainerMgr::RemoveContainerOfEntitiesFromContainer(EntityId tgtContainerId, EntityId srcContainerId)
{
	size_t tgtContainerSize = 0;

	CEntityContainer* pTgtContainer = GetContainer(tgtContainerId);
	if (pTgtContainer)
	{
		if (srcContainerId != tgtContainerId)
		{
			CEntityContainer::TEntitiesInContainer entitiesToRemove;
			GetEntitiesInContainer(srcContainerId, entitiesToRemove);
			for (CEntityContainer::TSentityInfoParam& entityInfo : entitiesToRemove)
			{
				pTgtContainer->RemoveEntity(entityInfo.id);
			}
		}
		else
		{
			pTgtContainer->Clear();
			ENTITY_CONTAINER_LOG("CEntityContainerMgr::RemoveContainerOfEntitiesFromContainer (Warning): asked to REMOVE container [%s] from itself. Did you mean Clear()? Clear operation performed.", pTgtContainer->GetName());
		}

		tgtContainerSize = pTgtContainer->GetSize();
	}

	return tgtContainerSize;
}


void CEntityContainerMgr::RemoveEntityFromAllContainers(EntityId entityId)
{
	for (auto& pairInfo : m_containers)
	{
		RemoveEntity(pairInfo.first, entityId);
	}
}


size_t CEntityContainerMgr::GetContainerSize(EntityId containerId) const
{
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	return pContainer ? pContainer->GetSize() : 0;
}


void CEntityContainerMgr::ClearContainer(EntityId containerId)
{
	CEntityContainer* pContainer = GetContainer(containerId);
	if (pContainer)
	{
		pContainer->Clear();
	}
}


bool CEntityContainerMgr::CreateContainer(EntityId containerId, const char* szContainerName /*= nullptr */)
{
	bool inserted = m_containers.emplace(containerId, CEntityContainer(containerId, &m_listeners, szContainerName)).second;
	return inserted;
}


void CEntityContainerMgr::DestroyContainer(EntityId containerId)
{
	m_containers.erase(containerId);
}


CEntityContainer* CEntityContainerMgr::GetContainer(EntityId containerId)
{
	return const_cast<CEntityContainer*>(GetContainerConst(containerId));
}


const CEntityContainer* CEntityContainerMgr::GetContainerConst(EntityId containerId) const
{
	if (containerId != INVALID_ENTITYID)
	{
		TContainerMap::const_iterator it = m_containers.find(containerId);
		if (m_containers.end() != it)
		{
			return &it->second;
		}
	}

	return nullptr;
}


void CEntityContainerMgr::SetHideEntities(EntityId containerId, bool bHide)
{
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer)
	{
		std::vector<EntityId> idsToRemove;

		const CEntityContainer::TEntitiesInContainer& entities = pContainer->GetEntities();
		for (const CEntityContainer::SEntityInfo& entityInfo : entities)
		{
			if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityInfo.id))
			{
				pEntity->Hide(bHide);
			}
			else
			{
				// gather entity ids from entities that no longer exist
				idsToRemove.push_back(entityInfo.id);
			}
		}

		// remove entities that no longer exist
		if (!idsToRemove.empty())
		{
			for (size_t i = 0, numItems = idsToRemove.size(); i < numItems; ++i)
			{
				RemoveEntity(containerId, idsToRemove[i]);
			}
		}
	}
}


void CEntityContainerMgr::RemoveEntitiesFromTheGame(EntityId containerId)
{
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer)
	{
		const CEntityContainer::TEntitiesInContainer& entities = pContainer->GetEntities();
		for (const CEntityContainer::SEntityInfo& entityInfo : entities)
		{
			gEnv->pEntitySystem->RemoveEntity(entityInfo.id);
		}

		ClearContainer(containerId);
	}
}


EntityId CEntityContainerMgr::GetRandomEntity(EntityId containerId) const
{
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer && !pContainer->IsEmpty())
	{
		const CEntityContainer::TEntitiesInContainer& entities = pContainer->GetEntities();
		const uint32 idx = cry_random((size_t)0, entities.size() - 1); // returns size_t between 0 and size-1
		return entities[idx].id;
	}

	return 0;
}


EntityId CEntityContainerMgr::GetEntitysContainerId(EntityId entityId) const
{
	auto const it = std::find_if(m_containers.cbegin(), m_containers.cend(), [entityId](const std::pair<EntityId, CEntityContainer>& container) { return container.second.IsInContainer(entityId); });

	if (it != m_containers.end())
	{
		return it->second.GetId();
	}

	return 0;
}


void CEntityContainerMgr::GetEntitysContainerIds(EntityId entityId, CEntityContainer::TEntitiesInContainer& resultIdArray) const
{
	for (const auto& pairInfo : m_containers)
	{
		const CEntityContainer& container = pairInfo.second;
		if (container.IsInContainer(entityId))
		{
			resultIdArray.push_back(container.GetId());
		}
	}
}


void CEntityContainerMgr::Reset()
{
	// Clear each container and notify
	for (auto& pairInfo : m_containers)
	{
		ClearContainer(pairInfo.first);
	}

	// Empty the container's array
	m_containers.clear();
}


bool CEntityContainerMgr::RunModule(EntityId containerId, const char* moduleName)
{
	IFlowGraphModule* pModule = gEnv->pFlowSystem->GetIModuleManager()->GetModule(moduleName);
	if (!pModule)
	{
		return false;
	}

	std::vector<EntityId> idsToRemove;

	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer && !pContainer->IsEmpty())
	{
		const CEntityContainer::TEntitiesInContainer& entities = pContainer->GetEntities();
		for (CEntityContainer::TSentityInfoParam entityInfo : entities)
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityInfo.id);
			if (pEntity)
			{
				pModule->CallDefaultInstanceForEntity(pEntity);
			}
			else
			{
				// gather entity ids from entities that no longer exist
				idsToRemove.push_back(entityInfo.id);
			}
		}
	}

	// remove entities that no longer exist
	if (!idsToRemove.empty())
	{
		for (size_t i = 0, numItems = idsToRemove.size(); i < numItems; ++i)
		{
			RemoveEntity(containerId, idsToRemove[i]);
		}
	}

	return true;
}


bool CEntityContainerMgr::IsContainer(EntityId entityId) const
{
	for (auto const& pairInfo : m_containers)
	{
		if (pairInfo.second.GetId() == entityId)
			return true;
	}

	return false;
}


bool CEntityContainerMgr::GetEntitiesInContainer(EntityId containerId, CEntityContainer::TEntitiesInContainer& oArrayOfEntities, bool addOriginalEntityIfNotContainer /* = false */) const
{
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer && !pContainer->IsEmpty())
	{
		const CEntityContainer::TEntitiesInContainer& entities = pContainer->GetEntities();
		oArrayOfEntities.reserve(entities.size() + oArrayOfEntities.size());

		for (const CEntityContainer::SEntityInfo& entityInfo : entities)
		{
			oArrayOfEntities.push_back(entityInfo);
		}
	}
	else if (addOriginalEntityIfNotContainer)
	{
		// add the original entity to the result in case containerId was not a container
		oArrayOfEntities.emplace_back(CEntityContainer::SEntityInfo(containerId));
	}

	return pContainer != nullptr;
}


size_t CEntityContainerMgr::MergeContainers(EntityId srcContainerId, EntityId dstContainerId, bool clearSrc /* = true */)
{
	CEntityContainer* pContainerSrc = GetContainer(srcContainerId);
	CEntityContainer* pContainerDst = GetContainer(dstContainerId);
	size_t dstContainerSize = 0;

	if (pContainerSrc && pContainerDst)
	{
		// Copy entities from Src to Dst
		dstContainerSize = pContainerDst->AddEntitiesFromContainer(*pContainerSrc);

		// Clear Src container
		if (clearSrc)
		{
			pContainerSrc->Clear();
		}
	}

	return dstContainerSize;
}


void CEntityContainerMgr::DebugRender(EntityId containerId)
{
#ifndef RELEASE
	if (CEntityContainer::CVars::IsEnabled(CEntityContainer::CVars::eDebugFlag_InEditor) || !gEnv->pGameFramework->IsEditing())
	{
		if (const IEntity* pEntityContainer = gEnv->pEntitySystem->GetEntity(containerId))
		{
			if (const CEntityContainer* pGroup = GetContainerConst(containerId))
			{
				// Render at original position
				DebugRender(containerId, pEntityContainer->GetWorldPos());
			}
		}
	}
#endif // !RELEASE
}


void CEntityContainerMgr::DebugRender(EntityId containerId, const Vec3& srcPos)
{
#ifndef RELEASE
	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();
	const CEntityContainer* pContainer = GetContainerConst(containerId);
	if (pContainer && pAux)
	{
		const Vec3 containerPosWS = srcPos;
		const Vec3 kDeltaOffset(0.f, 0.f, 0.2f);
		Vec3 textOffset(0.f, 0.f, 0.2f);
		static float k = 0.f;

		const CEntityContainer& container = *pContainer;
		const CEntityContainer::TEntitiesInContainer& entitiesInContainer = container.GetEntities();
		for (auto const& entity : entitiesInContainer)
		{
			const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entity.id);
			const char* szName = pEntity ? pEntity->GetName() : nullptr;

			// Draw Group Line and entities info
			if (CEntityContainer::CVars::IsEnabled(CEntityContainer::CVars::eDebugFlag_ShowConnections))
			{
				k = k > 0.99f ? 0.f : k + 0.001f;
				Vec3 line = pEntity->GetWorldPos() - containerPosWS;
				pAux->DrawSphere(containerPosWS + line * k, 0.015f, Col_Yellow);
				pAux->DrawLine(containerPosWS, Col_White, pEntity->GetWorldPos(), Col_White);
				IRenderAuxText::DrawLabelF(containerPosWS + line * k + Vec3(0.f, 0.f, 0.1f), 1.3f, "[%s:%d]", szName ? szName : "NULL(!?)", entity.id);
			}
			IRenderAuxText::DrawLabelF(containerPosWS + textOffset, 1.3f, "[%s:%d]", szName ? szName : "NULL(!?)", entity.id);
			textOffset += kDeltaOffset;
		}

		// Draw Group info
		//pAux->DrawRangeArc(containerPosWS,	groupDirFw, gf_PI2, 0.7f, 0.2f, Col_DarkWood, Col_BlueViolet, false);
		pAux->DrawSphere(containerPosWS, 0.1f, Col_Yellow);
		pAux->DrawLine(containerPosWS, Col_Yellow, containerPosWS + textOffset, Col_Red);
		IRenderAuxText::DrawLabelF(containerPosWS + textOffset, 1.6f, "CONTAINER:[%s][Id:%d][Size:%d]", container.GetName(), container.GetId(), container.GetSize());
	}
#endif // !RELEASE
}


void CEntityContainerMgr::Update()
{
	if (CEntityContainer::CVars::IsEnabled(CEntityContainer::CVars::eDebugFlag_ShowInfo))
	{
		for (const auto& pairInfo : m_containers)
		{
			DebugRender(pairInfo.first);
		}
	}
}
