// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Group.h"

#include "Objects/EntityObject.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/PrefabObject.h"
#include "CryEdit.h"
#include "IEditorImpl.h"

#include <Util/MFCUtil.h>

#include <Objects/DisplayContext.h>
#include <Objects/InspectorWidgetCreator.h>
#include <Objects/ObjectLoader.h>
#include <Preferences/SnappingPreferences.h>
#include <Preferences/ViewportPreferences.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <Serialization/Decorators/EditToolButton.h>
#include <Viewport.h>

#include <Controls/QuestionDialog.h>
#include <Util/Math.h>

#include <QDialogButtonBox>
#include <algorithm>

REGISTER_CLASS_DESC(CGroupClassDesc);
IMPLEMENT_DYNCREATE(CGroup, CBaseObject)

namespace Private_Group
{
void RemoveIfAlreadyChildrenOf(CBaseObject* pParent, std::vector<CBaseObject*>& objects)
{
	objects.erase(std::remove_if(objects.begin(), objects.end(), [pParent](CBaseObject* pObj)
		{
			return pObj->GetParent() == pParent;
		}), objects.end());
}

void ResetTransforms(CGroup* pParent, const std::vector<CBaseObject*>& children, bool shouldKeepPos, std::vector<Matrix34>& worldTMs,
                     std::vector<ITransformDelegate*>& transformDelegates)
{
	CScopedSuspendUndo suspendUndo;

	for (auto pChild : children)
	{
		transformDelegates.emplace_back(pChild->GetTransformDelegate());
		pChild->SetTransformDelegate(nullptr, true);
	}
	if (shouldKeepPos)
	{
		ITransformDelegate* pTransformDelegate = pParent->GetTransformDelegate();
		pParent->SetTransformDelegate(nullptr, true);
		for (auto pChild : children)
		{
			// TODO: check if possible to hold only pointers to matrices
			worldTMs.emplace_back(pChild->GetWorldTM());
		}
		pParent->SetTransformDelegate(pTransformDelegate, true);
	}
}

void RestoreTransforms(std::vector<CBaseObject*>& children, bool shouldKeepPos, const std::vector<Matrix34>& worldTMs,
                       const std::vector<ITransformDelegate*>& transformDelegates)
{
	CScopedSuspendUndo suspendUndoRestore;
	if (shouldKeepPos)
	{
		// Keep old world space transformation.
		for (int i = 0; i < children.size(); ++i)
		{
			children[i]->SetWorldTM(worldTMs[i]);
		}
	}

	for (int i = 0; i < children.size(); ++i)
	{
		children[i]->SetTransformDelegate(transformDelegates[i], true);
	}
}

std::vector<IObjectLayer*> GetObjectsLayers(const std::vector<CBaseObject*>& children)
{
	std::vector<IObjectLayer*> layers;
	layers.reserve(children.size());
	for (auto pChild : children)
	{
		assert(pChild);
		layers.push_back(pChild->GetLayer());
	}
	return layers;
}

class CBatchAttachChildrenTransformationsHandler
{
public:
	CBatchAttachChildrenTransformationsHandler(CGroup* pParent, const std::vector<CBaseObject*>& children, bool shouldKeepPos,
	                                           bool shouldInvalidateTM)
		: m_pParent(pParent)
		, m_children(children)
		, m_shouldKeepPos(shouldKeepPos)
		, m_shouldInvalidateTM(shouldInvalidateTM)
	{
	}

	void HandlePreAttach()
	{
		ResetTransformsWithNewParent();
	}

	void HandleAttach()
	{
		RestoreTransformsWithNewParent();
	}

private:
	void ResetTransformsWithNewParent()
	{
		m_transformDelegates.emplace_back(m_pParent->GetTransformDelegate());
		m_pParent->SetTransformDelegate(nullptr, m_shouldInvalidateTM);

		Matrix34 invParentTM = m_pParent->GetWorldTM();
		invParentTM.Invert();
		for (auto pChild : m_children)
		{
			m_transformDelegates.emplace_back(pChild->GetTransformDelegate());
			pChild->SetTransformDelegate(nullptr, m_shouldInvalidateTM);
			if (m_shouldKeepPos)
			{
				pChild->InvalidateTM(0);
			}
			m_pParent->SetChildsParent(pChild);
			if (m_shouldKeepPos)
			{
				// Set the object's local transform relative to it's new parent
				pChild->SetLocalTM(invParentTM * pChild->GetWorldTM());
			}
		}
	}

	void RestoreTransformsWithNewParent()
	{
		// Make sure world transform is recalculated
		if (!m_shouldKeepPos)
		{
			for (auto pChild : m_children)
			{
				pChild->InvalidateTM(0);
			}
		}

		m_pParent->SetTransformDelegate(m_transformDelegates[0], m_shouldInvalidateTM);
		for (int i = 1; i < m_transformDelegates.size(); ++i)
		{
			m_children[i - 1]->SetTransformDelegate(m_transformDelegates[i], m_shouldInvalidateTM);
		}
	}

	CGroup*                          m_pParent;
	const std::vector<CBaseObject*>& m_children;
	bool                             m_shouldKeepPos;
	bool                             m_shouldInvalidateTM;
	std::vector<ITransformDelegate*> m_transformDelegates;
};

class CUndoAttachmentHelper : public IUndoObject
{
protected:
	std::vector<CryGUID> GetObjectsLayerGuids(const std::vector<CBaseObject*>& objects)
	{
		std::vector<CryGUID> layerGUIDs;
		layerGUIDs.reserve(objects.size());
		for (auto pObject : objects)
		{
			CRY_ASSERT(pObject);
			layerGUIDs.push_back(pObject->GetLayer()->GetGUID());
		}
		return layerGUIDs;
	}

	std::vector<CryGUID> GetObjectsParentGuids(const std::vector<CBaseObject*>& objects)
	{
		std::vector<CryGUID> parentGUIDs;
		parentGUIDs.reserve(objects.size());
		for (auto pObject : objects)
		{
			CRY_ASSERT(pObject);
			CBaseObject* pParent = pObject->GetParent();
			if (pParent)
				parentGUIDs.push_back(pParent->GetId());
			else
				parentGUIDs.push_back(CryGUID());
		}
		return parentGUIDs;
	}

	void RemoveMembersToRootHelper(std::vector<CBaseObject*> objects, std::vector<CryGUID>& oldLayersGuids, bool shouldKeepTransform)
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		CGroup::ForEachParentOf(objects, [pObjectManager, oldLayersGuids, shouldKeepTransform](CBaseObject* pParent, std::vector<CBaseObject*>& children)
		{
			CGroup* pGroup = static_cast<CGroup*>(pParent);

			if (!pGroup)
				return;

			pGroup->RemoveMembers(children, shouldKeepTransform, true);

			for (int i = 0; i < children.size(); ++i)
			{
				children[i]->SetLayer(pObjectManager->GetIObjectLayerManager()->FindLayer(oldLayersGuids[i]));
				children[i]->UpdatePrefab();
			}
		});
	}
	void Detach(const std::vector<CryGUID>& objectsGuids, bool shouldKeepTransform, bool shouldPlaceOnRoot)
	{
		if (objectsGuids.empty())
		{
			return;
		}

		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		std::vector<CBaseObject*> objects;
		objects.reserve(objectsGuids.size());
		for (auto it = objectsGuids.rbegin(); it != objectsGuids.rend(); ++it)
		{
			CBaseObject* pObject = pObjectManager->FindObject(*it);
			if (pObject)
			{
				objects.emplace_back(pObject);
			}
		}
		CGroup::ForEachParentOf(objects, [shouldKeepTransform, shouldPlaceOnRoot](CGroup* pGroup, std::vector<CBaseObject*>& children)
		{
			pGroup->RemoveMembers(children, shouldKeepTransform, shouldPlaceOnRoot);
		});
	}

	void Attach(const std::vector<CryGUID>& objectsGuids, const CryGUID& attachToGuid, bool shouldKeepTransform)
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		std::vector<CBaseObject*> objects;
		objects.reserve(objectsGuids.size());
		for (const auto& objectGuid : objectsGuids)
		{
			CBaseObject* pObject = pObjectManager->FindObject(objectGuid);
			if (pObject)
			{
				objects.emplace_back(pObject);
			}

		}
		CBaseObject* pParentObject = pObjectManager->FindObject(attachToGuid);

		if (pParentObject && !objects.empty())
		{
			assert(GetIEditor()->IsCGroup(pParentObject));
			static_cast<CGroup*>(pParentObject)->AddMembers(objects, shouldKeepTransform);
		}
	}

	void Attach(const std::vector<CryGUID>& objectsGuids, std::vector<CryGUID>& parentGuids, std::vector<CryGUID>& oldLayersGuids, bool shouldKeepTransform)
	{
		if (objectsGuids.empty())
		{
			return;
		}

		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		std::vector<CBaseObject*> objects;
		objects.reserve(objectsGuids.size());
		for (auto it = objectsGuids.rbegin(); it != objectsGuids.rend(); ++it)
		{
			CBaseObject* pObject = pObjectManager->FindObject(*it);
			if (pObject)
			{
				objects.emplace_back(pObject);
			}
			else
			{
				oldLayersGuids.erase(std::next(it).base());
				parentGuids.erase(std::next(it).base());
			}
		}

		CRY_ASSERT_MESSAGE(parentGuids.size() == objects.size(), "Non-matching number of old parents to detach to and objects to detach");

		std::unordered_map<CGroup*, std::vector<CBaseObject*>> parentToChildrenMap;
		std::vector<CBaseObject*> rootObjects;
		rootObjects.reserve(objectsGuids.size());
		std::vector<CryGUID> rootObjectLayerGuids;
		rootObjectLayerGuids.reserve(objectsGuids.size());

		for (auto i = 0; i < parentGuids.size(); ++i)
		{
			if (parentGuids[i].IsNull())
			{
				rootObjects.push_back(objects[i]);
				rootObjectLayerGuids.push_back(oldLayersGuids[i]);
				continue;
			}

			CGroup* pGroup = static_cast<CGroup*>(pObjectManager->FindObject(parentGuids[i]));
			parentToChildrenMap[pGroup].push_back(objects[i]);
		}

		RemoveMembersToRootHelper(rootObjects, rootObjectLayerGuids, shouldKeepTransform);

		for (auto& parentToChildrenPair : parentToChildrenMap)
		{
			auto pGroup = parentToChildrenPair.first;
			std::vector<CBaseObject*>& children = parentToChildrenPair.second;

			pGroup->AddMembers(children, shouldKeepTransform);
		}
	}
};

class CUndoAttach : public CUndoAttachmentHelper
{
public:
	CUndoAttach(const std::vector<CBaseObject*>& objects, const CBaseObject* pParent,  bool shouldKeepPos)
		: m_shouldKeepPos(shouldKeepPos)
	{
		CRY_ASSERT_MESSAGE(pParent, "Invalid parent");
		CRY_ASSERT_MESSAGE(!objects.empty(), "Can't create attachment undo for 0 objects");

		m_parentGuid = pParent->GetId();
		m_description.Format("Objects Attached to %s", pParent->GetName());
		m_oldLayersGuids = GetObjectsLayerGuids(objects);
		m_oldParentGuids = GetObjectsParentGuids(objects);

		// Default place on root on undo
		m_objectsGuids.reserve(objects.size());
		for (auto pObject : objects)
		{
			m_objectsGuids.emplace_back(pObject->GetId());
		}
	}

	virtual void Undo(bool bUndo) override
	{
		// Call attach with all old parents since children could've been attached from multiple different parents on initial execution
		Attach(m_objectsGuids, m_oldParentGuids, m_oldLayersGuids, m_shouldKeepPos);
	}

	virtual void Redo() override
	{
		// Re-Attach to parent
		Attach(m_objectsGuids, m_parentGuid, m_shouldKeepPos);
	}

	virtual const char* GetDescription() { return m_description; }

private:
	std::vector<CryGUID> m_objectsGuids;
	std::vector<CryGUID> m_oldLayersGuids;
	std::vector<CryGUID> m_oldParentGuids;
	string               m_description;
	CryGUID              m_parentGuid;
	bool                 m_shouldKeepPos;
};

class CUndoDetach : public CUndoAttachmentHelper
{
public:
	CUndoDetach(const std::vector<CBaseObject*>& objects, bool shouldKeepPos, bool shouldPlaceOnRoot)
		: m_shouldKeepPos(shouldKeepPos)
		, m_shouldPlaceOnRoot(shouldPlaceOnRoot)
	{
		CRY_ASSERT_MESSAGE(!objects.empty(), "Can't create attachment undo for 0 objects");
		m_oldLayersGuids = GetObjectsLayerGuids(objects);
		m_oldParentGuids = GetObjectsParentGuids(objects);

		// Default place on root on undo
		m_objectsGuids.reserve(objects.size());
		for (auto pObject : objects)
		{
			m_objectsGuids.emplace_back(pObject->GetId());
		}
	}

	virtual void Undo(bool bUndo) override
	{
		Attach(m_objectsGuids, m_oldParentGuids, m_oldLayersGuids, m_shouldKeepPos);
	}

	virtual void Redo() override
	{
		Detach(m_objectsGuids, m_shouldKeepPos, m_shouldPlaceOnRoot);
	}

	virtual const char* GetDescription() { return "Objects Detached"; }

private:
	std::vector<CryGUID> m_objectsGuids;
	std::vector<CryGUID> m_oldLayersGuids;
	std::vector<CryGUID> m_oldParentGuids;
	bool                 m_shouldKeepPos;
	bool                 m_shouldPlaceOnRoot;
};
}

//////////////////////////////////////////////////////////////////////////
CGroup::CGroup()
{
	m_opened = false;
	m_bAlwaysDrawBox = true;
	m_ignoreChildModify = false;
	m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	m_bBBoxValid = false;
	m_bUpdatingPivot = false;
	SetColor(ColorB(0, 255, 0)); // Green
}

void CGroup::Done()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, GetName().c_str());
	DeleteAllMembers();
	CBaseObject::Done();
}

void CGroup::DeleteAllMembers()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)
	std::vector<CBaseObject*> children;
	for (auto pChild : m_children)
		children.push_back(pChild);

	// Must collect all linked objects since they're not really considered members of the group
	size_t childCount = m_children.size(); // cache current size
	for (size_t i = 0; i < childCount; ++i)
		GetAllLinkedObjects(children, m_children[i]);

	GetObjectManager()->DeleteObjects(children);
}

void CGroup::GetAllLinkedObjects(std::vector<CBaseObject*>& objects, CBaseObject* pObject)
{
	for (auto i = 0; i < pObject->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObject->GetLinkedObject(i);
		if (pLinkedObject)
		{
			objects.push_back(pLinkedObject);
			GetAllLinkedObjects(objects, pLinkedObject);
		}
	}
}

void CGroup::ForEachParentOf(const std::vector<CBaseObject*>& objects, std::function<void(CGroup*, std::vector<CBaseObject*>&)> func)
{
	std::unordered_map<CGroup*, std::vector<CBaseObject*>> parentToChildrenMap;
	for (auto pObj : objects)
	{
		auto pParent = pObj->GetParent();
		auto pGroup = static_cast<CGroup*>(pParent);
		assert(!pParent || pGroup);
		parentToChildrenMap[pGroup].push_back(pObj);
	}

	for (auto& parentToChildrenPair : parentToChildrenMap)
	{
		func(parentToChildrenPair.first, parentToChildrenPair.second);
	}
}

bool CGroup::CanCreateFrom(std::vector<CBaseObject*>& objects)
{
	if (objects.empty())
		return false;

	const auto lastIdx = objects.size() - 1;
	IObjectLayer* pDestLayer = objects[lastIdx]->GetLayer();

	for (auto i = 0; i < objects.size(); ++i)
	{
		if (!objects[i]->AreLinkedDescendantsInLayer(pDestLayer))
		{
			string message;
			message.Format("The objects you are trying to group objects from different layers. All objects will be moved to %s layer\n\n"
			               "Do you want to continue?", pDestLayer->GetName());

			if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(message)))
			{
				return false;
			}

			return true;
		}
	}

	return true;
}

bool CGroup::CreateFrom(std::vector<CBaseObject*>& objects)
{
	// Put the newly created group on the last selected object's layer
	if (objects.empty())
	{
		return false;
	}

	CBaseObject* pLastSelectedObject = objects[objects.size() - 1];
	GetIEditorImpl()->GetIUndoManager()->Suspend();
	SetLayer(pLastSelectedObject->GetLayer());
	GetIEditorImpl()->GetIUndoManager()->Resume();

	//add ourselves to the last selected group
	if (CBaseObject* pLastParent = pLastSelectedObject->GetGroup())
		pLastParent->AddMember(this);

	// Prefab support
	CPrefabObject* pPrefabToCompareAgainst = nullptr;
	CPrefabObject* pObjectPrefab = nullptr;

	for (auto pObject : objects)
	{
		// Prefab handling
		pObjectPrefab = (CPrefabObject*)pObject->GetPrefab();

		// Sanity check if user is trying to group objects from different prefabs
		if (pPrefabToCompareAgainst && pObjectPrefab && pPrefabToCompareAgainst->GetPrefabGuid() != pObjectPrefab->GetPrefabGuid())
		{
			return false;
		}

		if (!pPrefabToCompareAgainst)
			pPrefabToCompareAgainst = pObjectPrefab;
	}

	AddMembers(objects);

	GetIEditorImpl()->GetObjectManager()->SelectObject(this);
	GetIEditorImpl()->SetModifiedFlag();
	return true;
}

CGroup* CGroup::CreateFrom(std::vector<CBaseObject*>& objects, Vec3 center)
{
	if (objects.empty())
	{
		return nullptr;
	}

	CUndo undo("Create Group");
	CGroup* pGroup = (CGroup*)GetIEditorImpl()->NewObject("Group");
	if (!pGroup)
	{
		undo.Cancel();
		return nullptr;
	}

	if (CBaseObject* pLastParent = objects[objects.size() - 1]->GetGroup())
	{
		Matrix34 m = pLastParent->GetWorldTM();
		m.Invert();
		center = m.TransformPoint(center);
	}
	// Snap center to grid.
	pGroup->SetPos(gSnappingPreferences.Snap3D(center));

	if (!pGroup->CreateFrom(objects))
	{
		undo.Cancel();
		GetIEditorImpl()->DeleteObject(pGroup);
		return nullptr;
	}

	return pGroup;
}

bool CGroup::Init(CBaseObject* prev, const string& file)
{
	bool res = CBaseObject::Init(prev, file);
	if (prev)
	{
		InvalidateBBox();

		CGroup* prevObj = (CGroup*)prev;

		if (prevObj)
		{
			if (prevObj->IsOpen())
			{
				Open();
			}
		}
	}
	return res;
}

void CGroup::AddMember(CBaseObject* pMember, bool keepPos)
{
	std::vector<CBaseObject*> members = { pMember };
	AddMembers(members, keepPos);
}

void CGroup::AddMembers(std::vector<CBaseObject*>& objects, bool keepPos /*= true*/)
{
	using namespace Private_Group;

	RemoveIfAlreadyChildrenOf(this, objects);

	if (objects.empty())
	{
		return;
	}

	auto batchProcessDispatcher = GetObjectManager()->GetBatchProcessDispatcher();
	{
		std::vector<CBaseObject*> batchObjects;
		for (auto pObject : objects)
		{
			batchObjects.push_back(pObject);
		}
		// Add current group to array of batched objects. Since this object can be in a different layer,
		// we want to make sure to batch process in both the detaching objects' layers and the new parent's layer (this)
		batchObjects.push_back(this);
		batchProcessDispatcher.Start(batchObjects);
	}

	//If the object is in another prefab it needs to be removed from it (aka deserialize from CPrefabItem)
	//CASES:
	//1 - from top level of a prefab to a group : Needs remove from prefab and add to group  (he'll be the one to serialize)
	//2 - from different groups in same prefab : Needs no remove from prefab, but remove from group
	//3 - from another prefab : Needs remove from prefab
	//4 - from outside group to inside group in prefab : Needs remove from old group and add to new group in prefab
	{
		// Don't record the detach undos since attach undo will take care of this anyways
		CScopedSuspendUndo suspendUndo;
		ForEachParentOf(objects, [keepPos](CGroup* pParent, std::vector<CBaseObject*>& children)
		{
			if (pParent)
			{
				pParent->RemoveMembers(children, true, true);
			}
		});
	}

	//Attach children already provokes serialization into lib (i.e ModifyTransform), if the object comes from outside guids must be generated before
	//This needs to happen here or the prefab delete in the attach will mess up id generation
	if (CBaseObject* pPrefabObject = GetPrefab())
	{
		CPrefabObject* pPrefab = static_cast<CPrefabObject*>(pPrefabObject);
		for (auto pObj : objects)
		{
			pPrefab->GenerateGUIDsForObjectAndChildren(pObj);
		}
	}

	//Actually attach the objects to the group instance
	AttachChildren(objects, keepPos);

	//If in a prefab re serialize the group and regenerate all the members in all the group's instances
	UpdatePrefab(eOCOT_Modify);

	InvalidateBBox();
}

void CGroup::RemoveMember(CBaseObject* pMember, bool keepPos, bool placeOnRoot)
{
	std::vector<CBaseObject*> members = { pMember };
	RemoveMembers(members, keepPos, placeOnRoot);
}

void CGroup::RemoveMembers(std::vector<CBaseObject*>& members, bool keepPos /*= true*/, bool placeOnRoot /*= false*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)
	CBaseObject* pPrefab = GetPrefab();
	if (pPrefab != this)
	{
		DetachChildren(members, keepPos, placeOnRoot);
	}

	UpdateGroup();

	//Since we removed an item the group needs to be re serialized into the prefab item
	UpdatePrefab(eOCOT_Modify);

	//if we move out from prefab the prefab flag needs to be cleared.
	for (CBaseObject* pObject : members)
	{
		if (!pObject->GetPrefab())
			pObject->ClearFlags(OBJFLAG_PREFAB);
	}
}

void CGroup::SerializeGeneralVisualProperties(Serialization::IArchive& ar, bool bMultiEdit)
{
	//Don't add any property to the archive, this will stop properties from showing in the ui
}

void CGroup::RemoveChild(CBaseObject* pChild)
{	
	RemoveChildren({ pChild });
}

void CGroup::GetBoundBox(AABB& box)
{
	if (!m_bBBoxValid)
		CalcBoundBox();

	box.SetTransformedAABB(GetWorldTM(), m_bbox);
}

void CGroup::GetLocalBounds(AABB& box)
{
	if (!m_bBBoxValid)
		CalcBoundBox();
	box = m_bbox;
}

bool CGroup::HitTest(HitContext& hc)
{
	bool selected = false;
	if (m_opened || hc.ignoreHierarchyLocks)
	{
		selected = HitTestMembers(hc);
	}

	if (!selected && (OBJTYPE_GROUP & gViewportSelectionPreferences.objectSelectMask))
	{
		Vec3 p;

		Matrix34 invertWTM = GetWorldTM();
		Vec3 worldPos = invertWTM.GetTranslation();
		invertWTM.Invert();

		Vec3 xformedRaySrc = invertWTM.TransformPoint(hc.raySrc);
		Vec3 xformedRayDir = invertWTM.TransformVector(hc.rayDir).GetNormalized();

		float epsilonDist = max(.1f, hc.view->GetScreenScaleFactor(worldPos) * 0.01f);
		epsilonDist *= max(0.0001f, min(invertWTM.GetColumn0().GetLength(), min(invertWTM.GetColumn1().GetLength(), invertWTM.GetColumn2().GetLength())));
		float hitDist;

		float tr = hc.distanceTolerance / 2 + 1;
		AABB boundbox, box;
		GetLocalBounds(boundbox);
		box.min = boundbox.min - Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);
		box.max = boundbox.max + Vec3(tr + epsilonDist, tr + epsilonDist, tr + epsilonDist);
		if (Intersect::Ray_AABB(xformedRaySrc, xformedRayDir, box, p))
		{
			if (Intersect::Ray_AABBEdge(xformedRaySrc, xformedRayDir, boundbox, epsilonDist, hitDist, p))
			{
				hc.dist = xformedRaySrc.GetDistance(p);
				hc.object = this;
				return true;
			}
			else
			{
				// Check if any children of closed group selected.
				if (!m_opened)
				{
					if (HitTestMembers(hc))
					{
						hc.object = this;
						return true;
					}
				}
			}
		}
	}

	return selected;
}

bool CGroup::HitHelperTestForChildObjects(HitContext& hc)
{
	bool result = false;

	if (m_opened)
	{
		for (int i = 0, iChildCount(GetChildCount()); i < iChildCount; ++i)
		{
			CBaseObject* pChild = GetChild(i);
			result = result || pChild->HitHelperTest(hc);
		}
	}

	return result;
}

bool CGroup::HitTestMembers(HitContext& hcOrg)
{
	float mindist = FLT_MAX;

	HitContext hc = hcOrg;

	CBaseObject* selected = 0;
	for (auto pChild : m_children)
	{
		if (GetObjectManager()->HitTestObject(pChild, hc))
		{
			if (hc.dist < mindist)
			{
				mindist = hc.dist;
				// If collided object specified, accept it, otherwise take tested object itself.
				if (hc.object)
					selected = hc.object;
				else
					selected = pChild;
				hc.object = 0;
			}
		}
	}
	if (selected)
	{
		hcOrg.object = selected;
		hcOrg.dist = mindist;
		return true;
	}
	return false;
}

void CGroup::OnContextMenu(CPopupMenuItem* menu)
{
	CBaseObject::OnContextMenu(menu);
}

void CGroup::Display(CObjectRenderHelper& objRenderHelper)
{
	SDisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (!dc.showGroupHelper)
	{
		return;
	}

	DrawDefault(dc, CMFCUtils::ColorBToColorRef(GetColor()));

	dc.PushMatrix(GetWorldTM());

	AABB boundbox;
	GetLocalBounds(boundbox);

	if (IsSelected())
	{
		dc.SetColor(ColorB(0, 255, 0, 255));
		dc.DrawWireBox(boundbox.min, boundbox.max);
		dc.DepthTestOff();
		dc.DepthWriteOff();
		ColorB color = GetColor();
		color.a = 38;
		dc.SetColor(color);
		dc.DrawSolidBox(boundbox.min, boundbox.max);
		dc.DepthWriteOn();
		dc.DepthTestOn();
	}
	else
	{
		if (m_bAlwaysDrawBox)
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor());
			dc.DrawWireBox(boundbox.min, boundbox.max);
		}
	}
	dc.PopMatrix();
}

const ColorB& CGroup::GetSelectionPreviewHighlightColor()
{
	return gViewportSelectionPreferences.colorGroupBBox;
}

void CGroup::DrawSelectionPreviewHighlight(SDisplayContext& dc)
{
	CBaseObject::DrawSelectionPreviewHighlight(dc);

	AABB bbox;
	GetBoundBox(bbox);

	dc.DepthTestOff();

	dc.SetColor(GetSelectionPreviewHighlightColor());

	dc.DrawSolidBox(bbox.min, bbox.max);
	dc.DepthTestOn();

	// Highlight also children objects if this object is opened
	if (!IsOpen())
		return;

	for (auto childIdx = 0; childIdx < GetChildCount(); ++childIdx)
	{
		GetChild(childIdx)->DrawSelectionPreviewHighlight(dc);
	}
}

void CGroup::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CBaseObject::CreateInspectorWidgets(creator);

	// Only show group options for groups that are not prefabs
	if (!GetIEditorImpl()->IsCPrefabObject(this))
	{
		creator.AddPropertyTree<CGroup>("Group", [](CGroup* pObject, Serialization::IArchive& ar, bool bMultiEdit)
		{
			if (ar.openBlock("group", "<Group"))
			{
			  ar(Serialization::ActionButton([]()
				{
					GetIEditor()->GetICommandManager()->Execute("group.ungroup");
				}), "ungroup", "^Ungroup");
			  if (pObject->IsOpen())
			  {
			    ar(Serialization::ActionButton([]()
					{
						GetIEditor()->GetICommandManager()->Execute("group.close");
					}), "close", "^Close");
				}
			  else
			  {
			    ar(Serialization::ActionButton([]()
					{
						GetIEditor()->GetICommandManager()->Execute("group.open");
					}), "open", "^Open");
				}
			  ar.closeBlock();
			}
		});
	}
}

bool CGroup::CanAddMembers(std::vector<CBaseObject*>& objects)
{
	for (CBaseObject* pObject : objects)
	{
		if (IsDescendantOf(pObject))
		{
			return false;
		}
	}

	return true;
}

bool CGroup::CanAddMember(CBaseObject* pMember)
{
	std::vector<CBaseObject*> members = { pMember };
	return CanAddMembers(members);
}

void CGroup::Serialize(CObjectArchive& ar)
{
	CBaseObject::Serialize(ar);

	if (ar.bLoading)
	{
		// Loading.
		bool isOpened = m_opened;
		ar.node->getAttr("Opened", isOpened);
		m_opened = isOpened;

		SerializeMembers(ar);
		if (ar.IsReconstructingPrefab())
			m_opened = false;
	}
	else
	{
		ar.node->setAttr("Opened", m_opened);
		SerializeMembers(ar);
	}
}

void CGroup::SerializeMembers(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		if (!ar.bUndo)
		{
			//If we are loading a group that's already full we need to clean it up and reinstance all it's members from the ground up
			for (auto pChild : m_children)
			{
				GetIEditor()->GetObjectManager()->DeleteObject(pChild);
			}
			m_children.clear();

			// Loading, reload all the children from XML
			XmlNodeRef childsRoot = xmlNode->findChild("Objects");
			if (childsRoot)
			{
				// Load all childs from XML archive.
				int numObjects = childsRoot->getChildCount();
				for (int i = 0; i < numObjects; ++i)
				{
					XmlNodeRef objNode = childsRoot->getChild(i);
					ar.LoadObject(objNode);
				}
				InvalidateBBox();
			}
		}
	}
	else
	{
		if (m_children.size() > 0 && !ar.bUndo)
		{
			// Saving.
			XmlNodeRef root = xmlNode->newChild("Objects");
			ar.node = root;

			// Save all child objects to XML.
			for (auto pChild : m_children)
			{
				ar.SaveObject(pChild, true, true);
			}
		}
	}
	ar.node = xmlNode;
}

void CGroup::Ungroup()
{
	StoreUndo("Ungroup");

	if (IsSelected())
	{
		GetObjectManager()->UnselectObject(this);
	}

	if (m_children.empty())
		return;

	std::vector<CBaseObject*> newSelection;
	newSelection.reserve(m_children.size());

	for (CBaseObject* pChild : m_children)
	{
		if (pChild)
		{
			newSelection.push_back(pChild);
		}
	}

	DetachChildren(newSelection);

	GetObjectManager()->AddObjectsToSelection(newSelection);
}

void CGroup::Open()
{
	if (!m_opened)
	{
		GetObjectManager()->NotifyObjectListeners(this, CObjectEvent(OBJECT_ON_UI_PROPERTY_CHANGED));
	}
	m_opened = true;
	GetObjectManager()->NotifyObjectListeners(this, CObjectEvent(OBJECT_ON_GROUP_OPEN));

}

void CGroup::Close()
{
	if (m_opened)
	{
		GetObjectManager()->NotifyObjectListeners(this, CObjectEvent(OBJECT_ON_UI_PROPERTY_CHANGED));
	}
	m_opened = false;
	UpdateGroup();
	GetObjectManager()->NotifyObjectListeners(this, CObjectEvent(OBJECT_ON_GROUP_CLOSE));
}

void CGroup::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	// TODO: We need a way of creating objects outside the context of the level/layer.
	// That way we only need to handle adding the final created object and ignore all child events before final creation
	auto dispatcher = GetIEditor()->GetObjectManager()->GetBatchProcessDispatcher();
	dispatcher.Start({ this }, true);
	CBaseObject::PostClone(pFromObject, ctx);
}

void CGroup::RecursivelyGetBoundBox(CBaseObject* object, AABB& box, const Matrix34& parentTM)
{
	Matrix34 worldTM = parentTM * object->GetLocalTM();
	AABB b;
	object->GetLocalBounds(b);
	b.SetTransformedAABB(worldTM, b);
	box.Add(b.min);
	box.Add(b.max);

	int numChilds = object->GetChildCount();
	for (int i = 0; i < numChilds; ++i)
		RecursivelyGetBoundBox(object->GetChild(i), box, worldTM);

	int numLinkedObjects = object->GetLinkedObjectCount();
	for (int i = 0; i < numLinkedObjects; ++i)
		RecursivelyGetBoundBox(object->GetLinkedObject(i), box, worldTM);
}

void CGroup::RemoveChildren(const std::vector<CBaseObject*>& objects)
{
	if (objects.size() == m_children.size())
	{
		for (auto pObj : objects)
		{
			pObj->m_parent = nullptr;
		}
		m_children.clear();
	}
	else
	{
		m_children.erase(std::remove_if(m_children.begin(), m_children.end(), [this, &objects](CBaseObject* pChild)
		{
			return std::any_of(objects.cbegin(), objects.cend(), [pChild](CBaseObject* pObj)
			{
				pObj->m_parent = nullptr;
				return pObj == pChild;
			});
		}), m_children.end());
	}

	InvalidateBBox();
}

void CGroup::CalcBoundBox()
{
	Matrix34 identityTM;
	identityTM.SetIdentity();

	// Calc local bounds box..
	AABB box;
	box.Reset();

	for (auto pChild : m_children)
	{
		RecursivelyGetBoundBox(pChild, box, identityTM);
	}

	if (m_children.size() == 0)
	{
		box.min = Vec3(-1, -1, -1);
		box.max = Vec3(1, 1, 1);
	}

	m_bbox = box;
	m_bBBoxValid = true;
}

void CGroup::OnChildModified()
{
	if (m_ignoreChildModify)
		return;

	InvalidateBBox();
}

void CGroup::InvalidateBBox()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)
	if (CBaseObject* pParent = GetGroup())
	{
		CGroup* pParentGroup = static_cast<CGroup*>(pParent);
		pParentGroup->InvalidateBBox();
	}
	m_bBBoxValid = false;
}

void CGroup::UpdateGroup()
{
	InvalidateBBox();

	CBaseObject::UpdateGroup();
}

void CGroup::OnEvent(ObjectEvent event)
{
	CBaseObject::OnEvent(event);

	switch (event)
	{
	case EVENT_DBLCLICK:
		// Select all objects within the group when double clicking the group
		if (IsOpen())
		{
			std::vector<CBaseObject*> children;
			children.reserve(m_children.size());
			for (_smart_ptr<CBaseObject> pObject : m_children)
			{
				children.push_back(pObject.get());
			}

			GetObjectManager()->SelectObjects(children);
		}
		break;

	default:
		{
			for (auto pChild : m_children)
			{
				pChild->OnEvent(event);
			}
		}
		break;
	}
}

void CGroup::DetachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos /* = true */, bool shouldPlaceOnRoot /* = false */)
{
	if (objects.empty())
		return;

	using namespace Private_Group;
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	auto batchProcessDispatcher = GetObjectManager()->GetBatchProcessDispatcher();
	batchProcessDispatcher.Start(objects);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoDetach(objects, shouldKeepPos, shouldPlaceOnRoot));
	}

	std::vector<Matrix34> worldTMs;
	std::vector<ITransformDelegate*> transformDelegates;

	GetObjectManager()->NotifyObjectListeners(objects, CObjectPreDetachedEvent(this, shouldKeepPos));

	ResetTransforms(this, objects, shouldKeepPos, worldTMs, transformDelegates);

	{
		CScopedSuspendUndo suspendUndo;
		RemoveChildren(objects);
	}

	RestoreTransforms(objects, shouldKeepPos, worldTMs, transformDelegates);

	GetObjectManager()->NotifyObjectListeners(objects, CObjectDetachedEvent(this));

	// If we didn't mean to detach from the whole hierarchy, then reattach to it's grandparent
	CGroup* pGrandParent = static_cast<CGroup*>(GetGroup());
	if (pGrandParent && !shouldPlaceOnRoot)
	{
		pGrandParent->AddMembers(objects, shouldKeepPos);
	}
	//it's important we notify the layer that something changed as we are moving something that was serialized directly inside it to another place (aka group or prefab)
	//before this the layer was not notified at all and you ended up with two copies of the same thing
	GetLayer()->SetModified(true);
}

void CGroup::DetachAll(bool keepPos /*= true*/, bool placeOnRoot /*= false*/)
{
	if (m_children.empty())
	{
		return;
	}

	std::vector<CBaseObject*> objects;
	objects.reserve(m_children.size());
	for (auto pChild : m_children)
	{
		objects.push_back(pChild);
	}
	DetachChildren(objects, keepPos, placeOnRoot);
}

void CGroup::AttachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos /*= true*/, bool shouldInvalidateTM /*= true*/)
{
	using namespace Private_Group;

	// Make sure there's really objects to attach
	if (objects.empty())
		return;

	std::vector<IObjectLayer*> oldLayers = GetObjectsLayers(objects);

	RemoveIfAlreadyChildrenOf(this, objects);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoAttach(objects, this, shouldKeepPos));
	}

	CBatchAttachChildrenTransformationsHandler transformationsHandler(this, objects, shouldKeepPos, shouldInvalidateTM);

	{
		CScopedSuspendUndo suspendUndo;

		ForEachParentOf(objects, [shouldKeepPos](CGroup* pParent, std::vector<CBaseObject*>& children)
		{
			if (pParent)
			{
			  // TODO: optimize
			  for (auto pChild : children)
			  {
			    //If this object is in a prefab we have to clean it up (aka remove the serialized entry from the XML of the item), before the object is assigned to the new prefab
			    pChild->UpdatePrefab(eOCOT_Delete);
				}

			  pParent->DetachChildren(children, shouldKeepPos, true);
			}
			else
			{
			  // TODO: optimize
			  for (auto pChild : children)
			  {
			    pChild->UnLink(true);
				}
			}
		});

		// TODO: optimize
		for (auto pChild : objects)
		{
			pChild->SetDescendantsLayer(m_layer);
		}
		// Add to child list first to make sure node not get deleted while reattaching.
		m_children.reserve(m_children.size() + objects.size());
		m_children.insert(m_children.end(), objects.cbegin(), objects.cend());

		GetObjectManager()->NotifyObjectListeners(objects, CObjectPreAttachedEvent(this, shouldKeepPos));

		transformationsHandler.HandlePreAttach();
	}

	{
		CScopedSuspendUndo suspendUndo;

		transformationsHandler.HandleAttach();

		GetObjectManager()->NotifyObjectListeners(objects, CObjectAttachedEvent(this));

		//This causes the group to be re-serialized into the prefab
		UpdatePrefab(eOCOT_ModifyTransform);
	}

	//it's important we notify the layer that something changed as we are moving something that was serialized directly inside it to another place (aka group or prefab)
	//before this the layer was not notified at all and you ended up with two copies of the same thing
	GetLayer()->SetModified(true);
}

void CGroup::SetMaterial(IEditorMaterial* pMtl)
{
	if (pMtl)
	{
		for (auto pChild : m_children)
		{
			pChild->SetMaterial(pMtl);
		}
	}
	CBaseObject::SetMaterial(pMtl);
}

void CGroup::UpdateHighlightPassState(bool bSelected, bool bHighlighted)
{
	for (int i = 0, totObjects = GetChildCount(); i < totObjects; ++i)
	{
		bool bSelectedLocal = bSelected;
		bool bHighlightedLocal = bHighlighted;

		CBaseObject* pObj = GetChild(i);

		// if group is open, then take the state of the object itself into account too
		if (IsOpen())
		{
			bSelectedLocal |= pObj->IsSelected();
			bHighlightedLocal |= pObj->IsHighlighted();
		}

		pObj->UpdateHighlightPassState(bSelectedLocal, bHighlightedLocal);
	}
}

void CGroup::UpdatePivot(const Vec3& newWorldPivotPos)
{
	if (m_bUpdatingPivot)
		return;

	m_bUpdatingPivot = true;

	const Matrix34& worldTM = GetWorldTM();
	Matrix34 invWorldTM = worldTM.GetInverted();

	Vec3 offset = worldTM.GetTranslation() - newWorldPivotPos;

	for (int i = 0, iChildCount(GetChildCount()); i < iChildCount; ++i)
	{
		Vec3 childWorldPos = worldTM.TransformPoint(GetChild(i)->GetPos());
		GetChild(i)->SetPos(invWorldTM.TransformPoint(childWorldPos + offset));
	}

	CBaseObject::SetWorldPos(newWorldPivotPos);
	m_bUpdatingPivot = false;
}