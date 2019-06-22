// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SmartObjectOffMeshNavigation.h"
#include "SmartObjects.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"

class CRegisterSOHelper
{
public:
	struct SHelperInfo
	{
		SHelperInfo(const MNM::TriangleID& _triangleID, const Vec3& _position, SmartObjectHelper* _pHelper)
			: triangleID(_triangleID)
			, position(_position)
			, pHelper(_pHelper)
		{

		}

		SmartObjectHelper* pHelper;
		MNM::TriangleID    triangleID;
		Vec3               position;
	};
	typedef std::vector<SHelperInfo> THelpersInfo;

	struct SHelpersLink
	{
		SHelpersLink(int from, int to)
			: fromIndex(from)
			, toIndex(to)
		{

		}

		bool operator==(const SHelpersLink& other) const
		{
			return (fromIndex == other.fromIndex) && (toIndex == other.toIndex);
		}

		int fromIndex;
		int toIndex;
	};
	typedef std::vector<SHelpersLink> TObjectLinks;

	struct SAgentTypeInfo
	{
		friend class CRegisterSOHelper;

		SAgentTypeInfo(NavigationAgentTypeID _agentTypeID)
			: agentTypeID(_agentTypeID)
		{

		}

		ILINE NavigationAgentTypeID GetAgentTypeID() const { return agentTypeID; }

		bool                        IsUserClassCompatible(const char* userClass)
		{
			for (size_t i = 0; i < userClasses.size(); ++i)
			{
				if (strcmp(userClasses[i].c_str(), userClass))
					continue;

				return true;
			}

			return false;
		}

	private:

		ILINE void AddUserClass(const char* userClass) { stl::push_back_unique(userClasses, userClass); }

		NavigationAgentTypeID agentTypeID;
		std::vector<string>   userClasses;
	};

	CRegisterSOHelper(CSmartObject& smartObject, CSmartObjectClass& smartObjectClass)
		: m_smartObject(smartObject)
		, m_smartObjectClass(smartObjectClass)
		, m_navigationSystem(*gAIEnv.pNavigationSystem)
	{
		GenerateCompatibleTypes();
	}

	bool GetAgentInfo(SAgentTypeInfo& agentInfo)
	{
		if (m_currentElement < m_supportedAgentTypes.size())
		{
			agentInfo = m_supportedAgentTypes[m_currentElement];
			m_currentElement++;
			return true;
		}

		return false;
	}

	bool CanRegister() const
	{
		return !(m_supportedAgentTypes.empty());
	}

private:

	void GenerateCompatibleTypes()
	{
		const CSmartObjectClass::THelperLinks::const_iterator itLinksBegin = m_smartObjectClass.m_vHelperLinks.begin();
		const CSmartObjectClass::THelperLinks::const_iterator itLinksEnd = m_smartObjectClass.m_vHelperLinks.end();

		size_t agentTypeCount = m_navigationSystem.GetAgentTypeCount();
		m_supportedAgentTypes.reserve(agentTypeCount);

		for (size_t agentIdx = 0; agentIdx < agentTypeCount; ++agentIdx)
		{
			const NavigationAgentTypeID agentTypeId = m_navigationSystem.GetAgentTypeID(agentIdx);
			bool addAgentTypeInfo = false;
			SAgentTypeInfo agentTypeInfo(agentTypeId);

			for (CSmartObjectClass::THelperLinks::const_iterator linkIt = itLinksBegin; linkIt != itLinksEnd; ++linkIt)
			{
				const CSmartObjectClass::HelperLink& helperLink = *linkIt;
				const char* userClassName = (helperLink.condition && helperLink.condition->pUserClass) ? helperLink.condition->pUserClass->GetName().c_str() : NULL;

				if (userClassName && m_navigationSystem.AgentTypeSupportSmartObjectUserClass(agentTypeId, userClassName))
				{
					agentTypeInfo.AddUserClass(userClassName);
					addAgentTypeInfo = true;
				}
			}

			if (addAgentTypeInfo)
			{
				m_supportedAgentTypes.push_back(agentTypeInfo);
			}
		}

		m_currentElement = 0;
	}

	typedef std::vector<SAgentTypeInfo> TSupportedAgentTypes;
	TSupportedAgentTypes m_supportedAgentTypes;
	size_t               m_currentElement;

	CSmartObject&        m_smartObject;
	CSmartObjectClass&   m_smartObjectClass;
	NavigationSystem&    m_navigationSystem;
};

CSmartObjectOffMeshNavigation::CSmartObjectOffMeshNavigation()
{
	NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;
	const int agentTypesAmount = pNavigationSystem->GetAgentTypeCount();
	for (int agentTypeIndex = 0; agentTypeIndex < agentTypesAmount; agentTypeIndex++)
	{
		const NavigationAgentTypeID agentTypeId = pNavigationSystem->GetAgentTypeID(agentTypeIndex);
		pNavigationSystem->AddMeshChangeCallback(agentTypeId, functor(*this, &CSmartObjectOffMeshNavigation::OnNavMeshChanged));
	}
}

CSmartObjectOffMeshNavigation::~CSmartObjectOffMeshNavigation()
{
	NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;
	const int agentTypesAmount = pNavigationSystem->GetAgentTypeCount();
	for (int agentTypeIndex = 0; agentTypeIndex < agentTypesAmount; agentTypeIndex++)
	{
		const NavigationAgentTypeID agentTypeId = pNavigationSystem->GetAgentTypeID(agentTypeIndex);
		pNavigationSystem->RemoveMeshChangeCallback(agentTypeId, functor(*this, &CSmartObjectOffMeshNavigation::OnNavMeshChanged));
	}
}

void CSmartObjectOffMeshNavigation::RegisterSmartObject(CSmartObject* pSmartObject, CSmartObjectClass* pSmartObjectClass)
{
	assert(pSmartObject && pSmartObjectClass);

	if (!gAIEnv.pNavigationSystem->GetOffMeshNavigationManager()->IsLinkAdditionEnabled() || pSmartObjectClass->IsSmartObjectUser())
		return;

	// Filter out non-compatible smart-objects first...
	CRegisterSOHelper registerSOHelper(*pSmartObject, *pSmartObjectClass);
	if (!registerSOHelper.CanRegister())
		return;

	// If this smart object class is already associated with this object, unregister all related links to ensure we don't duplicate.
	if (ObjectRegistered(pSmartObject->GetEntityId(), pSmartObjectClass->GetName()))
	{
		UnregisterSmartObject(pSmartObject, pSmartObjectClass->GetName());
	}

	// Register the object, no matter what, so we keep track of it
	TRegisteredObjects::iterator registeredObjectIt = m_registeredObjects.find(pSmartObject->GetEntityId());
	if (registeredObjectIt == m_registeredObjects.end())
	{
		std::pair<TRegisteredObjects::iterator, bool> newObjectIt = m_registeredObjects.insert(TRegisteredObjects::value_type(pSmartObject->GetEntityId(), TSOClassInfo()));
		assert(newObjectIt.second);

		registeredObjectIt = newObjectIt.first;
	}

	// Iterate through all compatible agent types for the smart object
	CRegisterSOHelper::SAgentTypeInfo agentTypeInfo(NavigationAgentTypeID(0));
	std::vector<SmartObjectHelper*> validHelpers;

	while (registerSOHelper.GetAgentInfo(agentTypeInfo))
	{
		NavigationAgentTypeID agentTypeID = agentTypeInfo.GetAgentTypeID();
		NavigationMeshID targetMeshID;

		validHelpers.clear();
		validHelpers.reserve(pSmartObjectClass->m_vHelperLinks.size());

		std::vector<SmartObjectHelper*> validHelpers;

		// Find out to which mesh the object should belong
		for (CSmartObjectClass::SetHelpers::iterator itHelpersEnd = pSmartObjectClass->m_setNavHelpers.end(),
			itHelpers = pSmartObjectClass->m_setNavHelpers.begin(); itHelpers != itHelpersEnd; ++itHelpers)
		{
			SmartObjectHelper* pHelper = *itHelpers;

			// Calculate position
			const Vec3 helperPosition = pSmartObject->GetHelperPos(pHelper);

			NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentTypeID, helperPosition);

			if (targetMeshID && (targetMeshID != meshID))
			{
				//Bail out, not all helpers are connected to the same mesh
				return;
			}

			targetMeshID = meshID;
			validHelpers.push_back(pHelper);
		}

		//////////////////////////////////////////////////////////////////////////
		// Object belongs to a mesh; Create a new link
		if (targetMeshID)
		{
			// Grab the begin and end links of the smart object class
			const CSmartObjectClass::THelperLinks::const_iterator itLinksBegin = pSmartObjectClass->m_vHelperLinks.begin();
			const CSmartObjectClass::THelperLinks::const_iterator itLinksEnd = pSmartObjectClass->m_vHelperLinks.end();

			// Cache of registered object links to prevent duplication during iteration
			CRegisterSOHelper::TObjectLinks registeredLinks;
			registeredLinks.reserve(pSmartObjectClass->m_vHelperLinks.size());

			// Grab the link ID list
			// This is the link ID's generated for this smart object class on this smart object and are used as keys into m_links
			uint32 smartObjectClassNameCrC = CCrc32::ComputeLowercase(pSmartObjectClass->GetName().c_str());
			OffMeshLinkIDList& linkIDList = registeredObjectIt->second[smartObjectClassNameCrC];

			// Iterate over each helper link, adding their start/end points as off-mesh links if compatible with the
			// chosen user class.
			for (CSmartObjectClass::THelperLinks::const_iterator linkIt = itLinksBegin; linkIt != itLinksEnd; ++linkIt)
			{
				const CSmartObjectClass::HelperLink& helperLink = *linkIt;
				const char* userClassName = (helperLink.condition && helperLink.condition->pUserClass) ? helperLink.condition->pUserClass->GetName().c_str() : NULL;

				if (!userClassName || !agentTypeInfo.IsUserClassCompatible(userClassName))
					continue;  // Invalid smart object class name or not compatible with user smart object class

							   // Determine to/from helper indexes of this helper link
				int helperIndex = 0;
				int fromIndex = -1;
				int toIndex = -1;
				CSmartObjectClass::SetHelpers& helpers = pSmartObjectClass->m_setNavHelpers;
				CSmartObjectClass::SetHelpers::iterator itHelpersEnd = helpers.end();
				for (CSmartObjectClass::SetHelpers::iterator itHelpers = helpers.begin(); itHelpers != itHelpersEnd; ++itHelpers, ++helperIndex)
				{
					if (linkIt->from == *itHelpers)
					{
						fromIndex = helperIndex;
					}
					else if (linkIt->to == *itHelpers)
					{
						toIndex = helperIndex;
					}
				}

				// If both helper indexes are valid, attempt to register link
				if ((fromIndex != -1) && (toIndex != -1))
				{
					const bool alreadyRegistered = stl::find(registeredLinks, CRegisterSOHelper::SHelpersLink(fromIndex, toIndex));
					if (!alreadyRegistered)
					{
						const EntityId smartObjectEntityId = pSmartObject->GetEntityId();
						// Create a smart object off-mesh link data
						_smart_ptr<MNM::IOffMeshLink> pLinkData(new OffMeshLink_SmartObject(smartObjectEntityId, pSmartObject, pSmartObjectClass, validHelpers[fromIndex], validHelpers[toIndex]));

						// Register the new link with the off-mesh navigation system
						MNM::OffMeshLinkID linkID = MNM::OffMeshLinkID();

						MNM::SOffMeshLinkCallbacks callbacks;
						callbacks.acquireDataCallback = functor(*this, &CSmartObjectOffMeshNavigation::OnLinkDataRequested);
						callbacks.additionCompletedCallback = functor(linkIDList, &CSmartObjectOffMeshNavigation::OffMeshLinkIDList::OnLinkAdditionObjectServiced);
						callbacks.removedCallback = functor(*this, &CSmartObjectOffMeshNavigation::OnOffmeshLinkRemoved);
					
						linkID = gAIEnv.pNavigationSystem->GetIOffMeshNavigationManager().RequestLinkAddition(linkID, agentTypeID, pLinkData, callbacks);
						linkIDList.AddRequestededLinkId(linkID);

						// Prevent duplicate registration
						registeredLinks.push_back(CRegisterSOHelper::SHelpersLink(fromIndex, toIndex));
					}
				}
			}
		}
	}
}

MNM::EOffMeshLinkAdditionResult CSmartObjectOffMeshNavigation::OnLinkDataRequested(const NavigationAgentTypeID agentTypeId, MNM::IOffMeshLink* pLinkData, MNM::IOffMeshLink::SNavigationData& linkNavigationData) const
{
	OffMeshLink_SmartObject* pSmartObjectLinkData = pLinkData->CastTo<OffMeshLink_SmartObject>();
	if (!pSmartObjectLinkData)
		return MNM::EOffMeshLinkAdditionResult::AcquireDataFailed;

	return pSmartObjectLinkData->GetNavigationLinkData(agentTypeId, linkNavigationData);
}

void CSmartObjectOffMeshNavigation::UnregisterSmartObjectForAllClasses(CSmartObject* pSmartObject)
{
	CSmartObjectClasses& classes = pSmartObject->GetClasses();
	CSmartObjectClasses::iterator it, itEnd = classes.end();

	for (it = classes.begin(); it != itEnd; ++it)
	{
		UnregisterSmartObject(pSmartObject, (*it)->GetName());
	}
}

void CSmartObjectOffMeshNavigation::UnregisterSmartObject(CSmartObject* pSmartObject, const string& smartObjectClassName)
{
	assert(pSmartObject);

	// Attempt to find the object in the registered list
	const EntityId objectId = pSmartObject->GetEntityId();
	TRegisteredObjects::iterator objectIt = m_registeredObjects.find(objectId);

	if (objectIt != m_registeredObjects.end())
	{
		uint32 smartObjectClassNameCrC = CCrc32::ComputeLowercase(smartObjectClassName.c_str());
		TSOClassInfo::iterator classIt = objectIt->second.find(smartObjectClassNameCrC);
		if (classIt != objectIt->second.end())
		{
			// Remove associated links
			const OffMeshLinkIDList::TLinkIDList& linkList = classIt->second.GetLinkIDList();
			for (int i = 0; i < linkList.size(); ++i)
			{
				gAIEnv.pNavigationSystem->GetIOffMeshNavigationManager().RequestLinkRemoval(linkList[i]);
			}

			const OffMeshLinkIDList::TLinkIDList& requestedLinkList = classIt->second.GetRequestedIDList();
			for (int i = 0; i < requestedLinkList.size(); ++i)
			{
				gAIEnv.pNavigationSystem->GetIOffMeshNavigationManager().CancelLinkAddition(requestedLinkList[i]);
			}

			// Remove object from registration
			objectIt->second.erase(classIt);
		}

		// If there aren't any classes remaining on this registered object, remove it.
		if (objectIt->second.size() == 0)
		{
			m_registeredObjects.erase(objectIt);
		}
	}
}

bool CSmartObjectOffMeshNavigation::ObjectRegistered(const EntityId objectId, const string& smartObjectClassName) const
{
	uint32 smartObjectClassNameCrC = CCrc32::ComputeLowercase(smartObjectClassName.c_str());
	TRegisteredObjects::const_iterator itFoundObject = m_registeredObjects.find(objectId);
	return (itFoundObject != m_registeredObjects.end() && itFoundObject->second.find(smartObjectClassNameCrC) != itFoundObject->second.end());
}

bool CSmartObjectOffMeshNavigation::IsObjectLinkedWithNavigationMesh(const EntityId objectId) const
{
	TRegisteredObjects::const_iterator objectIt = m_registeredObjects.find(objectId);
	return (objectIt != m_registeredObjects.end()) ? !objectIt->second.empty() : false;
}

void CSmartObjectOffMeshNavigation::UpdateEditorDebugHelpers()
{
	if (!gEnv->IsEditing())
		return;

	static float time = 0.0f;

	time += (gEnv->pTimer->GetFrameTime() * 2.5f);

	IRenderAuxGeom* pRenderAux = gEnv->pRenderer->GetIRenderAuxGeom();

	SAuxGeomRenderFlags oldFlags = pRenderAux->GetRenderFlags();
	SAuxGeomRenderFlags renderFlags(oldFlags);

	renderFlags.SetAlphaBlendMode(e_AlphaBlended);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);

	pRenderAux->SetRenderFlags(renderFlags);

	const float alpha = clamp_tpl((1.0f + sinf(time)) * 0.5f, 0.25f, 0.7f);
	const ColorB color(255, 0, 0, (uint8)(alpha * 255));

	for (const auto& entityObjectPair : m_registeredObjects)
	{
		if (!entityObjectPair.second.empty())
			continue;

		IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(entityObjectPair.first);
		if (pObjectEntity)
		{
			uint32 numSOFound = 0;
			gAIEnv.pSmartObjectManager->GetSOClassTemplateIStatObj(pObjectEntity, NULL, numSOFound);

			ISmartObjectManager::IStatObjPtr* ppStatObjects = NULL;
			if (numSOFound)
			{
				ppStatObjects = new ISmartObjectManager::IStatObjPtr[numSOFound];
				if (gAIEnv.pSmartObjectManager->GetSOClassTemplateIStatObj(pObjectEntity, ppStatObjects, numSOFound) > 0)
				{
					pRenderAux->DrawAABB(ppStatObjects[0]->GetAABB(), pObjectEntity->GetWorldTM(), true, color, eBBD_Faceted);
				}

				SAFE_DELETE_ARRAY(ppStatObjects);
			}
		}
	}
	pRenderAux->SetRenderFlags(oldFlags);
}


void CSmartObjectOffMeshNavigation::OnOffmeshLinkRemoved(const MNM::OffMeshLinkID linkId, const NavigationMeshID meshId, const MNM::EOffMeshLinkRemovalReason reason)
{
	_smart_ptr<MNM::IOffMeshLink> pOffmeshLinkData(gAIEnv.pNavigationSystem->GetIOffMeshNavigationManager().GetOffMeshLink(linkId)->CastTo<OffMeshLink_SmartObject>());
	CRY_ASSERT(pOffmeshLinkData != nullptr);

	const EntityId entityId = pOffmeshLinkData->GetEntityIdForOffMeshLink();
	TRegisteredObjects::iterator objectIt = m_registeredObjects.find(entityId);
	if (objectIt == m_registeredObjects.end())
		return;

	if (reason == MNM::EOffMeshLinkRemovalReason::MeshUpdated)
	{
		for (TSOClassInfo::iterator classInfoIt = objectIt->second.begin(); classInfoIt != objectIt->second.end(); ++classInfoIt)
		{
			OffMeshLinkIDList::TLinkIDList& linkList = classInfoIt->second.GetLinkIDList();
			if (stl::find(linkList, linkId))
			{
				// Refresh connection
				const NavigationAgentTypeID agentTypeId = gAIEnv.pNavigationSystem->GetMesh(meshId).agentTypeID;

				MNM::SOffMeshLinkCallbacks callbacks;
				callbacks.acquireDataCallback = functor(*this, &CSmartObjectOffMeshNavigation::OnLinkDataRequested);
				callbacks.additionCompletedCallback = functor(classInfoIt->second, &CSmartObjectOffMeshNavigation::OffMeshLinkIDList::OnLinkAdditionObjectServiced);
				callbacks.removedCallback = functor(*this, &CSmartObjectOffMeshNavigation::OnOffmeshLinkRemoved);

				gAIEnv.pNavigationSystem->GetIOffMeshNavigationManager().RequestLinkAddition(linkId, agentTypeId, pOffmeshLinkData, callbacks);
				return;
			}
		}
	}
	else if (reason == MNM::EOffMeshLinkRemovalReason::MeshDestroyed)
	{
		for (TSOClassInfo::iterator classInfoIt = objectIt->second.begin(); classInfoIt != objectIt->second.end(); ++classInfoIt)
		{
			OffMeshLinkIDList::TLinkIDList& linkList = classInfoIt->second.GetLinkIDList();
			
			// Mesh doesn't exist anymore, remove the connection
			if (stl::find_and_erase(linkList, linkId))
				return;
		}
	}
}

void CSmartObjectOffMeshNavigation::OnNavMeshChanged(NavigationAgentTypeID navAgentId, NavigationMeshID navMeshId, MNM::TileID tileId)
{
	std::vector<EntityId> tempObjectIds;
	tempObjectIds.reserve(m_registeredObjects.size());

	// Find object's smart classes which were unable to register before to be registered again because of the change
	for (auto objectIt = m_registeredObjects.cbegin(), iterEnd = m_registeredObjects.cend(); objectIt != iterEnd; ++objectIt)
	{
		const TSOClassInfo classInfos = objectIt->second;
		for (TSOClassInfo::const_iterator classIt = classInfos.begin(); classIt != classInfos.end(); ++classIt)
		{
			const OffMeshLinkIDList::TLinkIDList& linkList = classIt->second.GetLinkIDList();
			if (linkList.empty() == false)
				continue;

			tempObjectIds.push_back(objectIt->first);
		}
	}

	// Register again those objects which could have been affected by the change
	for (auto objectIt = tempObjectIds.cbegin(), iterEnd = tempObjectIds.cend(); objectIt != iterEnd; ++objectIt)
	{
		CSmartObject* pSmartObject = CSmartObjectManager::GetSmartObject(*objectIt);
		CRY_ASSERT(pSmartObject);
		if (pSmartObject)
		{
			CSmartObjectClasses& classes = pSmartObject->GetClasses();
			CSmartObjectClasses::iterator it, itEnd = classes.end();
			for (it = classes.begin(); it != itEnd; ++it)
			{
				CSmartObjectClass* pSmartObjectClass = *it;
				RegisterSmartObject(pSmartObject, pSmartObjectClass);
			}
		}
	}
}

void CSmartObjectOffMeshNavigation::OnNavigationEvent(const INavigationSystem::ENavigationEvent event)
{
	if (event == INavigationSystem::ENavigationEvent::NavigationCleared)
	{
		m_registeredObjects.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
// OffMeshLink_SmartObject
//////////////////////////////////////////////////////////////////////////

Vec3 OffMeshLink_SmartObject::GetStartPosition() const
{
	return m_pSmartObject->GetHelperPos(m_pFromHelper);
}

Vec3 OffMeshLink_SmartObject::GetEndPosition() const
{
	return m_pSmartObject->GetHelperPos(m_pToHelper);
}

bool OffMeshLink_SmartObject::CanUse(const EntityId requesterEntityId, float* pCostMultiplier) const
{
	return gAIEnv.pSmartObjectManager->GetSmartObjectLinkCostFactorForMNM(this, requesterEntityId, pCostMultiplier);
}

MNM::EOffMeshLinkAdditionResult OffMeshLink_SmartObject::GetNavigationLinkData(const NavigationAgentTypeID agentTypeId, SNavigationData& navigationLinkData) const
{
	// Query the entry/exit positions
	const Vec3 startPoint = GetStartPosition();
	const Vec3 endPoint = GetEndPosition();
	
	// Grab the navigation mesh
	const NavigationMeshID startMeshId = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentTypeId, startPoint);
	const NavigationMeshID endMeshId = gAIEnv.pNavigationSystem->GetEnclosingMeshID(agentTypeId, endPoint);

	if (startMeshId != endMeshId)
	{
		// Bail out, starting and ending points aren't on the same mesh
		return MNM::EOffMeshLinkAdditionResult::NotSameMesh;
	}
	if (!startMeshId)
	{
		// Bail out, navigation mesh was not found
		return MNM::EOffMeshLinkAdditionResult::InvalidMesh;
	}

	const NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(startMeshId);
	
	MNM::TriangleID startTriangleID;
	MNM::TriangleID endTriangleID;

	MNM::vector3_t fixedStartPoint(startPoint);
	MNM::vector3_t fixedEndPoint(endPoint);

	const MNM::real_t range = MNM::real_t(1.0f);

	navigationLinkData.meshId = startMeshId;

	MNM::DefaultQueryFilters::g_acceptAllFilterVirtual;

	// Get entry triangle
	navigationLinkData.startTriangleId = mesh.navMesh.QueryTriangleAt(fixedStartPoint, range, range, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, &MNM::DefaultQueryFilters::g_acceptAllFilterVirtual);
	if (!navigationLinkData.startTriangleId)
		return MNM::EOffMeshLinkAdditionResult::InvalidStartTriangle; // Bail out; Entry position not connected to a triangle

	// Get end triangle
	navigationLinkData.endTriangleId = mesh.navMesh.QueryTriangleAt(fixedEndPoint, range, range, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, &MNM::DefaultQueryFilters::g_acceptAllFilterVirtual);
	if (!navigationLinkData.endTriangleId)
		return MNM::EOffMeshLinkAdditionResult::InvalidEndTriangle; // Bail out; Exit position not connected to a triangle

	return MNM::EOffMeshLinkAdditionResult::Success;
}