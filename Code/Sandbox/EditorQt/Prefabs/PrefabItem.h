// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PrefabItem_h__
#define __PrefabItem_h__
#pragma once

#include "BaseLibraryItem.h"
#include <Cry3DEngine/I3DEngine.h>

class CPrefabObject;

// Helpers for object ID mapping for better code readability
struct SObjectIdMapping
{
	CryGUID from;
	CryGUID to;
	SObjectIdMapping() : from(CryGUID::Null()), to(CryGUID::Null()) {}
	SObjectIdMapping(CryGUID src, CryGUID dst) : from(src), to(dst) {}
};

typedef std::vector<SObjectIdMapping> TObjectIdMapping;

/*! CPrefabItem contain definition of particle system spawning parameters.
 *
 */
class SANDBOX_API CPrefabItem : public CBaseLibraryItem
{
	friend class CPrefabObject;
public:
	CPrefabItem();
	~CPrefabItem();

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_PREFAB; };

	void                      Serialize(SerializeContext& ctx);

	//! Make prefab from selection of objects.
	void MakeFromSelection(const CSelectionGroup& selection);

	//! Called when something changed in a prefab the pPrefabObject is the changed object
	void UpdateFromPrefabObject(CPrefabObject* pPrefabObject, const SObjectChangedContext& context);

	//! Called after particle parameters where updated.
	void       Update();
	//! Returns xml node containing prefab objects.
	XmlNodeRef GetObjectsNode()           { return m_objectsNode; };
	string    GetPrefabObjectClassName() { return m_PrefabClassName; };
	void       SetPrefabClassName(string prefabClassNameString);

	void       UpdateObjects();

	//! Searches and finds the XmlNode with a specified Id in m_objectsNode (the XML representation of this prefab in the prefab library)
	XmlNodeRef FindObjectByGuid(const CryGUID& guid, bool fowardSearch = true);

private:
	//! Function to serialize changes to the main prefab lib (this changes only the internal XML representation m_objectsNode)
	void           ModifyLibraryPrefab(CSelectionGroup& objectsInPrefabAsFlatSelection, CPrefabObject* pPrefabObject, const SObjectChangedContext& context, const TObjectIdMapping& guidMapping);
	//! Function to update a instanced prefabs in the level
	void           ModifyInstancedPrefab(CSelectionGroup& objectsInPrefabAsFlatSelection, CPrefabObject* pPrefabObject, const SObjectChangedContext& context, const TObjectIdMapping& guidMapping);
	//! Registers prefab event flowgraph nodes from all prefab instances
	void           RegisterPrefabEventFlowNodes(CBaseObject* const pEntityObj);
	//! Searches for a object inside the container by a prefabId
	CBaseObjectPtr FindObjectByPrefabGuid(const std::vector<CBaseObject*>& objects, CryGUID guidInPrefab);
	//! Extracts all object mapping from a selection (prefabID -> uniqueID)
	void           ExtractObjectsPrefabIDtoGuidMapping(CSelectionGroup& objects, TObjectIdMapping& mapping);
	//! Remaps id to another id, or it returns the id unchanged if no mapping was found
	CryGUID        ResolveID(const TObjectIdMapping& prefabIdToGuidMapping, CryGUID id, bool prefabIdToGuidDirection);
	//! Goes through objectNode and all its children (except Flowgraph) and remaps the ids in the direction specified by the prefabIdToGuidDirection parameter
	void           RemapIDsInNodeAndChildren(XmlNodeRef objectNode, const TObjectIdMapping& mapping, bool prefabIdToGuidDirection);
	//! Function changes the ids XML attributes of objectNode in the direction specified by the prefabIdToGuidDirection parameter
	void           RemapIDsInNode(XmlNodeRef objectNode, const TObjectIdMapping& mapping, bool prefabIdToGuidDirection);
	//! Removes all XML child nodes in m_objectsNode, which has a Parent with the specified GUID
	void           RemoveAllChildsOf(CryGUID guid);

	void           SaveLinkedObjects(CObjectArchive& ar, CBaseObject* pObj, bool bAllowOwnedByPrefab);
	void           CollectLinkedObjects(CBaseObject* pObj, std::vector<CBaseObject*>& linkedObjects, CSelectionGroup& selection);

private:
	XmlNodeRef m_objectsNode;
	string    m_PrefabClassName;
};

#endif // __PrefabItem_h__

