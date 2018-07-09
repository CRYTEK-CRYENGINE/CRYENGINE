// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Group.h"
#include "CryEdit.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/DisplayContext.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "EntityObject.h"
#include "Objects/ObjectLayerManager.h"
#include <algorithm>

#include <Serialization/Decorators/EditToolButton.h>
#include <Serialization/Decorators/EditorActionButton.h>

REGISTER_CLASS_DESC(CGroupClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
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

void RemoveIfAlreadyMember(const TBaseObjects& members, std::vector<CBaseObject*>& objects)
{
	objects.erase(std::remove_if(objects.begin(), objects.end(), [&members](const CBaseObject* pObj)
		{
			return std::find(members.cbegin(), members.cend(), pObj) != members.cend();
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
	CScopedSuspendUndo suspendUndo;

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

	CGroup*                          m_pParent { nullptr };
	const std::vector<CBaseObject*>& m_children;
	bool                             m_shouldKeepPos { false };
	bool                             m_shouldInvalidateTM { false };
	std::vector<ITransformDelegate*> m_transformDelegates;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for attach/detach changes
class CUndoBatchAttachBaseObject : public IUndoObject
{
public:
	CUndoBatchAttachBaseObject(const std::vector<CBaseObject*>& objects, const std::vector<IObjectLayer*>& oldLayers, bool shouldKeepPos,
	                           bool shouldPlaceOnRoot, bool isAttaching)
		: m_shouldPlaceOnRoot(shouldPlaceOnRoot)
		, m_shouldKeepPos(shouldKeepPos)
		, m_isAttaching(isAttaching)
	{
		if (objects.empty())
		{
			return;
		}
		m_oldLayersGuids.reserve(oldLayers.size());
		for (auto pOldLayer : oldLayers)
		{
			m_oldLayersGuids.emplace_back(pOldLayer->GetGUID());
		}
		m_parentGuid = objects[0]->GetParent()->GetId();
		m_objectsGuids.reserve(objects.size());
		for (auto pObj : objects)
		{
			m_objectsGuids.emplace_back(pObj->GetId());
		}
	}

	virtual void Undo(bool bUndo) override
	{
		if (m_isAttaching)
		{
			Detach();
		}
		else
		{
			Attach();
		}
	}

	virtual void Redo() override
	{
		if (m_isAttaching)
		{
			Attach();
		}
		else
		{
			Detach();
		}
	}

private:
	void Attach()
	{
		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		std::vector<CBaseObject*> objects;
		objects.reserve(m_objectsGuids.size());
		for (const auto& objectGuid : m_objectsGuids)
		{
			CBaseObject* pObject = pObjectManager->FindObject(objectGuid);
			if (pObject)
			{
				objects.emplace_back(pObject);
			}

		}
		CBaseObject* pParentObject = pObjectManager->FindObject(m_parentGuid);

		if (pParentObject && !objects.empty())
		{
			assert(GetIEditor()->IsCGroup(pParentObject));
			static_cast<CGroup*>(pParentObject)->AddMembers(objects, m_shouldKeepPos);

			for (auto pObject : objects)
			{
				pObject->UpdatePrefab();
			}
		}
	}

	void Detach()
	{
		if (m_objectsGuids.empty())
		{
			return;
		}

		IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
		std::vector<CBaseObject*> objects;
		objects.reserve(m_objectsGuids.size());
		for (auto it = m_objectsGuids.rbegin(); it != m_objectsGuids.rend(); ++it)
		{
			CBaseObject* pObject = pObjectManager->FindObject(*it);
			if (pObject)
			{
				objects.emplace_back(pObject);
			}
			else
			{
				m_oldLayersGuids.erase(std::next(it).base());
			}

		}
		CBaseObject* pParentObject = pObjectManager->FindObject(m_parentGuid);
		CGroup* pGroup = static_cast<CGroup*>(pParentObject);
		pGroup->DetachChildren(objects, m_shouldKeepPos, m_shouldPlaceOnRoot);

		for (int i = 0; i < objects.size(); ++i)
		{
			objects[i]->SetLayer(pObjectManager->GetIObjectLayerManager()->FindLayer(m_oldLayersGuids[i]));
			objects[i]->UpdatePrefab();
		}
	}

	virtual int         GetSize()        { return sizeof(CUndoBatchAttachBaseObject); }
	virtual const char* GetDescription() { return "Attachment Changed"; }

	std::vector<CryGUID> m_objectsGuids;
	std::vector<CryGUID> m_oldLayersGuids;
	CryGUID              m_parentGuid;
	bool                 m_shouldPlaceOnRoot;
	bool                 m_shouldKeepPos;
	bool                 m_isAttaching;
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
	SetColor(RGB(0, 255, 0)); // Green
}

//////////////////////////////////////////////////////////////////////////
void CGroup::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	DeleteAllMembers();
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
void CGroup::DeleteAllMembers()
{
	LOADING_TIME_PROFILE_SECTION
	std::vector<CBaseObject*> members;
	for (size_t i = 0; i < m_members.size(); ++i)
		members.push_back(m_members[i]);

	// Must collect all linked objects since they're not really considered members of the group
	size_t childCount = m_members.size(); // cache current size
	for (size_t i = 0; i < childCount; ++i)
		GetAllLinkedObjects(members, members[i]);

	GetObjectManager()->DeleteObjects(members);
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CGroup::AddMember(CBaseObject* pMember, bool keepPos)
{
	std::vector<CBaseObject*> members = { pMember };
	AddMembers(members, keepPos);
}

void CGroup::AddMembers(std::vector<CBaseObject*>& objects, bool keepPos /*= true*/)
{
	using namespace Private_Group;

	RemoveIfAlreadyChildrenOf(this, objects);

	RemoveIfAlreadyMember(m_members, objects);

	if (objects.empty())
	{
		return;
	}

	auto batchProcessDispatcher = GetObjectManager()->GetBatchProcessDispatcher();
	batchProcessDispatcher.Start(objects);

	auto oldNumMembers = m_members.size();
	m_members.reserve(m_members.size() + objects.size());
	for (auto pObj : objects)
	{
		m_members.push_back(pObj);
	}

	AttachChildren(objects, keepPos);

	if (oldNumMembers != m_members.size())
	{
		CBaseObject* pObj = nullptr;
		for (auto i = oldNumMembers; i < m_members.size(); ++i)
		{
			pObj = m_members[i];
			if (CGroup* pPrefab = (CGroup*)pObj->GetPrefab())
			{
				pPrefab->AddMember(pObj, true);
			}
		}
	}

	InvalidateBBox();
}

//////////////////////////////////////////////////////////////////////////
void CGroup::RemoveMember(CBaseObject* pMember, bool keepPos, bool placeOnRoot)
{
	std::vector<CBaseObject*> members = { pMember };
	RemoveMembers(members, keepPos, placeOnRoot);
}

void CGroup::RemoveMembers(std::vector<CBaseObject*>& members, bool keepPos /*= true*/, bool placeOnRoot /*= false*/)
{
	LOADING_TIME_PROFILE_SECTION
	FilterOutNonMembers(members);

	CBaseObject* pPrefab = GetPrefab();
	if (pPrefab)
	{
		pPrefab->RemoveMembers(members, keepPos, placeOnRoot);
	}

	if (pPrefab != this)
	{
		DetachChildren(members, keepPos, placeOnRoot);
	}

	UpdateGroup();
}

void CGroup::FilterOutNonMembers(std::vector<CBaseObject*>& objects)
{
	objects.erase(std::remove_if(objects.begin(), objects.end(), [this](CBaseObject* pObject)
	{
		return !pObject || pObject->GetGroup() != this;
	}), objects.end());
}

//////////////////////////////////////////////////////////////////////////
void CGroup::RemoveChild(CBaseObject* child)
{
	bool bMemberChild = stl::find_and_erase(m_members, child);
	CBaseObject::RemoveChild(child);
	if (bMemberChild)
		InvalidateBBox();
}

//////////////////////////////////////////////////////////////////////////
void CGroup::GetBoundBox(AABB& box)
{
	if (!m_bBBoxValid)
		CalcBoundBox();

	box.SetTransformedAABB(GetWorldTM(), m_bbox);
}

//////////////////////////////////////////////////////////////////////////
void CGroup::GetLocalBounds(AABB& box)
{
	if (!m_bBBoxValid)
		CalcBoundBox();
	box = m_bbox;
}

//////////////////////////////////////////////////////////////////////////
bool CGroup::HitTest(HitContext& hc)
{
	bool selected = false;
	if (m_opened)
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
bool CGroup::HitTestMembers(HitContext& hcOrg)
{
	float mindist = FLT_MAX;

	HitContext hc = hcOrg;

	CBaseObject* selected = 0;
	int numMembers = static_cast<int>(m_members.size());
	for (int i = 0; i < numMembers; ++i)
	{
		CBaseObject* obj = m_members[i];

		if (GetObjectManager()->HitTestObject(obj, hc))
		{
			if (hc.dist < mindist)
			{
				mindist = hc.dist;
				// If collided object specified, accept it, otherwise take tested object itself.
				if (hc.object)
					selected = hc.object;
				else
					selected = obj;
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

//////////////////////////////////////////////////////////////////////////
void CGroup::OnContextMenu(CPopupMenuItem* menu)
{
	CBaseObject::OnContextMenu(menu);
}

//////////////////////////////////////////////////////////////////////////
void CGroup::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!gViewportDebugPreferences.showGroupObjectHelper)
		return;

	bool hideNames = dc.flags & DISPLAY_HIDENAMES;

	DrawDefault(dc, GetColor());

	dc.PushMatrix(GetWorldTM());

	AABB boundbox;
	GetLocalBounds(boundbox);

	if (IsSelected())
	{
		dc.SetColor(ColorB(0, 255, 0, 255));
		dc.DrawWireBox(boundbox.min, boundbox.max);
		dc.DepthTestOff();
		dc.DepthWriteOff();
		dc.SetColor(GetColor(), 0.15f);
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
			  ar(Serialization::ActionButton(std::bind(&CCryEditApp::OnGroupUngroup, CCryEditApp::GetInstance())), "ungroup", "^Ungroup");
			  if (pObject->IsOpen())
			  {
			    ar(Serialization::ActionButton(std::bind(&CCryEditApp::OnGroupClose, CCryEditApp::GetInstance())), "close", "^Close");
			  }
			  else
			  {
			    ar(Serialization::ActionButton(std::bind(&CCryEditApp::OnGroupOpen, CCryEditApp::GetInstance())), "open", "^Open");
			  }
			  ar.closeBlock();
			}
		});
	}
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CGroup::SerializeMembers(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		if (!ar.bUndo)
		{
			int num = static_cast<int>(m_members.size());
			for (int i = 0; i < num; ++i)
			{
				CBaseObject* member = m_members[i];
				member->DetachThis(true);
			}
			m_members.clear();

			// Loading.
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
		if (m_members.size() > 0 && !ar.bUndo && !ar.IsSavingInPrefab())
		{
			// Saving.
			XmlNodeRef root = xmlNode->newChild("Objects");
			ar.node = root;

			// Save all child objects to XML.
			int num = static_cast<int>(m_members.size());
			for (int i = 0; i < num; ++i)
			{
				CBaseObject* obj = m_members[i];
				ar.SaveObject(obj, true);
			}
		}
	}
	ar.node = xmlNode;
}

//////////////////////////////////////////////////////////////////////////
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

	GetObjectManager()->SelectObjects(newSelection);
}

//////////////////////////////////////////////////////////////////////////
void CGroup::Open()
{
	if (!m_opened)
	{
		NotifyListeners(OBJECT_ON_UI_PROPERTY_CHANGED);
	}
	m_opened = true;
	GetIEditor()->GetObjectManager()->NotifyObjectListeners(this, OBJECT_ON_GROUP_OPEN);

}

//////////////////////////////////////////////////////////////////////////
void CGroup::Close()
{
	if (m_opened)
	{
		NotifyListeners(OBJECT_ON_UI_PROPERTY_CHANGED);
	}
	m_opened = false;
	UpdateGroup();
	GetIEditor()->GetObjectManager()->NotifyObjectListeners(this, OBJECT_ON_GROUP_CLOSE);
}

void CGroup::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	// TODO: We need a way of creating objects outside the context of the level/layer. 
	// That way we only need to handle adding the final created object and ignore all child events before final creation
	auto dispatcher = GetIEditor()->GetObjectManager()->GetBatchProcessDispatcher();
	dispatcher.Start({ this }, true);
	CBaseObject::PostClone(pFromObject, ctx);
}

//////////////////////////////////////////////////////////////////////////
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
		m_members.clear();
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

		m_members.erase(std::remove_if(m_members.begin(), m_members.end(), [this, &objects](CBaseObject* pChild)
		{
			return std::any_of(objects.cbegin(), objects.cend(), [pChild](CBaseObject* pObj)
			{
				return pObj == pChild;
			});
		}), m_members.end());
	}

	InvalidateBBox();
}

//////////////////////////////////////////////////////////////////////////
void CGroup::CalcBoundBox()
{
	Matrix34 identityTM;
	identityTM.SetIdentity();

	// Calc local bounds box..
	AABB box;
	box.Reset();

	int numMembers = static_cast<int>(m_members.size());
	for (int i = 0; i < numMembers; ++i)
	{
		RecursivelyGetBoundBox(m_members[i], box, identityTM);
	}

	if (numMembers == 0)
	{
		box.min = Vec3(-1, -1, -1);
		box.max = Vec3(1, 1, 1);
	}

	m_bbox = box;
	m_bBBoxValid = true;
}

//////////////////////////////////////////////////////////////////////////
void CGroup::OnChildModified()
{
	if (m_ignoreChildModify)
		return;

	InvalidateBBox();
}

void CGroup::InvalidateBBox()
{
	LOADING_TIME_PROFILE_SECTION
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

//! Select objects within specified distance from given position.
int CGroup::SelectObjects(const AABB& box, bool bUnselect)
{
	int numSel = 0;

	AABB objBounds;
	uint32 hideMask = gViewportDebugPreferences.GetObjectHideMask();
	int num = static_cast<int>(m_members.size());
	for (int i = 0; i < num; ++i)
	{
		CBaseObject* obj = m_members[i];

		if (obj->IsHidden())
			continue;

		if (obj->IsFrozen())
			continue;

		if (obj->GetGroup())
			continue;

		obj->GetBoundBox(objBounds);
		if (box.IsIntersectBox(objBounds))
		{
			numSel++;
			if (!bUnselect)
				GetObjectManager()->SelectObject(obj);
			else
				GetObjectManager()->UnselectObject(obj);
		}
		// If its group.
		if (obj->GetRuntimeClass() == RUNTIME_CLASS(CGroup))
		{
			numSel += ((CGroup*)obj)->SelectObjects(box, bUnselect);
		}
	}
	return numSel;
}

//////////////////////////////////////////////////////////////////////////
void CGroup::OnEvent(ObjectEvent event)
{
	CBaseObject::OnEvent(event);

	switch (event)
	{
	case EVENT_DBLCLICK:
		if (IsOpen())
		{
			int numMembers = static_cast<int>(m_members.size());
			for (int i = 0; i < numMembers; ++i)
			{
				GetObjectManager()->SelectObject(m_members[i]);
			}
		}
		break;

	default:
		{
			int numMembers = static_cast<int>(m_members.size());
			for (int i = 0; i < numMembers; ++i)
			{
				m_members[i]->OnEvent(event);
			}
		}
		break;
	}
};

//////////////////////////////////////////////////////////////////////////
void CGroup::BindToParent()
{
	CBaseObject* parent = GetParent();
	if (parent && parent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* parentEntity = (CEntityObject*)parent;

		IEntity* ientParent = parentEntity->GetIEntity();
		if (ientParent)
		{
			for (int i = 0; i < GetChildCount(); ++i)
			{
				CBaseObject* child = GetChild(i);
				if (child && child->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				{
					CEntityObject* childEntity = (CEntityObject*)child;
					if (childEntity->GetIEntity())
						ientParent->AttachChild(childEntity->GetIEntity(), IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGroup::DetachThis(bool bKeepPos, bool bPlaceOnRoot)
{
	CBaseObject* parent = GetParent();
	if (parent && parent->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* parentEntity = (CEntityObject*)parent;

		IEntity* ientParent = parentEntity->GetIEntity();
		if (ientParent)
		{
			for (int i = 0; i < GetChildCount(); ++i)
			{
				CBaseObject* child = GetChild(i);
				if (child && child->IsKindOf(RUNTIME_CLASS(CEntityObject)))
				{
					CEntityObject* childEntity = (CEntityObject*)child;
					if (childEntity->GetIEntity())
					{
						childEntity->GetIEntity()->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
					}
				}
			}
		}
	}

	CBaseObject::DetachThis(bKeepPos, bPlaceOnRoot);
}

void CGroup::DetachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos /* = true */, bool shouldPlaceOnRoot /* = false */)
{
	if (objects.empty())
		return;

	using namespace Private_Group;
	LOADING_TIME_PROFILE_SECTION;

	auto batchProcessDispatcher = GetObjectManager()->GetBatchProcessDispatcher();
	batchProcessDispatcher.Start(objects);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoBatchAttachBaseObject(objects, GetObjectsLayers(objects), shouldKeepPos, shouldPlaceOnRoot, false));
	}

	std::vector<Matrix34> worldTMs;
	std::vector<ITransformDelegate*> transformDelegates;

	GetObjectManager()->signalBeforeObjectsDetached(this, objects, shouldKeepPos);

	ResetTransforms(this, objects, shouldKeepPos, worldTMs, transformDelegates);

	for (auto pChild : objects)
	{
		pChild->OnDetachThis();
	}

	{
		CScopedSuspendUndo suspendUndo;
		RemoveChildren(objects);
	}

	RestoreTransforms(objects, shouldKeepPos, worldTMs, transformDelegates);

	GetObjectManager()->signalObjectsDetached(this, objects);

	// If we didn't mean to detach from the whole hierarchy, then reattach to it's grandparent
	CGroup* pGrandParent = static_cast<CGroup*>(GetGroup());
	if (pGrandParent && !shouldPlaceOnRoot)
	{
		pGrandParent->AddMembers(objects, shouldKeepPos);
	}
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

	//bool bHasParent = child->m_parent != nullptr;

	RemoveIfAlreadyChildrenOf(this, objects);

	CBatchAttachChildrenTransformationsHandler transormatoinsHandler(this, objects, shouldKeepPos, shouldInvalidateTM);

	{
		CScopedSuspendUndo suspendUndo;

		for (auto pChild : objects)
		{
			pChild->m_bSuppressUpdatePrefab = true;
		}

		ForEachParentOf(objects, [shouldKeepPos](CGroup* pParent, std::vector<CBaseObject*>& children)
		{
			if (pParent)
			{
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

		GetObjectManager()->signalBeforeObjectsAttached(this, objects, shouldKeepPos);

		transormatoinsHandler.HandlePreAttach();
	}

	for (auto pChild : objects)
	{
		OnAttachChild(pChild);
	}

	{
		CScopedSuspendUndo suspendUndo;

		transormatoinsHandler.HandleAttach();

		GetObjectManager()->signalObjectsAttached(this, objects);

		NotifyListeners(OBJECT_ON_CHILDATTACHED);
		for (auto pChild : objects)
		{
			pChild->m_bSuppressUpdatePrefab = false;
		}
		UpdatePrefab(eOCOT_ModifyTransform);
	}

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoBatchAttachBaseObject(objects, oldLayers, shouldKeepPos, false, true));
	}
}

void CGroup::SetMaterial(IEditorMaterial* pMtl)
{
	if (pMtl)
	{
		for (int i = 0, iNumberSize(m_members.size()); i < iNumberSize; ++i)
		{
			m_members[i]->SetMaterial(pMtl);
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

