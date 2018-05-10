// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Objects/ObjectLoader.h"
#include "Objects/BaseObject.h"
#include "IObjectManager.h"
#include "QT/Widgets/QWaitProgress.h"
#include "Util/PakFile.h"
#include "IEditorMaterial.h"
#include <CryPhysics/physinterface.h>
#include <CryMovie/IMovieSystem.h>
#include "IUndoManager.h"

//////////////////////////////////////////////////////////////////////////
// CObjectArchive Implementation.
//////////////////////////////////////////////////////////////////////////
CObjectArchive::CObjectArchive(IObjectManager* objMan, XmlNodeRef xmlRoot, bool loading)
{
	m_objectManager = objMan;
	bLoading = loading;
	bUndo = false;
	m_nFlags = 0;
	node = xmlRoot;
	m_pGeometryPak = NULL;
	m_pCurrentObject = NULL;
	m_bNeedResolveObjects = false;
	m_bProgressBarEnabled = true;
}

//////////////////////////////////////////////////////////////////////////
CObjectArchive::~CObjectArchive()
{
	if (m_pGeometryPak)
		delete m_pGeometryPak;
	// Always make sure objects are resolved when loading from archive.
	if (bLoading && m_bNeedResolveObjects)
	{
		ResolveObjects();
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetResolveCallback(CBaseObject* fromObject, const CryGUID& objectId, ResolveObjRefFunctor1& func)
{
	if (objectId == CryGUID::Null())
	{
		func(0);
		return;
	}

	CryGUID guid(objectId);
	if (m_nFlags & eObjectLoader_ReconstructPrefabs)
	{
		for (int i = 0, iLoadedObjectsSize(m_loadedObjects.size()); i < iLoadedObjectsSize; ++i)
		{
			CryGUID guidInPrefab;
			if (!m_loadedObjects[i].xmlNode->getAttr("Id", guidInPrefab))
				continue;
			if (guidInPrefab == objectId)
			{
				guid = m_loadedObjects[i].newGuid;
				break;
			}
		}
	}

	CBaseObject* pObject = m_objectManager->FindObject(guid);
	if (pObject && !m_pGuidProvider)
	{
		// Object is already resolved. immediately call callback.
		func(pObject);
	}
	else
	{
		Callback cb;
		cb.fromObject = fromObject;
		cb.func1 = func;
		m_resolveCallbacks.insert(Callbacks::value_type(guid, cb));
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetResolveCallback(CBaseObject* fromObject, const CryGUID& objectId, ResolveObjRefFunctor2& func, uint32 userData)
{
	if (objectId == CryGUID::Null())
	{
		func(0, userData);
		return;
	}

	CBaseObject* object = m_objectManager->FindObject(objectId);
	if (object && !m_pGuidProvider)
	{
		// Object is already resolved. immediately call callback.
		func(object, userData);
	}
	else
	{
		Callback cb;
		cb.fromObject = fromObject;
		cb.func2 = func;
		cb.userData = userData;
		m_resolveCallbacks.insert(Callbacks::value_type(objectId, cb));
	}
}

//////////////////////////////////////////////////////////////////////////
CryGUID CObjectArchive::ResolveID(const CryGUID& id)
{
	return stl::find_in_map(m_IdRemap, id, id);
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::ResolveObjects(bool processEvents)
{
	if (!bLoading)
	{
		return;
	}

	SerializeObjects(processEvents);

	SortObjects();

	ResolveObjectsGuids();

	CreateObjects(processEvents);

	CallPostLoadOnObjects();
	
	m_bNeedResolveObjects = false;
	m_sequenceIdRemap.clear();
	m_pendingIds.clear();
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SaveObject(CBaseObject* pObject, bool bSaveInGroupObjects, bool bSaveInPrefabObjects)
{
	// Not save objects in prefabs.
	if (!bSaveInPrefabObjects && pObject->CheckFlags(OBJFLAG_PREFAB))
		return;

	// Not save objects in group.
	if ((!bSaveInGroupObjects && pObject->GetGroup() && !pObject->GetLinkedTo()) && !bSaveInPrefabObjects)
		return;

	if (m_savedObjects.find(pObject) == m_savedObjects.end())
	{
		m_pCurrentObject = pObject;
		m_savedObjects.insert(pObject);
		// If this object was not saved before.
		XmlNodeRef objNode = node->newChild("Object");
		XmlNodeRef prevRoot = node;
		node = objNode;

		if (bSaveInPrefabObjects)
			m_nFlags |= eObjectLoader_SavingInPrefabLib;

		pObject->Serialize(*this);
		node = prevRoot;
	}
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectArchive::LoadObject(XmlNodeRef& objNode, CBaseObject* pPrevObject)
{
	XmlNodeRef prevNode = node;
	node = objNode;
	CBaseObject* pObject;
	const bool bLoadInCurrentLayer = (m_nFlags & eObjectLoader_LoadToCurrentLayer) ? true : false;

	pObject = m_objectManager->NewObject(*this, pPrevObject, bLoadInCurrentLayer);
	if (pObject)
	{
		SLoadedObjectInfo obj;
		// Sort order is set after serializing the object
		obj.nSortOrder = 0;
		obj.pObject = pObject;
		obj.newGuid = pObject->GetId();
		obj.xmlNode = node;
		m_loadedObjects.push_back(obj);
		m_bNeedResolveObjects = true;
	}
	node = prevNode;
	return pObject;
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::LoadObjects(XmlNodeRef& rootObjectsNode)
{
	int numObjects = rootObjectsNode->getChildCount();
	for (int i = 0; i < numObjects; i++)
	{
		XmlNodeRef objNode = rootObjectsNode->getChild(i);
		LoadObject(objNode, NULL);
	}
}

void CObjectArchive::LoadInCurrentLayer(bool bEnable)
{
	if (bEnable)
	{
		m_nFlags |= eObjectLoader_LoadToCurrentLayer;
	}
	else
	{
		m_nFlags &= ~(eObjectLoader_LoadToCurrentLayer);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::EnableReconstructPrefabObject(bool bEnable)
{
	if (bEnable)
	{
		m_nFlags |= eObjectLoader_ReconstructPrefabs;
	}
	else
	{
		m_nFlags &= ~(eObjectLoader_ReconstructPrefabs);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::SetShouldResetInternalMembers(bool reset)
{
	if (reset)
	{
		m_nFlags |= eObjectLoader_ResetInternalMembers;
	}
	else
	{
		m_nFlags &= ~(eObjectLoader_ResetInternalMembers);
	}
}

void CObjectArchive::SerializeObjects(bool processEvents)
{
	CWaitProgress wait("Loading Objects", false);
	if (m_bProgressBarEnabled)
	{
		wait.Start();
	}

	GetIEditor()->GetIUndoManager()->Suspend();
	//////////////////////////////////////////////////////////////////////////
	// Serialize All Objects from XML.
	//////////////////////////////////////////////////////////////////////////
	int numObj = m_loadedObjects.size();
	for (int i = 0; i < numObj; i++)
	{
		SLoadedObjectInfo& obj = m_loadedObjects[i];
		node = obj.xmlNode;

		if (m_bProgressBarEnabled)
		{
			wait.Step((i * 100) / numObj, processEvents);
		}

		obj.pObject->Serialize(*this);

		// Objects can be added to the list here (from Groups).
		numObj = m_loadedObjects.size();
	}
	//////////////////////////////////////////////////////////////////////////
	GetIEditor()->GetIUndoManager()->Resume();
}

void CObjectArchive::SortObjects()
{
	// set up the sort order
	for (SLoadedObjectInfo& loadedObject : m_loadedObjects)
	{
		CBaseObject* pObject = loadedObject.pObject;
		while (pObject != nullptr)
		{
			// Increase the sort order by one for each parent this object has
			// This ensure that children are always initialized last
			loadedObject.nSortOrder++;

			if (pObject->GetLinkedTo() != nullptr)
			{
				pObject = pObject->GetLinkedTo();
			}
			else
			{
				pObject = pObject->GetParent();
			}
		}
	}

	// Sort objects by sort order now that serialization is done
	std::stable_sort(m_loadedObjects.begin(), m_loadedObjects.end());
}

void CObjectArchive::ResolveObjectsGuids()
{
	for (Callbacks::iterator it = m_resolveCallbacks.begin(); it != m_resolveCallbacks.end(); it++)
	{
		Callback& cb = it->second;
		CryGUID objectId = ResolveID(it->first);
		CBaseObject* pObject = m_objectManager->FindObject(objectId);
		if (!pObject)
		{
			string from;
			if (cb.fromObject)
			{
				from = cb.fromObject->GetName();
			}
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Unresolved ObjectID: %s, Referenced from Object %s", 
				objectId.ToString(), (const char*)from);

			continue;
		}
		// Call callback with this object.
		if (cb.func1)
		{
			(cb.func1)(pObject);
		}
		if (cb.func2)
		{
			(cb.func2)(pObject, cb.userData);
		}
	}
	m_resolveCallbacks.clear();
}

void CObjectArchive::CreateObjects(bool processEvents)
{
	CWaitProgress wait("Creating Objects", false);
	if (m_bProgressBarEnabled)
	{
		wait.Start();
	}
	int numObj = m_loadedObjects.size();
	for (int i = 0; i < numObj; i++)
	{
		SLoadedObjectInfo& obj = m_loadedObjects[i];

		if (m_bProgressBarEnabled)
		{
			wait.Step((i * 100) / numObj, processEvents);
		}

		obj.pObject->CreateGameObject();
		// remove collisions of hidden objects
		if (obj.pObject->CheckFlags(OBJFLAG_INVISIBLE) || obj.pObject->IsHiddenBySpec())
		{
			IPhysicalEntity* pPhEn = obj.pObject->GetCollisionEntity();
			if (pPhEn && gEnv->pPhysicalWorld)
			{
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(pPhEn, 1);
			}
		}

		IEditorMaterial* pMaterial = obj.pObject->GetRenderMaterial();
		if (pMaterial && pMaterial->GetMatInfo())
		{
			GetIEditor()->OnRequestMaterial(pMaterial->GetMatInfo());
		}
	}
}

void CObjectArchive::CallPostLoadOnObjects()
{
	for (SLoadedObjectInfo& obj : m_loadedObjects)
	{
		node = obj.xmlNode;
		obj.pObject->PostLoad(*this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::RemapID(CryGUID oldId, CryGUID newId)
{
	m_IdRemap[oldId] = newId;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectArchive::GetCurrentObject()
{
	return m_pCurrentObject;
}

//////////////////////////////////////////////////////////////////////////
void CObjectArchive::AddSequenceIdMapping(uint32 oldId, uint32 newId)
{
	assert(oldId != newId);
	assert(GetIEditor()->GetMovieSystem()->FindSequenceById(oldId) || stl::find(m_pendingIds, oldId));
	assert(GetIEditor()->GetMovieSystem()->FindSequenceById(newId) == NULL);
	assert(stl::find(m_pendingIds, newId) == false);
	m_sequenceIdRemap[oldId] = newId;
	m_pendingIds.push_back(newId);
}

//////////////////////////////////////////////////////////////////////////
uint32 CObjectArchive::RemapSequenceId(uint32 id) const
{
	std::map<uint32, uint32>::const_iterator itr = m_sequenceIdRemap.find(id);
	if (itr == m_sequenceIdRemap.end())
		return id;
	else
		return itr->second;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectArchive::IsAmongPendingIds(uint32 id) const
{
	return stl::find(m_pendingIds, id);
}

