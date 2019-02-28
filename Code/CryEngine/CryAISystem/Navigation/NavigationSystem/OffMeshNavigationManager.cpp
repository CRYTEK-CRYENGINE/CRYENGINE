// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "OffMeshNavigationManager.h"

#include "SmartObjects.h"
#include "NavigationSystem.h"

OffMeshNavigationManager::OffMeshNavigationManager(const int offMeshMapSize)
	: m_offMeshMap(offMeshMapSize)
	, m_objectRegistrationEnabled(false)
	, m_operations(128)
	, m_listeners(32)
{
}

const MNM::OffMeshNavigation& OffMeshNavigationManager::GetOffMeshNavigationForMesh(const NavigationMeshID& meshID) const
{
	if (meshID && m_offMeshMap.validate(meshID))
	{
		return m_offMeshMap[meshID];
	}
	else
	{
		assert(0);
		AIWarning("No off-mesh navigation available, returning empty one! Navigation might not be able to use external links when path-finding");
		return m_emptyOffMeshNavigation;
	}
}

MNM::OffMeshNavigation& OffMeshNavigationManager::GetOffMeshNavigationForMesh(const NavigationMeshID& meshID)
{
	if (meshID && m_offMeshMap.validate(meshID))
	{
		return m_offMeshMap[meshID];
	}
	else
	{
		assert(0);
		AIWarning("No off-mesh navigation available, returning empty one! Navigation might not be able to use external links when path-finding");
		return m_emptyOffMeshNavigation;
	}
}

void OffMeshNavigationManager::QueueCustomLinkAddition(const MNM::LinkAdditionRequest& request)
{
#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "OffMeshNavigationManager::QueueCustomLinkAddition for entity %x", request.requestOwner);
#endif
	m_operations.push_back(request);
}

void OffMeshNavigationManager::QueueCustomLinkRemoval(const MNM::LinkRemovalRequest& request)
{
#if DEBUG_MNM_LOG_OFFMESH_LINK_OPERATIONS
	AILogCommentID("<MNM:OffMeshLink>", "OffMeshNavigationManager::QueueCustomLinkRemoval linkId %u for entity %x", request.linkId, request.requestOwner);
#endif
	m_operations.push_back(request);
}

bool OffMeshNavigationManager::IsLinkRemovalRequested(const MNM::OffMeshLinkID& linkID) const
{
	for (const MNM::OffMeshOperationRequestBase& requestBase : m_operations)
	{
		if ((requestBase.operationType == MNM::eOffMeshOperationType_Remove) && (requestBase.linkId == linkID))
		{
			return true;
		}
	}

	return false;
}

bool OffMeshNavigationManager::AddCustomLink(const NavigationMeshID& meshID, MNM::OffMeshLinkPtr& pLinkData, MNM::OffMeshLinkID& linkID, MNM::TriangleID* pOutStartTriangleID, MNM::TriangleID* pOutEndTriangleID, const bool bCloneLinkData)
{
	// Grab the navigation mesh
	NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);

	CRY_ASSERT(pLinkData.get() != nullptr);

	MNM::OffMeshLink& linkData = *pLinkData;

	// Query the entry/exit positions
	const Vec3 startPoint = linkData.GetStartPosition();
	const Vec3 endPoint = linkData.GetEndPosition();

	MNM::vector3_t fixedStartPoint(startPoint);
	MNM::vector3_t fixedEndPoint(endPoint);

	MNM::TriangleID startTriangleID, endTriangleID;

	const MNM::real_t range = MNM::real_t(1.0f);

	// Get entry triangle
	startTriangleID = mesh.navMesh.GetTriangleAt(fixedStartPoint, range, range, nullptr);

	if (!startTriangleID)
	{
		//Bail out; Entry position not connected to a triangle
		return false;
	}

	// Get entry triangle
	endTriangleID = mesh.navMesh.GetTriangleAt(fixedEndPoint, range, range, nullptr);

	if (!endTriangleID)
	{
		//Bail out; Exit position not connected to a triangle
		return false;
	}

	if (startTriangleID == endTriangleID)
	{
		// points are on the same triangle so don't bother
		return false;
	}

	// Select the corresponding off-mesh navigation object
	CRY_ASSERT_TRACE(m_offMeshMap.validate(meshID), ("Trying to add offmesh link to invalid mesh! (meshID = %u)", (uint32)meshID));
	MNM::OffMeshNavigation& offMeshNavigation = m_offMeshMap[meshID];

	// Important: if we already added this particular link before, we need to remove it prior to re-adding it, as the startTriangleID might be
	//            a different one now and then the NavMesh would suddenly have 2 different triangle-links referring to the same offmesh-link.
	//            This can happen if there are multiple add-link requests in m_operations dealing with the same linkID but with *different* start/end positions (!)
	TLinkInfoMap::const_iterator it = m_links.find(linkID);
	if (it != m_links.end())
	{
		// we added this link before already, now see if the starting triangle changed
		if (it->second.startTriangleID != startTriangleID)
		{
			// gotcha! (notice that we end up here very rarely in practice)
			offMeshNavigation.RemoveLink(mesh, it->second.startTriangleID, linkID);
		}
	}

	// Register the new link with the off-mesh navigation system
	offMeshNavigation.AddLink(mesh, startTriangleID, endTriangleID, *&linkID);
	CRY_ASSERT_TRACE(linkID != MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID, ("Adding new offmesh link failed"));

	MNM::OffMeshLinkPtr pOffMeshLink;
	if (bCloneLinkData)
	{
		pOffMeshLink.reset(linkData.Clone());
	}
	else
	{
		pOffMeshLink = pLinkData;
	}
	pOffMeshLink->SetLinkID(linkID);

	m_links[linkID] = SLinkInfo(meshID, startTriangleID, endTriangleID, std::move(pOffMeshLink));

	gAIEnv.pNavigationSystem->AddOffMeshLinkIslandConnectionsBetweenTriangles(meshID, startTriangleID, endTriangleID, linkID);

	if (pOutStartTriangleID)
	{
		*pOutStartTriangleID = startTriangleID;
	}

	if (pOutEndTriangleID)
	{
		*pOutEndTriangleID = endTriangleID;
	}

	return true;
}

void OffMeshNavigationManager::RemoveCustomLink(const MNM::OffMeshLinkID& linkID)
{
	TLinkInfoMap::iterator linkIt = m_links.find(linkID);
	if (linkIt != m_links.end())
	{
		SLinkInfo& linkInfo = linkIt->second;

		// Select the corresponding off-mesh navigation object
		assert(m_offMeshMap.validate(linkInfo.meshID));
		MNM::OffMeshNavigation& offMeshNavigation = m_offMeshMap[linkInfo.meshID];
		NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(linkInfo.meshID);

		// Remove link
		// NOTE: There should only ever be one element in the link array
		offMeshNavigation.RemoveLink(mesh, linkInfo.startTriangleID, linkID);

		gAIEnv.pNavigationSystem->RemoveOffMeshLinkIslandsConnectionBetweenTriangles(linkInfo.meshID, linkInfo.startTriangleID, linkInfo.endTriangleID, linkID);

		// Remove cached data
		m_links.erase(linkIt);
	}
}

void OffMeshNavigationManager::ProcessQueuedRequests()
{
	if (!m_operations.empty())
	{
		for (Operations::iterator it = m_operations.begin(); it != m_operations.end(); ++it)  // notice: the callbacks might add new operations, so we cannot simply cache end()
		{
			switch (it->operationType)
			{
			case MNM::eOffMeshOperationType_Add:
				{
					MNM::TriangleID startTriangleID = MNM::TriangleID(0);
					MNM::TriangleID endTriangleID = MNM::TriangleID(0);
					const bool linkGotSuccessfullyAdded = AddCustomLink(it->meshId, it->pLinkData, it->linkId, &startTriangleID, &endTriangleID, it->bCloneLinkData);
					if (it->callback)
					{
						it->callback(MNM::SOffMeshOperationCallbackData(it->linkId, startTriangleID, endTriangleID, linkGotSuccessfullyAdded));
					}
				}
				break;
			case MNM::eOffMeshOperationType_Remove:
				NotifyAllListenerAboutLinkDeletion(it->linkId);
				RemoveCustomLink(it->linkId);
				break;
			default:
				assert(0);
			}

		}

		stl::free_container(m_operations);
	}
}

void OffMeshNavigationManager::NotifyAllListenerAboutLinkDeletion(const MNM::OffMeshLinkID& linkID)
{
	for (Listeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnOffMeshLinkGoingToBeRemoved(linkID);
	}
}

MNM::OffMeshLink* OffMeshNavigationManager::GetOffMeshLink(const MNM::OffMeshLinkID& linkID)
{
	TLinkInfoMap::iterator linkIt = m_links.find(linkID);
	if (linkIt != m_links.end())
	{
		SLinkInfo& linkInfo = linkIt->second;
		return linkInfo.offMeshLink.get();
	}

	return NULL;
}

const MNM::OffMeshLink* OffMeshNavigationManager::GetOffMeshLink(const MNM::OffMeshLinkID& linkID) const
{
	TLinkInfoMap::const_iterator linkIt = m_links.find(linkID);
	if (linkIt != m_links.end())
	{
		const SLinkInfo& linkInfo = linkIt->second;
		return linkInfo.offMeshLink.get();
	}

	return NULL;
}

void OffMeshNavigationManager::RegisterListener(IOffMeshNavigationListener* pListener, const char* listenerName)
{
	m_listeners.Add(pListener, listenerName);
}

void OffMeshNavigationManager::UnregisterListener(IOffMeshNavigationListener* pListener)
{
	m_listeners.Remove(pListener);
}

void OffMeshNavigationManager::RemoveAllQueuedAdditionRequestForEntity(const EntityId requestOwner)
{
	m_operations.erase(std::remove_if(m_operations.begin(), m_operations.end(), MNM::IsOffMeshAdditionOperationRequestRelatedToEntity(requestOwner)), m_operations.end());
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
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

void OffMeshNavigationManager::RegisterSmartObject(CSmartObject* pSmartObject, CSmartObjectClass* pSmartObjectClass)
{
	assert(pSmartObject && pSmartObjectClass);

	if (!CanRegisterObject() || pSmartObjectClass->IsSmartObjectUser())
		return;

	//Filter out non-compatible smart-objects first...
	CRegisterSOHelper registerSOHelper(*pSmartObject, *pSmartObjectClass);
	if (!registerSOHelper.CanRegister())
		return;

	//////////////////////////////////////////////////////////////////////////
	/// If this smart object class is already associated with this object, unregister
	/// all related links to ensure we don't duplicate.
	if (ObjectRegistered(pSmartObject->GetEntityId(), pSmartObjectClass->GetName()))
	{
		UnregisterSmartObject(pSmartObject, pSmartObjectClass->GetName());
	}

	//////////////////////////////////////////////////////////////////////////
	/// Register the object, no matter what, so we keep track of it
	TRegisteredObjects::iterator registeredObjectIt = m_registeredObjects.find(pSmartObject->GetEntityId());
	if (registeredObjectIt == m_registeredObjects.end())
	{
		std::pair<TRegisteredObjects::iterator, bool> newObjectIt = m_registeredObjects.insert(TRegisteredObjects::value_type(pSmartObject->GetEntityId(), TSOClassInfo()));
		assert(newObjectIt.second);

		registeredObjectIt = newObjectIt.first;
	}

	//////////////////////////////////////////////////////////////////////////
	/// Iterate through all compatible agent types for the smart object
	CRegisterSOHelper::SAgentTypeInfo agentTypeInfo(NavigationAgentTypeID(0));
	std::vector<SmartObjectHelper*> validHelpers;

	while (registerSOHelper.GetAgentInfo(agentTypeInfo))
	{
		NavigationAgentTypeID agentTypeID = agentTypeInfo.GetAgentTypeID();
		NavigationMeshID targetMeshID;

		validHelpers.clear();
		validHelpers.reserve(pSmartObjectClass->m_vHelperLinks.size());

		std::vector<SmartObjectHelper*> validHelpers;

		//////////////////////////////////////////////////////////////////////////
		/// Find out to which mesh the object should belong
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
		/// Object belongs to a mesh; Create a new link
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
						MNM::OffMeshLinkPtr linkData(new OffMeshLink_SmartObject(smartObjectEntityId, pSmartObject, pSmartObjectClass, validHelpers[fromIndex], validHelpers[toIndex]));

						// Register the new link with the off-mesh navigation system
						MNM::OffMeshLinkID linkID = MNM::Constants::eOffMeshLinks_InvalidOffMeshLinkID;

						MNM::LinkAdditionRequest request(smartObjectEntityId, targetMeshID, linkData, linkID);
						request.SetCallback(functor(linkIDList, &OffMeshNavigationManager::OffMeshLinkIDList::OnLinkAdditionRequestForSmartObjectServiced));
						QueueCustomLinkAddition(request);

						// Prevent duplicate registration
						registeredLinks.push_back(CRegisterSOHelper::SHelpersLink(fromIndex, toIndex));
					}
				}
			}
		}
	}
}

void OffMeshNavigationManager::UnregisterSmartObjectForAllClasses(CSmartObject* pSmartObject)
{
	CSmartObjectClasses& classes = pSmartObject->GetClasses();
	CSmartObjectClasses::iterator it, itEnd = classes.end();

	for (it = classes.begin(); it != itEnd; ++it)
	{
		UnregisterSmartObject(pSmartObject, (*it)->GetName());
	}
}

void OffMeshNavigationManager::UnregisterSmartObject(CSmartObject* pSmartObject, const string& smartObjectClassName)
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
				MNM::LinkRemovalRequest request(objectId, linkList[i]);
				QueueCustomLinkRemoval(request);
			}

			// Remove object from registration
			objectIt->second.erase(classIt);
		}

		// If there aren't any classes remaining on this registered object, remove it.
		if (objectIt->second.size() == 0)
		{
			RemoveAllQueuedAdditionRequestForEntity(objectId);
			m_registeredObjects.erase(objectIt);
		}
	}
}

void OffMeshNavigationManager::RefreshConnections(const NavigationMeshID meshID, const MNM::TileID tileID)
{
	if (m_offMeshMap.validate(meshID))
	{
		MNM::OffMeshNavigation& offMeshNavigation = m_offMeshMap[meshID];
		NavigationMesh& mesh = gAIEnv.pNavigationSystem->GetMesh(meshID);

		//////////////////////////////////////////////////////////////////////////
		// Invalidate links for the tile.
		// Because the tile has been regenerated, all outgoing off-mesh links are invalid now
		offMeshNavigation.InvalidateLinks(tileID);

		//////////////////////////////////////////////////////////////////////////
		/// Refresh all links tied to this mesh and tile
		/// NOTE: This only re-adds them to the Tile with the cached data.
		TLinkInfoMap::iterator iter;
		for (iter = m_links.begin(); iter != m_links.end(); )
		{
			// Find out if object is bound to the modified mesh and tile
			SLinkInfo& linkInfo = iter->second;
			if (linkInfo.meshID != meshID)
			{
				++iter;
				continue;  // Mismatched mesh id
			}

			const bool isLinkOutgoingFromThisTile = (tileID == MNM::ComputeTileID(linkInfo.startTriangleID));
			const bool isLinkIncomingToThisTile = (tileID == MNM::ComputeTileID(linkInfo.endTriangleID));

			// is the link completely unrelated to given tile?
			if (!isLinkOutgoingFromThisTile && !isLinkIncomingToThisTile)
			{
				++iter;
				continue;
			}

			// is the link incoming from a different tile?
			// -> need to remove it before it can potentially become a dangling link (i. e. the triangleID where it ends would no longer exist or would have morphed into a different one)
			//    (in fact, it *is* already a dangling link because given tile has just been regenerated and all its triangleIDs most likely have changed!)
			//    Notice that InvalidateLinks(tileID) only cares about outgoing links and also doesn't remove the link-information from the NavMesh, so we need
			//    to call RemoveLink() to get the NavMesh patched as well.
			if (isLinkIncomingToThisTile && !isLinkOutgoingFromThisTile)
			{
				const MNM::OffMeshLinkID& linkID = iter->first;
				offMeshNavigation.RemoveLink(mesh, linkInfo.startTriangleID, linkID);
			}

			// disconnect whatever islands were connected by the offmesh-link
			// TODO pavloi 2016.02.05: actually, it will remove all links, which have the same owner as this link, from same meshID.
			// We probably should call RemoveOffMeshLinkIslandsConnectionBetweenTriangles(), but we need to know start and end triangles to
			// get the start and end island. Right now, I'm not sure, whether the triangleId's in linkInfo are valid at this stage, so I leave this
			// call as it is.
			gAIEnv.pNavigationSystem->RemoveAllIslandConnectionsForObject(linkInfo.meshID, linkInfo.offMeshLink->GetEntityIdForOffMeshLink());

			// Try to re-connect everything later on, but only if it was not explicitly requested to be deleted by an outside source.
			// For example: if, at the exact same update the tile was refreshed but an outside source also requested the explicit
			// removal of a link then we should basically not re-apply the link.
			const MNM::OffMeshLinkID& linkID = iter->first;
			if (!IsLinkRemovalRequested(linkID))
			{
				MNM::LinkAdditionRequest request(linkInfo.offMeshLink->GetEntityIdForOffMeshLink(), meshID, linkInfo.offMeshLink, linkID);

				// Link data was already cloned when it was added the first time. If we clone it again, the game-side of the link have no way
				// to know, that link data was cloned (there is no event about refreshed connections). That means, if the game-side holds a
				// pointer to the link, it will become dangling and will lead to crash.
				request.bCloneLinkData = false;

				OffMeshLinkIDList* linkIDList = GetClassInfoFromLinkInfo(linkInfo);
				if (linkIDList)
				{
					//stored linkID is removed from OffMeshLinkIDList in callback, when the link cannot be added again.
					request.SetCallback(functor(*linkIDList, &OffMeshNavigationManager::OffMeshLinkIDList::OnLinkRepairRequestForSmartObjectServiced));
				}
				QueueCustomLinkAddition(request);
			}

			// But for the meantime, also remove the entry from m_links because the .triangleID member was referencing a triangle in the now regenerated tile (!)
			//
			// Carefully read this:
			//
			//    If we didn't do this, then remove-link requests and add-link requests in the m_operations queue could use that "dead" triangle reference
			//    to try to remove offmesh-links in the NavMesh where there was none, OR try to add offmesh-links in the NavMesh where there was one already, OR
			//    (even worse, as no assert will catch this scenario) it would manipulate *foreign* offmesh-links in the NavMesh.
			//
			m_links.erase(iter++);
		}

		std::vector<EntityId> tempObjectIds;
		tempObjectIds.reserve(m_registeredObjects.size());

		//////////////////////////////////////////////////////////////////////////
		/// Find object's smart classes which were unable to register before to be
		/// registered again because of the change
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

		//////////////////////////////////////////////////////////////////////////
		/// Register again those objects which could have been affected by the change
		for (auto objectIt = tempObjectIds.cbegin(), iterEnd = tempObjectIds.cend(); objectIt != iterEnd; ++objectIt)
		{
			CSmartObject* pSmartObject = CSmartObjectManager::GetSmartObject(*objectIt);
			assert(pSmartObject);
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
}

OffMeshNavigationManager::OffMeshLinkIDList* OffMeshNavigationManager::GetClassInfoFromLinkInfo(const SLinkInfo& linkInfo)
{
	EntityId entityId = linkInfo.offMeshLink->GetEntityIdForOffMeshLink();
	TRegisteredObjects::iterator objectIt = m_registeredObjects.find(entityId);
	if (objectIt != m_registeredObjects.end())
	{
		TSOClassInfo* classInfos = &objectIt->second;
		for (TSOClassInfo::iterator classIt = classInfos->begin(); classIt != classInfos->end(); ++classIt)
		{
			const OffMeshLinkIDList::TLinkIDList& linkList = classIt->second.GetLinkIDList();
			for (size_t i = 0; i < linkList.size(); ++i)
			{
				if (linkList[i] == linkInfo.offMeshLink->GetLinkId())
				{
					return &classIt->second;
				}
			}
		}
	}
	CRY_ASSERT_MESSAGE(linkInfo.offMeshLink->GetLinkType() != MNM::OffMeshLink::eLinkType_SmartObject, "Offmesh Link for entity id %u doesn't have SmartObject assigned!", entityId);
	return nullptr;
}

void OffMeshNavigationManager::Clear()
{
	m_offMeshMap.clear();
	m_registeredObjects.clear();
	m_links.clear();
	m_objectRegistrationEnabled = false;

	stl::free_container(m_operations);
}

void OffMeshNavigationManager::Enable()
{
	m_objectRegistrationEnabled = true;
}

void OffMeshNavigationManager::OnNavigationMeshCreated(const NavigationMeshID& meshID)
{
	assert(!m_offMeshMap.validate(meshID));

	m_offMeshMap.insert(meshID, MNM::OffMeshNavigation());
}

void OffMeshNavigationManager::OnNavigationMeshDestroyed(const NavigationMeshID& meshID)
{
	assert(m_offMeshMap.validate(meshID));

	m_offMeshMap.erase(meshID);

	//////////////////////////////////////////////////////////////////////////
	/// Remove all existing links for this mesh
	OffMeshLinkIDList::TLinkIDList removedLinkIDs;
	TLinkInfoMap::iterator iter;
	for (iter = m_links.begin(); iter != m_links.end(); )
	{
		// Find out if object is bound to the modified mesh and tile
		SLinkInfo& linkInfo = iter->second;
		if (linkInfo.meshID != meshID)
		{
			++iter;
			continue;  // Mismatched mesh id
		}

		const MNM::OffMeshLinkID& linkID = iter->first;
		removedLinkIDs.push_back(linkID);

		NotifyAllListenerAboutLinkDeletion(linkID);

		m_links.erase(iter++);
	}

	//////////////////////////////////////////////////////////////////////////
	/// Remove smart object references to removed link IDs
	for (TRegisteredObjects::iterator objectIt = m_registeredObjects.begin(); objectIt != m_registeredObjects.end(); ++objectIt)
	{
		for (TSOClassInfo::iterator classInfoIt = objectIt->second.begin(); classInfoIt != objectIt->second.end(); ++classInfoIt)
		{
			OffMeshLinkIDList::TLinkIDList& linkList = classInfoIt->second.GetLinkIDList();
			for (int i = 0; i < removedLinkIDs.size(); ++i)
			{
				stl::find_and_erase(linkList, removedLinkIDs[i]);
			}
		}
	}
}

void OffMeshNavigationManager::OnNavigationLoadedComplete()
{
	//////////////////////////////////////////////////////////////////////////
	//Only after the navigation loaded process is completed off-mesh links can be created
	//Off-mesh links are created after the mesh is loaded, they are not stored within the mesh when exported
	Enable();
}

bool OffMeshNavigationManager::ObjectRegistered(const EntityId objectId, const string& smartObjectClassName) const
{
	uint32 smartObjectClassNameCrC = CCrc32::ComputeLowercase(smartObjectClassName.c_str());
	TRegisteredObjects::const_iterator itFoundObject = m_registeredObjects.find(objectId);
	return (itFoundObject != m_registeredObjects.end() && itFoundObject->second.find(smartObjectClassNameCrC) != itFoundObject->second.end());
}

bool OffMeshNavigationManager::IsObjectLinkedWithNavigationMesh(const EntityId objectId) const
{
	TRegisteredObjects::const_iterator objectIt = m_registeredObjects.find(objectId);

	return (objectIt != m_registeredObjects.end()) ? !objectIt->second.empty() : false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if DEBUG_MNM_ENABLED

void OffMeshNavigationManager::UpdateEditorDebugHelpers()
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

	TRegisteredObjects::const_iterator objectIt = m_registeredObjects.begin();
	TRegisteredObjects::const_iterator endIt = m_registeredObjects.end();

	const float alpha = clamp_tpl((1.0f + sinf(time)) * 0.5f, 0.25f, 0.7f);
	const ColorB color(255, 0, 0, (uint8)(alpha * 255));

	for (; objectIt != endIt; ++objectIt)
	{
		if (!objectIt->second.empty())
			continue;

		IEntity* pObjectEntity = gEnv->pEntitySystem->GetEntity(objectIt->first);
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

OffMeshNavigationManager::ProfileMemoryStats OffMeshNavigationManager::GetMemoryStats(ICrySizer* pSizer, const NavigationMeshID meshID) const
{
	const size_t initialSize = pSizer->GetTotalSize();
	ProfileMemoryStats memStats;

	// for all links: track memory of the ones with given mesh ID
	for (TLinkInfoMap::const_iterator it = m_links.begin(); it != m_links.end(); ++it)
	{
		const SLinkInfo& linkInfo = it->second;

		if (linkInfo.meshID == meshID)
		{
			const size_t sizeBeforeLinkInfo = pSizer->GetTotalSize();
			pSizer->AddObjectSize(&linkInfo);
			memStats.linkInfoSize += pSizer->GetTotalSize() - sizeBeforeLinkInfo;
		}
	}

	memStats.totalSize = pSizer->GetTotalSize() - initialSize;

	return memStats;
}

#endif
