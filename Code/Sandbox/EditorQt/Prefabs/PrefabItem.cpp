// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabItem.h"

#include "PrefabLibrary.h"
#include "PrefabManager.h"
#include "Prefabs/PrefabEvents.h"
#include "BaseLibraryManager.h"

#include "Grid.h"
#include <CryMath/Cry_Math.h>

#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphManager.h"

#include "Objects/PrefabObject.h"
#include "Objects/SelectionGroup.h"
#include "Objects/EntityObject.h"
#include "Objects/ObjectLoader.h"

#include "IUndoManager.h"

#include <Objects/IObjectLayer.h>

//////////////////////////////////////////////////////////////////////////
CPrefabItem::CPrefabItem()
{
	m_PrefabClassName = PREFAB_OBJECT_CLASS_NAME;
	m_objectsNode = XmlHelpers::CreateXmlNode("Objects");
}

void CPrefabItem::SetPrefabClassName(string prefabClassNameString)
{
	m_PrefabClassName = prefabClassNameString;
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem::~CPrefabItem()
{

}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::Serialize(SerializeContext& ctx)
{
	CBaseLibraryItem::Serialize(ctx);
	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		XmlNodeRef objects = node->findChild("Objects");
		if (objects)
		{
			m_objectsNode = objects;
			// Flatten groups if not flatten already
			// The following code will just transform the previous nested groups to flatten hierarchy for easier operation implementation
			std::queue<XmlNodeRef> groupObjects;
			std::deque<XmlNodeRef> objectsToReattach;

			for (int i = 0, count = objects->getChildCount(); i < count; ++i)
			{
				XmlNodeRef objectNode = objects->getChild(i);
				string objectType;
				objectNode->getAttr("Type", objectType);
				if (!objectType.CompareNoCase("Group"))
				{
					XmlNodeRef groupObjectsNode = objectNode->findChild("Objects");
					if (groupObjectsNode)
						groupObjects.push(groupObjectsNode);
				}

				// Process group
				while (!groupObjects.empty())
				{
					XmlNodeRef group = groupObjects.front();
					groupObjects.pop();

					for (int j = 0, childCount = group->getChildCount(); j < childCount; ++j)
					{
						XmlNodeRef child = group->getChild(j);
						string childType;

						child->getAttr("Type", childType);
						if (!childType.CompareNoCase("Group"))
						{
							XmlNodeRef groupObjectsNode = child->findChild("Objects");
							if (groupObjectsNode)
							{
								objectsToReattach.push_back(child);
								groupObjects.push(groupObjectsNode);
							}
						}
						else
						{
							objectsToReattach.push_back(child);
						}
					}

					group->getParent()->removeAllChilds();
				}
			}

			for (int i = 0, count = objectsToReattach.size(); i < count; ++i)
			{
				m_objectsNode->addChild(objectsToReattach[i]);
			}
		}
	}
	else
	{
		if (m_objectsNode)
		{
			node->addChild(m_objectsNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::Update()
{
	// Mark library as modified.
	SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::MakeFromSelection(const CSelectionGroup& fromSelection)
{
	const CSelectionGroup* selection = GetIEditorImpl()->GetSelection();
	selection->FilterParents();

	int i;
	std::vector<CBaseObject*> objects;
	objects.reserve(selection->GetFilteredCount());
	for (i = 0; i < selection->GetFilteredCount(); i++)
	{
		objects.push_back(selection->GetFilteredObject(i));
	}

	if (objects.size())
	{
		const auto lastIdx = objects.size() - 1;
		IObjectLayer* pDestLayer = objects[lastIdx]->GetLayer();

		for (i = 0; i < objects.size(); ++i)
		{
			if (!objects[i]->AreLinkedDescendantsInLayer(pDestLayer))
			{
				string message;
				message.Format("The objects you are trying to create a prefab from are on different layers. All objects will be moved to %s layer\n\n"
					"Do you want to continue?", pDestLayer->GetName());

				if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(message)))
				{
					return;
				}

				break;
			}
		}
	}

	GetIEditorImpl()->GetIUndoManager()->Begin();
	CBaseObject* pObject = GetIEditorImpl()->NewObject(PREFAB_OBJECT_CLASS_NAME);

	if (!pObject || !pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		return;

	CPrefabObject* pPrefabObj = (CPrefabObject*)pObject;
	pPrefabObj->SetPrefab(this, false);

	if (!pPrefabObj)
	{
		GetIEditorImpl()->GetIUndoManager()->Cancel();
		return;
	}

	// Snap center to grid.
	Vec3 center = gSnappingPreferences.Snap(selection->GetCenter());
	pPrefabObj->SetPos(center);

	GetIEditorImpl()->GetPrefabManager()->AddSelectionToPrefab(pPrefabObj);

	// Clear selection
	GetIEditorImpl()->GetObjectManager()->SelectObject(pPrefabObj);

	GetIEditorImpl()->GetIUndoManager()->Accept("Create Prefab");
	GetIEditorImpl()->SetModifiedFlag();

	SetModified();
}

void CPrefabItem::SaveLinkedObjects(CObjectArchive& ar, CBaseObject* pObj, bool bAllowOwnedByPrefab)
{
	for (auto i = 0; i < pObj->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObj->GetLinkedObject(i);
		if (bAllowOwnedByPrefab || !pLinkedObject->IsPartOfPrefab())
		{
			ar.SaveObject(pLinkedObject, false, true);
			SaveLinkedObjects(ar, pLinkedObject, bAllowOwnedByPrefab);
		}
	}
}

void CPrefabItem::CollectLinkedObjects(CBaseObject* pObj, std::vector<CBaseObject*>& linkedObjects, CSelectionGroup& selection)
{
	for (auto i = 0; i < pObj->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObj->GetLinkedObject(i);
		if (selection.IsContainObject(pLinkedObject))
			selection.RemoveObject(pLinkedObject);
		
		linkedObjects.push_back(pLinkedObject);

		CollectLinkedObjects(pLinkedObject, linkedObjects, selection);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::UpdateFromPrefabObject(CPrefabObject* pPrefabObject, const SObjectChangedContext& context)
{
	CPrefabManager* const pPrefabManager = GetIEditorImpl()->GetPrefabManager();
	// Skip other prefab types
	if (!pPrefabObject || pPrefabObject->IsModifyInProgress()
		||!pPrefabManager || context.m_modifiedObjectGuidInPrefab == CryGUID::Null() || pPrefabManager->ShouldSkipPrefabUpdate())
	{
		return;
	}

	IObjectManager* const pObjMan = GetIEditorImpl()->GetObjectManager();
	CBaseObject* pObj = pObjMan->FindObject(context.m_modifiedObjectGlobalId);
	if(!pObj)
		return;

	pPrefabObject->SetModifyInProgress(true);
	// Prevent recursive calls to this function when modifying other instances of this same prefab
	CPrefabManager::SkipPrefabUpdate skipUpdates;

	CSelectionGroup selection;
	CSelectionGroup flatSelection;

	// Get all objects part of the prefab
	pPrefabObject->GetAllPrefabFlagedChildren(selection);

	CBaseObjectsArray allInstancedPrefabs;
	pObjMan->FindObjectsOfType(RUNTIME_CLASS(CPrefabObject), allInstancedPrefabs);

	// While modifying the instances we don't want the FG manager to send events since they can call into GUI components and cause a crash
	// e.g. we are modifying one FG but in fact we are working on multiple instances of this FG the MFC code does not cope well with this
	CFlowGraphManager* const pFlowMan = GetIEditorImpl()->GetFlowGraphManager();
	pFlowMan->DisableNotifyListeners(true);

	{
		//////////////////////////////////////////////////////////////////////////
		// Save all objects in flat selection to XML.
		selection.FlattenHierarchy(flatSelection);

		// Serialize in the prefab lib (the main XML representation of the prefab)
		{
			CScopedSuspendUndo suspendUndo;
			TObjectIdMapping mapping;
			ExtractObjectsPrefabIDtoGuidMapping(flatSelection, mapping);
			ModifyLibraryPrefab(flatSelection, pPrefabObject, context, mapping);
		}

		SetModified();

		if (context.m_operation == eOCOT_ModifyTransformInLibOnly)
		{
			pFlowMan->DisableNotifyListeners(false);
			pPrefabObject->SetModifyInProgress(false);
			return;
		}

		SObjectChangedContext modifiedContext = context;

		// If modify transform case
		if (context.m_operation == eOCOT_ModifyTransform)
			modifiedContext.m_localTM = pObj->GetLocalTM();

		// Now change all the rest instances of this prefab in the level (skipping this one, since it is already been modified)
		pPrefabManager->SetSelectedItem(this);
		pFlowMan->SetGUIControlsProcessEvents(false, false);

		{
			CScopedSuspendUndo suspendUndo;

			for (size_t i = 0, prefabsCount = allInstancedPrefabs.size(); i < prefabsCount; ++i)
			{
				CPrefabObject* const pInstancedPrefab = static_cast<CPrefabObject*>(allInstancedPrefabs[i]);
				if (pPrefabObject != pInstancedPrefab && pInstancedPrefab->GetPrefabGuid() == pPrefabObject->GetPrefabGuid())
				{
					CSelectionGroup instanceSelection;
					CSelectionGroup instanceFlatSelection;

					pInstancedPrefab->GetAllPrefabFlagedChildren(instanceSelection);

					instanceSelection.FlattenHierarchy(instanceFlatSelection);

					TObjectIdMapping mapping;
					ExtractObjectsPrefabIDtoGuidMapping(instanceFlatSelection, mapping);

					// Modify instanced prefabs in the level
					pInstancedPrefab->SetModifyInProgress(true);
					ModifyInstancedPrefab(instanceFlatSelection, pInstancedPrefab, modifiedContext, mapping);
					pInstancedPrefab->SetModifyInProgress(false);
				}
			}
		} // ~CScopedSuspendUndo suspendUndo;

		pFlowMan->SetGUIControlsProcessEvents(true, true);
	}

	pFlowMan->DisableNotifyListeners(false);

	pPrefabObject->SetModifyInProgress(false);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::UpdateObjects()
{
	std::vector<CBaseObject*> pPrefabObjects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CPrefabObject), pPrefabObjects);
	for (int i = 0, iObjListSize(pPrefabObjects.size()); i < iObjListSize; ++i)
	{
		CPrefabObject* pPrefabObject = (CPrefabObject*)pPrefabObjects[i];
		if (pPrefabObject->GetPrefabGuid() == GetGUID())
		{
			// Even though this is not undo process, the undo flags should be true to avoid crashes caused by light anim sequence.
			pPrefabObject->SetPrefab(this, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::ModifyLibraryPrefab(CSelectionGroup& objectsInPrefabAsFlatSelection, CPrefabObject* pPrefabObject, const SObjectChangedContext& context, const TObjectIdMapping& guidMapping)
{
	IObjectManager* const pObjManager = GetIEditorImpl()->GetObjectManager();

	if (CBaseObject* const pObj = objectsInPrefabAsFlatSelection.GetObjectByGuidInPrefab(context.m_modifiedObjectGuidInPrefab))
	{
		// MEMBER ADD
		if (context.m_operation == eOCOT_Add)
		{
			// If we are trying to add something which already exists in the prefab definition we are trying to modify it
			if (FindObjectByGuid(pObj->GetIdInPrefab(), true))
			{
				SObjectChangedContext correctedContext = context;
				correctedContext.m_operation = eOCOT_Modify;
				ModifyLibraryPrefab(objectsInPrefabAsFlatSelection, pPrefabObject, correctedContext, guidMapping);
				return;
			}

			CObjectArchive ar(pObjManager, m_objectsNode, false);

			if (pObj->GetParent() == pPrefabObject)
				pObj->DetachThis(false);

			ar.SaveObject(pObj, false, true);
			SaveLinkedObjects(ar, pObj, true);

			if (!pObj->GetParent())
				pPrefabObject->AttachChild(pObj, false);

			XmlNodeRef savedObj = m_objectsNode->getChild(m_objectsNode->getChildCount() - 1);
			RemapIDsInNodeAndChildren(savedObj, guidMapping, false);
		}
		// Member MODIFY
		else if (context.m_operation == eOCOT_Modify || context.m_operation == eOCOT_ModifyTransform || context.m_operation == eOCOT_ModifyTransformInLibOnly)
		{
			// Find -> remove previous definition of the object and serialize the new changes
			if (XmlNodeRef object = FindObjectByGuid(pObj->GetIdInPrefab(), true))
			{
				m_objectsNode->removeChild(object);

				CObjectArchive ar(pObjManager, m_objectsNode, false);

				if (pObj->GetParent() == pPrefabObject)
					pObj->DetachThis(false, true);

				ar.SaveObject(pObj, false, true);

				if (!pObj->GetParent() && !pObj->GetLinkedTo())
					pPrefabObject->AttachChild(pObj, false);

				XmlNodeRef savedObj = m_objectsNode->getChild(m_objectsNode->getChildCount() - 1);
				RemapIDsInNodeAndChildren(savedObj, guidMapping, false);
			}
		}
		// Member DELETE
		else if (context.m_operation == eOCOT_Delete)
		{
			if (XmlNodeRef object = FindObjectByGuid(pObj->GetIdInPrefab(), true))
			{
				m_objectsNode->removeChild(object);

				// Clear the prefab references to this if it is a parent to objects and it is a GROUP
				string objType;
				object->getAttr("Type", objType);
				if (!objType.CompareNoCase("Group"))
				{
					RemoveAllChildsOf(pObj->GetIdInPrefab());
				}
				else
				{
					// A parent was deleted (which wasn't a group!!!) so just remove the parent attribute
					for (int i = 0, count = m_objectsNode->getChildCount(); i < count; ++i)
					{
						CryGUID parentGuid = CryGUID::Null();
						if (m_objectsNode->getChild(i)->getAttr("Parent", parentGuid) && parentGuid == pObj->GetIdInPrefab())
						{
							m_objectsNode->getChild(i)->delAttr("Parent");
						}
					}
				}
			}
		} // ~Member DELETE
	}   // ~if (CBaseObject* const pObj = objectsInPrefabAsFlatSelection.GetObjectByGuidInPrefab(context.m_modifiedObjectGuidInPrefab))
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::ModifyInstancedPrefab(CSelectionGroup& objectsInPrefabAsFlatSelection, CPrefabObject* pPrefabObject, const SObjectChangedContext& context, const TObjectIdMapping& guidMapping)
{
	IObjectManager* const pObjManager = GetIEditorImpl()->GetObjectManager();

	if (context.m_operation == eOCOT_Add)
	{
		if (CBaseObject* const pObj = pObjManager->FindObject(context.m_modifiedObjectGlobalId))
		{
			if (XmlNodeRef changedObject = FindObjectByGuid(pObj->GetIdInPrefab(), false))
			{
				// See if we already have this object as part of the instanced prefab
				std::vector<CBaseObject*> childs;
				pPrefabObject->GetAllPrefabFlagedChildren(childs);
				if (CBaseObject* pModified = FindObjectByPrefabGuid(childs, pObj->GetIdInPrefab()))
				{
					SObjectChangedContext correctedContext = context;
					correctedContext.m_operation = eOCOT_Modify;
					ModifyInstancedPrefab(objectsInPrefabAsFlatSelection, pPrefabObject, correctedContext, guidMapping);
					return;
				}

				CObjectArchive loadAr(pObjManager, changedObject, true);
				CPrefabChildGuidProvider guidProvider(pPrefabObject);
				loadAr.SetGuidProvider(&guidProvider);
				loadAr.EnableProgressBar(false);

				// PrefabGUID -> GUID
				RemapIDsInNodeAndChildren(changedObject, guidMapping, true);

				// Load/Clone
				CBaseObject* pClone = loadAr.LoadObject(changedObject);
				loadAr.ResolveObjects();

				RegisterPrefabEventFlowNodes(pClone);

				pPrefabObject->SetObjectPrefabFlagAndLayer(pClone);

				if (!pClone->GetParent())
					pPrefabObject->AttachChild(pClone, false);

				// GUID -> PrefabGUID
				RemapIDsInNodeAndChildren(changedObject, guidMapping, false);
			}
		}
	}
	else if (CBaseObject* const pObj = objectsInPrefabAsFlatSelection.GetObjectByGuidInPrefab(context.m_modifiedObjectGuidInPrefab))
	{
		if (context.m_operation == eOCOT_Modify)
		{
			if (XmlNodeRef changedObject = FindObjectByGuid(pObj->GetIdInPrefab(), false))
			{
				// Build load archive
				CObjectArchive loadAr(pObjManager, changedObject, true);
				loadAr.bUndo = true;
				loadAr.SetShouldResetInternalMembers(true);

				// Load ids remapping info into the load archive
				for (int j = 0, count = guidMapping.size(); j < count; ++j)
					loadAr.RemapID(guidMapping[j].from, guidMapping[j].to);

				// PrefabGUID -> GUID
				RemapIDsInNodeAndChildren(changedObject, guidMapping, true);

				// Set parent attr if we dont have any (because we don't store this in the prefab XML since it is implicitly given)
				bool noParentIdAttr = false;
				if (!changedObject->haveAttr("Parent") && !changedObject->haveAttr("LinkedTo"))
				{
					noParentIdAttr = true;
					changedObject->setAttr("Parent", pPrefabObject->GetId());
				}

				// Back-up flags before serializing. We need some flags to be persistent after serialization. Ex: Selection flags
				auto flags = pObj->GetFlags();

				// Load the object
				pObj->Serialize(loadAr);

				// Set old flags
				pObj->SetFlags(flags);

				RegisterPrefabEventFlowNodes(pObj);

				pPrefabObject->SetObjectPrefabFlagAndLayer(pObj);

				// Remove parent attribute if it is directly a child of the prefab
				if (noParentIdAttr)
					changedObject->delAttr("Parent");

				if (!pObj->GetParent() && !pObj->GetLinkedTo())
					pPrefabObject->AttachChild(pObj, false);

				// GUID -> PrefabGUID
				RemapIDsInNodeAndChildren(changedObject, guidMapping, false);
			}
		}
		else if (context.m_operation == eOCOT_ModifyTransform)
		{
			if (XmlNodeRef changedObject = FindObjectByGuid(pObj->GetIdInPrefab(), false))
			{
				CryGUID parentGUID = CryGUID::Null();
				CryGUID linkedToGUID = CryGUID::Null();
				changedObject->getAttr("Parent", parentGUID);
				changedObject->getAttr("LinkedTo", linkedToGUID);
				if (CBaseObject* pObjParent = objectsInPrefabAsFlatSelection.GetObjectByGuidInPrefab(parentGUID))
					pObjParent->AttachChild(pObj);
				else if (CBaseObject* pLinkedTo = objectsInPrefabAsFlatSelection.GetObjectByGuidInPrefab(linkedToGUID))
					pObj->LinkTo(pLinkedTo);

				pObj->SetLocalTM(context.m_localTM, eObjectUpdateFlags_Undo);
			}
		}
		else if (context.m_operation == eOCOT_Delete)
		{
			pObjManager->DeleteObject(pObj);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CPrefabItem::FindObjectByGuid(const CryGUID& guid, bool forwardSearch)
{
	CryGUID objectId = CryGUID::Null();

	if (forwardSearch)
	{
		for (int i = 0, count = m_objectsNode->getChildCount(); i < count; ++i)
		{
			m_objectsNode->getChild(i)->getAttr("Id", objectId);
			if (objectId == guid)
				return m_objectsNode->getChild(i);
		}
	}
	else
	{
		for (int i = m_objectsNode->getChildCount() - 1; i >= 0; --i)
		{
			m_objectsNode->getChild(i)->getAttr("Id", objectId);
			if (objectId == guid)
				return m_objectsNode->getChild(i);
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CBaseObjectPtr CPrefabItem::FindObjectByPrefabGuid(const std::vector<CBaseObject*>& objects, CryGUID guidInPrefab)
{
	for (auto it = objects.cbegin(), end = objects.cend(); it != end; ++it)
	{
		if ((*it)->GetIdInPrefab() == guidInPrefab)
			return (*it);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::ExtractObjectsPrefabIDtoGuidMapping(CSelectionGroup& objects, TObjectIdMapping& mapping)
{
	mapping.reserve(objects.GetCount());
	std::queue<CBaseObject*> groups;

	for (int i = 0, count = objects.GetCount(); i < count; ++i)
	{
		CBaseObject* pObject = objects.GetObject(i);
		// Save group mapping
		mapping.push_back(SObjectIdMapping(pObject->GetIdInPrefab(), pObject->GetId()));
		// If this is a group recursively get all the objects ID mapping inside
		if (pObject->IsKindOf(RUNTIME_CLASS(CGroup)))
		{
			groups.push(pObject);
			while (!groups.empty())
			{
				CBaseObject* pGroup = groups.front();
				groups.pop();

				for (int j = 0, childCount = pGroup->GetChildCount(); j < childCount; ++j)
				{
					CBaseObject* pChild = pGroup->GetChild(j);
					mapping.push_back(SObjectIdMapping(pChild->GetIdInPrefab(), pChild->GetId()));
					// If group add it for further processing
					if (pChild->IsKindOf(RUNTIME_CLASS(CGroup)))
						groups.push(pChild);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CryGUID CPrefabItem::ResolveID(const TObjectIdMapping& prefabIdToGuidMapping, CryGUID id, bool prefabIdToGuidDirection)
{
	if (prefabIdToGuidDirection)
	{
		for (int j = 0, count = prefabIdToGuidMapping.size(); j < count; ++j)
		{
			if (prefabIdToGuidMapping[j].from == id)
				return prefabIdToGuidMapping[j].to;
		}
	}
	else
	{
		for (int j = 0, count = prefabIdToGuidMapping.size(); j < count; ++j)
		{
			if (prefabIdToGuidMapping[j].to == id)
				return prefabIdToGuidMapping[j].from;
		}
	}

	return id;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::RemapIDsInNodeAndChildren(XmlNodeRef objectNode, const TObjectIdMapping& mapping, bool prefabIdToGuidDirection)
{
	std::queue<XmlNodeRef> objects;
	const char* kFlowGraphTag = "FlowGraph";

	if (!objectNode->isTag(kFlowGraphTag))
		RemapIDsInNode(objectNode, mapping, prefabIdToGuidDirection);

	for (int i = 0; i < objectNode->getChildCount(); ++i)
	{
		objects.push(objectNode->getChild(i));
		// Recursively traverse all objects and exclude flowgraph parts
		while (!objects.empty())
		{
			XmlNodeRef object = objects.front();
			// Skip flowgraph
			if (object->isTag(kFlowGraphTag))
			{
				objects.pop();
				continue;
			}

			RemapIDsInNode(object, mapping, prefabIdToGuidDirection);
			objects.pop();

			for (int j = 0, childCount = object->getChildCount(); j < childCount; ++j)
			{
				XmlNodeRef child = object->getChild(j);
				if (child->isTag(kFlowGraphTag))
					continue;

				RemapIDsInNode(child, mapping, prefabIdToGuidDirection);

				if (child->getChildCount())
					objects.push(child);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::RemapIDsInNode(XmlNodeRef objectNode, const TObjectIdMapping& mapping, bool prefabIdToGuidDirection)
{
	CryGUID patchedId;
	if (objectNode->getAttr("Id", patchedId))
		objectNode->setAttr("Id", ResolveID(mapping, patchedId, prefabIdToGuidDirection));
	if (objectNode->getAttr("Parent", patchedId))
		objectNode->setAttr("Parent", ResolveID(mapping, patchedId, prefabIdToGuidDirection));
	if (objectNode->getAttr("LinkedTo", patchedId))
		objectNode->setAttr("LinkedTo", ResolveID(mapping, patchedId, prefabIdToGuidDirection));
	if (objectNode->getAttr("LookAt", patchedId))
		objectNode->setAttr("LookAt", ResolveID(mapping, patchedId, prefabIdToGuidDirection));
	if (objectNode->getAttr("TargetId", patchedId))
		objectNode->setAttr("TargetId", ResolveID(mapping, patchedId, prefabIdToGuidDirection));
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::RemoveAllChildsOf(CryGUID guid)
{
	std::deque<XmlNodeRef> childrenToRemove;
	std::deque<CryGUID> guidsToCheckAgainst;
	CryGUID parentId = CryGUID::Null();
	CryGUID linkedToId = CryGUID::Null();

	for (int i = 0, count = m_objectsNode->getChildCount(); i < count; ++i)
	{
		XmlNodeRef child = m_objectsNode->getChild(i);
		if (child->getAttr("Parent", parentId) && guid == parentId ||
			child->getAttr("LinkedTo", linkedToId) && guid == linkedToId)
		{
			childrenToRemove.push_front(child);
			string objType;
			if (child->getAttr("Type", objType) && !objType.CompareNoCase("Group"))
			{
				CryGUID groupId;
				child->getAttr("Id", groupId);
				guidsToCheckAgainst.push_back(groupId);
			}
		}
	}

	for (std::deque<XmlNodeRef>::iterator it = childrenToRemove.begin(), end = childrenToRemove.end(); it != end; ++it)
	{
		m_objectsNode->removeChild((*it));
	}

	for (int i = 0, count = guidsToCheckAgainst.size(); i < count; ++i)
	{
		RemoveAllChildsOf(guidsToCheckAgainst[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabItem::RegisterPrefabEventFlowNodes(CBaseObject* const pEntityObj)
{
	if (pEntityObj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pEntityObj);
		if (CHyperFlowGraph* pFG = pEntity->GetFlowGraph())
		{
			IHyperGraphEnumerator* pEnumerator = pFG->GetNodesEnumerator();
			IHyperNode* pNode = pEnumerator->GetFirst();
			while (pNode)
			{
				GetIEditorImpl()->GetPrefabManager()->GetPrefabEvents()->OnHyperGraphManagerEvent(EHG_NODE_ADD, pNode->GetGraph(), pNode);
				pNode = pEnumerator->GetNext();
			}
			pEnumerator->Release();
		}
	}
}

