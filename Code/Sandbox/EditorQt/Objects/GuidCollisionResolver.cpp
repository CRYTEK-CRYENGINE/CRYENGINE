// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GuidCollisionResolver.h"
#include "Objects/PrefabObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/ObjectManager.h"
#include "Prefabs/PrefabManager.h"

namespace Private_GuidCollisionResolver
{ 

CPrefabItem* FindPrefabItemContaining(CBaseObject* pObject)
{
	auto pEnum = GetIEditorImpl()->GetPrefabManager()->GetItemEnumerator();
	auto pPrefabItem = static_cast<CPrefabItem*>(pEnum->GetFirst());
	while (pPrefabItem)
	{
		if (pPrefabItem->FindObjectByGuid(pObject->GetIdInPrefab()))
		{
			break;
		}
		pPrefabItem = static_cast<CPrefabItem*>(pEnum->GetNext());
	}
	return pPrefabItem;
}

CPrefabObject* FindPrefabObjectContaining(CBaseObject* pObject)
{
	// we basically have to do this because the object that is conflicting is not registered nor initialized yet and therefore 
	// it has no parent, children and prefab references.
	auto pPrefabItem = FindPrefabItemContaining(pObject);
	if (!pPrefabItem)
	{
		return nullptr;
	}
	std::vector<CPrefabObject*> prefabs;
	GetIEditorImpl()->GetPrefabManager()->GetPrefabObjects(prefabs);
	for (auto pPrefabObject : prefabs)
	{
		if (pPrefabObject->GetPrefabItem() == pPrefabItem && CPrefabChildGuidProvider::IsValidChildGUid(pObject->GetId(), pPrefabObject))
		{
			return pPrefabObject;
		}
	}
	return nullptr;
}

CPrefabObject* FindTopMostPrefab(CPrefabObject* pPrefabObject)
{
	while (auto pTmpPrefab = FindPrefabObjectContaining(pPrefabObject))
	{
		pPrefabObject = pTmpPrefab;
	}
	return pPrefabObject;
}

}

bool CGuidCollisionResolver::Resolve()
{
	using namespace Private_GuidCollisionResolver;

	// we don't know which one of 2 colliding objects belongs to a prefab.
	// so first we try to find prefab containing the object
	CPrefabObject* pPrefabObject = FindPrefabObjectContaining(m_pConflictingObject);
	// otherwise we try to fetch from the object that current object collides with
	if (!pPrefabObject)
	{
		// current object  has illegal (colliding) guid that belongs to another object. So we find that object first
		auto pObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_pConflictingObject->GetId());
		pPrefabObject = static_cast<CPrefabObject*>(pObject->GetPrefab());
	}
	if (pPrefabObject)
	{
		auto pTopMostPrefab = FindTopMostPrefab(pPrefabObject);
		// we need to save whole hierarchy in the beginning because once we start changing guids of prefabs it won't be possible to find
		// theirs children anymore by guids.
		SavePrefabHierarchy(pTopMostPrefab);
		int i = 5;
		while (!RegenerateGuids(pTopMostPrefab, pPrefabObject) && --i != 0) {}
		return i != 0;
	}
	return false;
}

bool CGuidCollisionResolver::RegenerateGuids(CPrefabObject* pTopMostPrefab, CPrefabObject* pPrefabObject)
{
	using namespace Private_GuidCollisionResolver;

	CryGUID newPrefabGuid = CRandomUniqueGuidProvider().GetFor(pTopMostPrefab);
	GetIEditor()->GetObjectManager()->ChangeObjectId(pTopMostPrefab->GetId(), newPrefabGuid);
	if (!RegenerateGuidsForPrefabHierarchy(pTopMostPrefab))
	{
		return false;
	}

	// now try to resolve the conflict for conflicting object itself. Note: we don't know here if it belongs to the prefab
	return RegenerateGuidsForConflictingObject(pPrefabObject);
}

void CGuidCollisionResolver::SavePrefabHierarchy(CPrefabObject* pPrefabObject)
{
	std::deque<CPrefabObject*> prefabs = { pPrefabObject };
	while (!prefabs.empty())
	{
		auto pPrefabParent = prefabs.front();
		prefabs.pop_front();

		auto pair = std::make_pair(pPrefabParent, GetPrefabChildren(pPrefabParent));
		m_hierarchy.push_back(pair);

		for (auto pChild : pair.second)
		{
			if (pChild->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				auto pPrefabChild = static_cast<CPrefabObject*>(pChild);
				// some prefab children of pPrefabParent might not have been initialized yet, so they won't have a reference to 
				// the prefab item. These prefabs we treat as normal children. Once guids resolved, initialization will continue
				// and the children on this skipped prefab child will get correct guids.
				if (pPrefabChild->GetPrefabItem())
				{
					prefabs.push_back(pPrefabChild);
				}
			}
		}
	}
}

const std::vector<CBaseObject*>& CGuidCollisionResolver::GetSavedChildren(CPrefabObject* pPrefabObject)
{
	auto it = std::find_if(m_hierarchy.cbegin(), m_hierarchy.cend(), [pPrefabObject](const std::pair<CPrefabObject*, std::vector<CBaseObject*>>& pair)
	{
		return pair.first == pPrefabObject;
	});

	CRY_ASSERT_MESSAGE(it != m_hierarchy.cend(), "No saved children found for prefab");

	return it->second;
}

bool CGuidCollisionResolver::RegenerateGuidsForPrefabHierarchy(CPrefabObject* pPrefabObject)
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	const auto& prefabChildren = GetSavedChildren(pPrefabObject);
	CPrefabChildGuidProvider guidProvider = { pPrefabObject };

	for (auto pObject : prefabChildren)
	{
		CryGUID newGuid = guidProvider.GetFor(pObject);
		if (pObjectManager->FindObject(newGuid))
		{
			return false;
		}
		m_archive.RemapID(pObject->GetIdInPrefab(), newGuid);
		pObjectManager->ChangeObjectId(pObject->GetId(), newGuid);
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			if (!RegenerateGuidsForPrefabHierarchy(static_cast<CPrefabObject*>(pObject)))
			{
				return false;
			}
		}
	}
	return true;
}

std::vector<CBaseObject*> CGuidCollisionResolver::GetPrefabChildren(CPrefabObject* pPrefabObject)
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	XmlNodeRef objectsXml = pPrefabObject->GetPrefabItem()->GetObjectsNode();

	std::vector<CBaseObject*> prefabChildren;
	CPrefabChildGuidProvider guidProvider = { pPrefabObject };
	int numObjects = objectsXml->getChildCount();
	for (int i = 0; i < numObjects; i++)
	{
		XmlNodeRef objNode = objectsXml->getChild(i);

		CryGUID id = CryGUID::Null();
		if (!objNode->getAttr("Id", id))
		{
			continue;
		}
		CryGUID childGuid = guidProvider.GetFrom(id);
		if (childGuid == m_pConflictingObject->GetId())
		{
			continue;
		}
		if (auto pChild = pObjectManager->FindObject(childGuid))
		{
			prefabChildren.push_back(pChild);
		}
	}

	return prefabChildren;
}

bool CGuidCollisionResolver::RegenerateGuidsForConflictingObject(CPrefabObject* pPrefabObject)
{
	using namespace Private_GuidCollisionResolver;
	// if conflicting object is part of prefab, then we generate new guid
	if (FindPrefabItemContaining(m_pConflictingObject))
	{
		CPrefabChildGuidProvider guidProvider = { pPrefabObject };
		m_pConflictingObject->SetId(guidProvider.GetFor(m_pConflictingObject));
		if (GetIEditorImpl()->GetObjectManager()->FindObject(m_pConflictingObject->GetId()))
		{
			return false;
		}
		m_archive.RemapID(m_pConflictingObject->GetIdInPrefab(), m_pConflictingObject->GetId());
	}
	// otherwise it's guid is not conflicting anymore since the object it was conflicting with has a new guid
	return true;
}

