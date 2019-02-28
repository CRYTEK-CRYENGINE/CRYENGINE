// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectManager.h"

#include <Preferences/ViewportPreferences.h>

#include "EntityObject.h"
#include "EntityObjectWithComponent.h"
#include "AIWave.h"
#include "Group.h"
#include "ShapeObject.h"
#include "BrushObject.h"
#include "GeomEntity.h"
#include "SimpleEntity.h"
#include "MiscEntities.h"
#include "CameraObject.h"
#include "IAIManager.h"
#include "PrefabObject.h"
#include "Prefabs/PrefabManager.h"
#include "AICoverSurface.h"
#include "RefPicture.h"
#include "Viewport.h"
#include "Gizmos/GizmoManager.h"
#include "Gizmos/TransformManipulator.h"
#include "ObjectLayerManager.h"
#include "ObjectPhysicsManager.h"
#include "CryEdit.h"
#include "QtUtil.h"

#include "EditMode\ObjectMode.h"

#include <CryAISystem/IAgent.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <io.h>

#include "Geometry\EdMesh.h"

#include "Material\MaterialManager.h"

#include <CryAISystem/IAIObject.h>
#include "../EditMode/DeepSelection.h"
#include "Objects\EnvironmentProbeObject.h"
#include "Util/CubemapUtils.h"

#include "HyperGraph\FlowGraphManager.h"

#include "Util/BoostPythonHelpers.h"
#include "HyperGraph/FlowGraphHelpers.h"
#include "GameEngine.h"

#include <CryNetwork/IRemoteCommand.h>

#include <CryCore/ToolsHelpers/GuidUtil.h>
#include <regex>
#include <QApplication>
#include <QMenu>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include "EditorFramework/BroadcastManager.h"
#include "EditorFramework/Events.h"
#include "Objects/ObjectPropertyWidget.h"
#include "QT/QtMainFrame.h"
#include <CrySandbox/ScopedVariableSetter.h>

#include "IObjectEnumerator.h"
#include "QT/Widgets/QWaitProgress.h"
#include "Controls/QuestionDialog.h"

#include <FilePathUtil.h>
#include <CrySystem/ICryLink.h>

#include "Gizmos/ITransformManipulator.h"

#include "Objects/InspectorWidgetCreator.h"
#include "Objects/GuidCollisionResolver.h"

#include <QAdvancedPropertyTree.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/env/Elements/EnvComponent.h>
#include <DefaultComponents/Cameras/CameraComponent.h>

namespace Private_ObjectManager
{
void FindSelectableObjectsInRectRecursive(CObjectManager* manager, CViewport* view, CBaseObject* obj, HitContext& hc, std::vector<CBaseObject*>& selectableObjects)
{
	if (obj->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		CGroup* pGroup = static_cast<CGroup*>(obj);
		// If the group is open check children
		if (pGroup->IsOpen())
		{
			for (int j = 0; j < pGroup->GetChildCount(); ++j)
			{
				FindSelectableObjectsInRectRecursive(manager, view, pGroup->GetChild(j), hc, selectableObjects);
			}
		}
		else if (manager->IsObjectSelectableInHitContext(view, hc, obj))
		{
			selectableObjects.push_back(obj);
		}
	}
	else if (manager->IsObjectSelectableInHitContext(view, hc, obj))
	{
		selectableObjects.push_back(obj);
	}
}

void StoreUndoLinkInformation(const std::vector<CBaseObject*>& objects)
{
	if (!CUndo::IsRecording())
	{
		return;
	}

	// TODO: These exceptions can be removed by implementing global object archive per undo step
	// if the object contains an entity which has link, the link information should be recorded for undo separately.
	for (auto pObject : objects)
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = (CEntityObject*)pObject;
			if (pEntity->GetEntityLinkCount() > 0)
			{
				CEntityObject::StoreUndoEntityLink(objects);
			}
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CShapeObject)))
		{
			CShapeObject* pShape = (CShapeObject*)pObject;
			if (pShape->GetEntityCount() > 0)
			{
				CShapeObject::StoreUndoShapeTargets(objects);
			}
		}
	}
}

void ProcessDeletionPerType(CObjectManager* pManager, const std::vector<CBaseObject*>& objects)
{
	for (auto pObject : objects)
	{
		// Check if object is a group then delete all children.
		if (pObject->IsKindOf(RUNTIME_CLASS(CGroup)))
		{
			((CGroup*)pObject)->DeleteAllMembers();
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CAITerritoryObject)))
		{
			pManager->FindAndRenameProperty2("aiterritory_Territory", pObject->GetName(), "<None>");
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CAIWaveObject)))
		{
			pManager->FindAndRenameProperty2("aiwave_Wave", pObject->GetName(), "<None>");
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
		{
			CCameraObject* pCamera = (CCameraObject*)pObject;
			CBaseObjectPtr lookat = pCamera->GetLookAt();

			// If look at is also camera class, delete lookat target.
			if (lookat && lookat->IsKindOf(RUNTIME_CLASS(CCameraObjectTarget)))
			{
				pManager->DeleteObject(lookat);
			}
		}
	}
}

AABB GetBoundingBoxForObjects(const std::vector<CBaseObject*>& objects)
{
	AABB totalAAB;
	AABB objAAB;
	if (!objects.empty())
	{
		objects[0]->GetBoundBox(totalAAB);
	}
	for (int i = 1; i < objects.size(); ++i)
	{
		objects[i]->GetBoundBox(objAAB);
		totalAAB.Add(objAAB);
	}
	return totalAAB;
}

void UpdateParentsOfDeletedObjects(const std::vector<CGroup*>& parents)
{
	for (auto pParent : parents)
	{
		pParent->UpdateGroup();
	}
}

class CObjectsRefOwner
{
public:
	CObjectsRefOwner(const std::vector<CBaseObject*>& objects)
		: m_objects(objects)
	{
		for (auto pObject : m_objects)
		{
			pObject->AddRef();
		}
	}

	void Release()
	{
		if (m_isReleased)
		{
			return;
		}

		m_isReleased = true;
		for (auto pObject : m_objects)
		{
			pObject->Release();
		}
	}

	~CObjectsRefOwner()
	{
		Release();
	}

private:
	const std::vector<CBaseObject*>& m_objects;
	bool                             m_isReleased = { false };
};

//////////////////////////////////////////////////////////////////////////
//! Prefab helper functions
// Because a prefab consist of multiple object but it is defined as a single GUID,
// we want to extract all the children GUIDs. We need this info if we undo redo new/delete
// operations on prefabs, since we want to NOT generate new IDs for the children, otherwise
// other UNDO operations depending on the guids won't work
void ExtractRemapingInformation(CBaseObject* pPrefab, TGUIDRemap& remapInfo)
{
	std::vector<CBaseObject*> children;
	pPrefab->GetAllPrefabFlagedChildren(children);

	for (size_t i = 0, count = children.size(); i < count; ++i)
	{
		remapInfo.insert(std::make_pair(children[i]->GetIdInPrefab(), children[i]->GetId()));
	}
}

void RemapObjectsInPrefab(CBaseObject* pPrefab, const TGUIDRemap& remapInfo)
{
	IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();

	std::vector<CBaseObject*> children;
	pPrefab->GetAllPrefabFlagedChildren(children);

	for (size_t i = 0, count = children.size(); i < count; ++i)
	{
		TGUIDRemap::const_iterator it = remapInfo.find(children[i]->GetIdInPrefab());
		if (it != remapInfo.end())
		{
			pObjMan->ChangeObjectId(children[i]->GetId(), (*it).second);
		}
	}
}

}

/*!
 *	Class Description used for object templates.
 *	This description filled from Xml template files.
 */
class CXMLObjectClassDesc : public CObjectClassDesc
{
public:
	CObjectClassDesc* superType;
	string            type;
	string            category;
	string            fileSpec;
	string            dataFilter;

public:
	CXMLObjectClassDesc()
	{
		CEntityScriptRegistry::Instance()->m_scriptAdded.Connect(this, &CXMLObjectClassDesc::OnScriptAdded);
		CEntityScriptRegistry::Instance()->m_scriptChanged.Connect(this, &CXMLObjectClassDesc::OnScriptChanged);
		CEntityScriptRegistry::Instance()->m_scriptRemoved.Connect(this, &CXMLObjectClassDesc::OnScriptRemoved);
	}
	~CXMLObjectClassDesc()
	{
		CEntityScriptRegistry::Instance()->m_scriptAdded.DisconnectObject(this);
		CEntityScriptRegistry::Instance()->m_scriptChanged.DisconnectObject(this);
		CEntityScriptRegistry::Instance()->m_scriptRemoved.DisconnectObject(this);
	}

	ObjectType     GetObjectType()   { return superType->GetObjectType(); };
	const char*    ClassName()       { return type; };
	const char*    Category()        { return category; };
	CRuntimeClass* GetRuntimeClass() { return superType->GetRuntimeClass(); };
	const char*    GetTextureIcon()  { return superType->GetTextureIcon(); };
	const char*    GetFileSpec()
	{
		if (!fileSpec.IsEmpty())
			return fileSpec;
		else
			return superType->GetFileSpec();
	};
	static bool GetScriptData(CEntityScript* pScript, string& file, string& name)
	{
		name = pScript->GetName();
		file = pScript->GetFile();

		const SEditorClassInfo& editorClassInfo = pScript->GetClass()->GetEditorClassInfo();
		string clsCategory = editorClassInfo.sCategory;

		string entityFilePathStr(file);

		if (entityFilePathStr.MakeLower().Find("scripts/entities/sound/") >= 0)
		{
			return false;
		}

		if (clsCategory == "Schematyc")
		{
			// Schematyc class names are already fully qualified so we just need to display the correct folder structure.
			file = name;
			file.replace("::", "/");
		}
		else if (!clsCategory.IsEmpty())
		{
			file = clsCategory + "/" + name;
		}
		else if (file.IsEmpty())
		{
			// no script file: get the path from the Editor script table instead
			file = pScript->GetDisplayPath();
		}
		else
		{
			file = PathUtil::ReplaceFilename(file.GetString(), name.GetString());
			file.Replace("Scripts/Entities/", "");
		}
		file = PathUtil::RemoveExtension(file.GetString());
		return true;
	}
	void OnScriptAdded(CEntityScript* pScript)
	{
		string name;
		string clsFile;
		if (GetScriptData(pScript, clsFile, name))
		{
			if (pScript->IsUsable())
				m_itemAdded(clsFile, name, "");
			else
				m_itemRemoved(clsFile, name);
		}
	}
	void OnScriptChanged(CEntityScript* pScript)
	{
		string name;
		string clsFile;
		if (GetScriptData(pScript, clsFile, name))
		{
			if (pScript->IsUsable())
				m_itemChanged(clsFile, name, "");
			else
				m_itemRemoved(clsFile, name);
		}
	}
	void OnScriptRemoved(CEntityScript* pScript)
	{
		string name;
		string clsFile;
		if (GetScriptData(pScript, clsFile, name))
			m_itemRemoved(clsFile, name);
	}
	virtual const char* GetDataFilesFilterString() override { return dataFilter; }
	virtual void        EnumerateObjects(IObjectEnumerator* pTree) override
	{
		CEntityScriptRegistry::Instance()->Reload();

		CEntityScriptRegistry::Instance()->IterateOverScripts([this, pTree](CEntityScript& script)
		{
			string clsFile;
			string name;
			if (script.IsUsable() && GetScriptData(&script, clsFile, name))
			{
			  pTree->AddEntry(clsFile, name);
			}
		});
	}
	virtual bool IsCreatable() const override
	{
		// Hide entities whose classes don't exist
		if (!strcmp(superType->ClassName(), "StdEntity") && fileSpec.size() > 0)
		{
			IEntityClass* pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass(fileSpec);
			if (pClass == nullptr)
			{
				return false;
			}
		}

		// Special case for the legacy entity group, only show if there are scripts inside
		if (!strcmp(type, "Entity"))
		{
			bool bHasUsableScripts = false;

			CEntityScriptRegistry::Instance()->IterateOverScripts([&bHasUsableScripts](CEntityScript& script)
			{
				string clsFile;
				string name;
				if (!bHasUsableScripts && script.IsUsable() && CXMLObjectClassDesc::GetScriptData(&script, clsFile, name))
				{
				  bHasUsableScripts = true;
				}
			});

			if (!bHasUsableScripts)
			{
				return false;
			}
		}

		return true;
	}
};

//////////////////////////////////////////////////////////////////////////
//! Undo New Object
class CUndoBaseObjectNew : public IUndoObject
{
public:
	CUndoBaseObjectNew(CBaseObject* pObj)
	{
		using namespace Private_ObjectManager;
		m_object = pObj;

		m_isPrefab = pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject));

		if (m_isPrefab)
			ExtractRemapingInformation(pObj, m_remaping);
	}
protected:
	virtual const char* GetDescription() override { return "New BaseObject"; };
	virtual const char* GetObjectName()  override { return m_object->GetName(); };

	virtual void        Undo(bool bUndo) override
	{
		if (bUndo)
		{
			m_redo = XmlHelpers::CreateXmlNode("Redo");
			// Save current object state.
			CObjectArchive ar(GetIEditorImpl()->GetObjectManager(), m_redo, false);
			ar.bUndo = true;
			m_object->Serialize(ar);
			m_object->SetLayerModified();
		}

		m_object->UpdatePrefab(eOCOT_Delete);

		// Delete this object.
		GetIEditorImpl()->DeleteObject(m_object);
	}
	virtual void Redo() override
	{
		using namespace Private_ObjectManager;
		if (m_redo)
		{
			IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();
			{
				CObjectArchive ar(pObjMan, m_redo, true);
				ar.bUndo = true;
				ar.SetGuidProvider(nullptr);
				ar.LoadObject(m_redo, m_object);
			}
			pObjMan->ClearSelection();
			pObjMan->SelectObject(m_object);
			m_object->SetLayerModified();

			if (m_isPrefab)
				RemapObjectsInPrefab(m_object, m_remaping);

			m_object->UpdatePrefab(eOCOT_Add);
		}
	}

private:
	CBaseObjectPtr m_object;
	TGUIDRemap     m_remaping;
	XmlNodeRef     m_redo;
	bool           m_isPrefab;
};

//////////////////////////////////////////////////////////////////////////
//! Undo Delete Object
class CUndoBaseObjectDelete : public IUndoObject
{
public:
	CUndoBaseObjectDelete(CBaseObject* pObj)
	{
		using namespace Private_ObjectManager;
		pObj->SetTransformDelegate(nullptr, true);
		m_object = pObj;

		// Must save the list of linked objects because they are not serialized at the parent level
		// linking happens from child to parent
		for (auto i = 0; i < m_object->GetLinkedObjectCount(); ++i)
			m_linkedObjectGuids.push_back(m_object->GetLinkedObject(i)->GetId());

		// Save current object state.
		m_undo = XmlHelpers::CreateXmlNode("Undo");
		CObjectArchive ar(GetIEditorImpl()->GetObjectManager(), m_undo, false);
		ar.bUndo = true;
		m_bSelected = m_object->IsSelected();
		m_object->Serialize(ar);

		m_isPrefab = pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject));

		if (m_isPrefab)
			ExtractRemapingInformation(m_object, m_remaping);
	}
protected:
	virtual const char* GetDescription() override { return "Delete BaseObject"; };
	virtual const char* GetObjectName() override  { return m_object->GetName(); };

	virtual void        Undo(bool bUndo) override
	{
		using namespace Private_ObjectManager;
		IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();
		{
			CObjectArchive ar(pObjMan, m_undo, true);
			ar.bUndo = true;
			ar.SetGuidProvider(nullptr);
			ar.LoadObject(m_undo, m_object);
			m_object->ClearFlags(OBJFLAG_SELECTED);
		}
		if (m_bSelected)
		{
			pObjMan->SelectObject(m_object);
		}
		m_object->SetLayerModified();

		for (auto i = 0; i < m_linkedObjectGuids.size(); ++i)
		{
			if (CBaseObject* pLinkedObject = pObjMan->FindObject(m_linkedObjectGuids[i]))
				pLinkedObject->LinkTo(m_object);
		}

		if (m_isPrefab)
			RemapObjectsInPrefab(m_object, m_remaping);

		m_object->UpdatePrefab(eOCOT_Add);
	}
	virtual void Redo()
	{
		// Delete this object.
		GetIEditorImpl()->DeleteObject(m_object);
	}

private:
	CBaseObjectPtr       m_object;
	std::vector<CryGUID> m_linkedObjectGuids;
	XmlNodeRef           m_undo;
	TGUIDRemap           m_remaping;
	bool                 m_bSelected;
	bool                 m_isPrefab;
};

//////////////////////////////////////////////////////////////////////////
//! Undo Select Object
class CUndoBaseObjectSelect : public IUndoObject
{
public:
	CUndoBaseObjectSelect(CBaseObject* pObj)
	{
		CRY_ASSERT(pObj != 0);
		m_objectsGuids = { pObj->GetId() };
		m_bUndoSelect = !pObj->IsSelected();
	}

	CUndoBaseObjectSelect(bool select, const std::vector<CBaseObject*>& objects = {})
		: m_bUndoSelect(select)
	{
		m_objectsGuids.reserve(objects.size());
		for (auto pObject : objects)
		{
			m_objectsGuids.push_back(pObject->GetId());
		}
	}
protected:
	virtual void        Release()                 { delete this; };
	virtual const char* GetDescription() override { return "Select Objects"; };
	virtual const char* GetObjectName() override
	{
		if (m_objectsGuids.size() == 1)
		{
			CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_objectsGuids[0]);
			if (pObject)
				return pObject->GetName();
		}

		string str;
		str.Format("%d objects", m_objectsGuids.size());
		return str.c_str();
	}

	virtual void Undo(bool bUndo) override
	{
		Select(!m_bUndoSelect);
	}

	virtual void Redo() override
	{
		Select(m_bUndoSelect);
	}

private:
	void Select(bool bSelect)
	{
		if (m_objectsGuids.empty())
		{
			return;
		}

		std::vector<CBaseObject*> objects;
		objects.reserve(m_objectsGuids.size());
		for (int i = m_objectsGuids.size() - 1; i >= 0; --i)
		{
			CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_objectsGuids[i]);
			if (!pObject)
				continue;

			objects.push_back(pObject);
		}

		if (objects.empty())
			return;

		if (bSelect)
		{
			if (objects.size() > 1)
				GetIEditorImpl()->GetObjectManager()->SelectObjects(objects);
			else
				GetIEditorImpl()->GetObjectManager()->SelectObject(objects[0]);
		}
		else
		{
			if (objects.size() > 1)
				GetIEditorImpl()->GetObjectManager()->UnselectObjects(objects);
			else
				GetIEditorImpl()->GetObjectManager()->UnselectObject(objects[0]);
		}

	}

	std::vector<CryGUID> m_objectsGuids;
	bool                 m_bUndoSelect;
	bool                 m_bRedoSelect;
};

CBatchProcessDispatcher::~CBatchProcessDispatcher()
{
	Finish();
}

void CBatchProcessDispatcher::Start(const CBaseObjectsArray& objects, bool force /*= false*/)
{
	if (objects.size() > 1 || force)
	{
		m_shouldDispatch = true;
		GetIEditorImpl()->GetObjectManager()->signalBatchProcessStarted(objects);
	}
}

void CBatchProcessDispatcher::Finish()
{
	if (m_shouldDispatch)
	{
		m_shouldDispatch = false;
		GetIEditorImpl()->GetObjectManager()->signalBatchProcessFinished();
	}
}

CryGUID CRandomUniqueGuidProvider::GetFrom(const CryGUID&) const
{
	CryGUID newGuid;
	do
	{
		newGuid = CryGUID::Create();
	}
	while (GetIEditorImpl()->GetObjectManager()->FindObject(newGuid));
	return newGuid;
}

CryGUID IGuidProvider::GetFor(CBaseObject* pObject) const
{
	return GetFrom(pObject->GetIdInPrefab());
}

//////////////////////////////////////////////////////////////////////////
// CObjectManager implementation.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CObjectManager::CObjectManager()
	: m_bLayerChanging(false)
{
	m_createGameObjects = true;

	m_bVisibleObjectValid = true;
	m_lastHideMask = 0;

	m_pLoadProgress = 0;
	m_totalObjectsToLoad = 0;
	m_loadedObjects = 0;

	m_isUpdateVisibilityList = false;

	m_maxObjectViewDistRatio = 0.00001f;

	// Creates objects layers manager.
	m_pLayerManager = new CObjectLayerManager(this);
	m_pPhysicsManager = new CObjectPhysicsManager;

	m_bLevelExporting = false;
	m_bLoadingObjects = false;

	m_forceID = 0;

	RegisterTypeConverter(RUNTIME_CLASS(CBrushObject), "Entity", [](CBaseObject* pObject)
	{
		if (CBaseObjectPtr pNewObject = GetIEditorImpl()->NewObject("EntityWithStaticMeshComponent"))
		{
		  if (pNewObject->ConvertFromObject(pObject))
		  {
		    GetIEditorImpl()->GetObjectManager()->DeleteObject(pObject);
		    return;
		  }

		  // Conversion should never fail!
		  CRY_ASSERT(false);
		}
	});

	RegisterTypeConverter(RUNTIME_CLASS(CEntityObject), "Brush", [](CBaseObject* pObject)
	{
		if (CBaseObjectPtr pNewObject = GetIEditorImpl()->NewObject("Brush"))
		{
		  if (pNewObject->ConvertFromObject(pObject))
		  {
		    GetIEditorImpl()->GetObjectManager()->DeleteObject(pObject);
		    return;
		  }

		  // Conversion should never fail!
		  CRY_ASSERT(false);
		}
	});
}

//////////////////////////////////////////////////////////////////////////
CObjectManager::~CObjectManager()
{
	ClearSelection();
	DeleteAllObjects();

	if (m_pLayerManager)
		delete m_pLayerManager;

	delete m_pPhysicsManager;
}

//////////////////////////////////////////////////////////////////////////
IObjectLayerManager* CObjectManager::GetIObjectLayerManager() const
{
	return m_pLayerManager;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::CanCreateObject() const
{
	return (m_pLayerManager->GetCurrentLayer() != nullptr);
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::NewObject(CObjectClassDesc* cls, CBaseObject* prev, const string& file)
{
	CRY_ASSERT(cls != 0);

	if (!CanCreateObject())
	{
		return nullptr;
	}

	CRuntimeClass* rtClass = cls->GetRuntimeClass();
	CRY_ASSERT(rtClass->IsDerivedFrom(RUNTIME_CLASS(CBaseObject)));
	if (prev)
	{
		// Both current and previous object must be of same type.
		CRY_ASSERT(cls == prev->GetClassDesc());
	}

	// Suspend undo operations when initializing object.
	GetIEditorImpl()->GetIUndoManager()->Suspend();

	CBaseObjectPtr obj;
	{
		obj = (CBaseObject*)rtClass->CreateObject();
		obj->SetClassDesc(cls);
		obj->SetScriptName(file, prev);
		obj->SetLayer(m_pLayerManager->GetCurrentLayer());
		obj->InitVariables();
		obj->m_guid = CRandomUniqueGuidProvider().GetFor(obj); 

		if (obj->Init(prev, file))
		{
			if (obj->GetName().IsEmpty())
			{
				obj->SetName(GenUniqObjectName(cls->ClassName()));
			}

			// Create game object itself.
			obj->CreateGameObject();

			if (!AddObject(obj))
				obj = 0;

			if (obj)
				obj->PostInit(file);
		}
		else
		{
			obj = 0;
		}
	}

	GetIEditorImpl()->GetIUndoManager()->Resume();

	if (obj != 0 && GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoBaseObjectNew(obj));
	}

	return obj;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::NewObject(CObjectArchive& ar, CBaseObject* pUndoObject, bool bLoadInCurrentLayer)
{
	using namespace Private_ObjectManager;
	XmlNodeRef objNode = ar.node;

	// Load all objects from XML.
	string typeName;
	CryGUID id = CryGUID::Null();
	CryGUID idInPrefab = CryGUID::Null();

	if (!objNode->getAttr("Type", typeName))
	{
		return nullptr;
	}

	if (!objNode->getAttr("Id", id))
	{
		// Make new ID for object that doesn't have if.
		id = CryGUID::Create();
	}

	idInPrefab = id;

	if (ar.GetGuidProvider())
	{
		// Make new guid for this object.
		CryGUID newId;
		newId = ar.GetGuidProvider()->GetFrom(id);
		ar.RemapID(id, newId);  // Mark this id remapped.
		id = newId;
	}

	CBaseObjectPtr pObject;
	if (pUndoObject)
	{
		// if undoing restore object pointer.
		pObject = pUndoObject;
		// if reading an object an object make sure it's not dead
		pObject->ClearFlags(OBJFLAG_DELETED);

		// Make sure to re-set the layer that object belongs to because when performing an undo it might be pointing to a stale
		// layer pointer
		string layerName;
		if (objNode->getAttr("Layer", layerName))
		{
			CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayerByName(layerName);
			if (pLayer)
			{
				pObject->SetLayer(pLayer);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Could not find layer %s", (const char*)layerName);
				return nullptr;
			}
		}
	}
	else
	{
		// New object creation.

		// Suspend undo operations when initializing object.
		CScopedSuspendUndo undoSuspender;

		string entityClass;
		if (objNode->getAttr("EntityClass", entityClass))
		{
			typeName = typeName + "::" + entityClass;
		}

		CObjectClassDesc* cls = FindClass(typeName);
		if (!cls)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "RuntimeClass %s not registered", (const char*)typeName);
			return nullptr;
		}
		if (*cls->GetSuccessorClassName() != 0)
		{
			cls = FindClass(cls->GetSuccessorClassName());
		}

		CRuntimeClass* rtClass = cls->GetRuntimeClass();
		CRY_ASSERT(rtClass->IsDerivedFrom(RUNTIME_CLASS(CBaseObject)));

		pObject = (CBaseObject*)rtClass->CreateObject();
		pObject->SetClassDesc(cls);
		pObject->SetScriptName(typeName, pUndoObject);
		pObject->m_guid = id;

		string objName;
		objNode->getAttr("Name", objName);
		pObject->m_name = objName;

		string layerName;
		if (bLoadInCurrentLayer)
		{
			pObject->SetLayer(GetLayersManager()->GetCurrentLayer());
		}
		else
		{
			if (objNode->getAttr("Layer", layerName))
			{
				CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayerByName(layerName);
				if (pLayer)
				{
					pObject->SetLayer(pLayer);
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Could not find layer %s for object %s", (const char*)layerName, (const char*)objName);
					return nullptr;
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Object %s is missing Layer information", (const char*)objName);
				return nullptr;
			}
		}

		pObject->InitVariables();
		pObject->SetIdInPrefab(idInPrefab);

		// @FIXME: Make sure this id not taken.
		CBaseObject* obj = FindObject(pObject->GetId());

		if (obj && !CGuidCollisionResolver(pObject, ar).Resolve())
		{
			string error;
			error.Format(_T("[Error] Object %s in layer %s is a duplicate of object %s in layer %s. %s has been removed. %s"),
			             (const char*)pObject->GetName(), (const char*)pObject->GetLayer()->GetName(), (const char*)obj->GetName(),
			             (const char*)obj->GetLayer()->GetName(), pObject->GetName(),
			             CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", obj->GetName()));

			CLogFile::WriteLine(error);

			if (!GetIEditorImpl()->IsInTestMode() && !GetIEditorImpl()->IsInLevelLoadTestMode())
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, (const char*)error);
			}

			return nullptr;
		}
	}

	if (!pObject->Init(0, ""))
	{
		return nullptr;
	}

	if (!AddObject(pObject))
	{
		return nullptr;
	}

	if (!pObject->GetLayer())
	{
		// Cannot be.
		CRY_ASSERT(0);
	}

	if (pObject != 0 && pUndoObject == 0)
	{
		// If new object with no undo, record it.
		if (CUndo::IsRecording())
			GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoBaseObjectNew(pObject));
	}

	m_loadedObjects++;
	if (m_pLoadProgress && m_totalObjectsToLoad > 0)
		m_pLoadProgress->Step((m_loadedObjects * 100) / m_totalObjectsToLoad, true);

	return pObject;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::NewObject(const string& typeName, CBaseObject* prev, const string& file)
{
	// [9/22/2009 evgeny] If it is "Entity", figure out if a CEntity subclass is actually needed
	string fullName = typeName + "::" + file;
	CObjectClassDesc* cls = FindClass(fullName);
	if (!cls)
	{
		cls = FindClass(typeName);
	}

	if (!cls)
	{
		GetIEditorImpl()->GetSystem()->GetILog()->Log("Warning: RuntimeClass %s (as well as %s) not registered", (const char*)typeName, (const char*)fullName);
		return 0;
	}
	CBaseObject* pObject = NewObject(cls, prev, file);
	return pObject;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::DeleteObject(CBaseObject* obj)
{
	std::vector<CBaseObject*> objects = { obj };
	DeleteObjects(objects);
}

void CObjectManager::DeleteObjects(std::vector<CBaseObject*>& objects)
{
	LOADING_TIME_PROFILE_SECTION

	using namespace Private_ObjectManager;

	FilterOutDeletedObjects(objects);

	if (objects.empty())
	{
		return;
	}

	CObjectsRefOwner objectsRefLock(objects);

	CBatchProcessDispatcher batchProcessDispatcher;
	batchProcessDispatcher.Start(objects);

	UnselectObjects(objects);
	StoreUndoLinkInformation(objects);

	for (auto pObject : objects)
	{
		// Update unique names so this name can be re-used if necessary
		UpdateRegisterObjectName(pObject->GetName());
	}

	ProcessDeletionPerType(this, objects);

	// we need to check this condition again because some objects might be already removed 
	// in ProcessDeletionPerType method, and specifically when deleting members of a group.
	FilterOutDeletedObjects(objects);

	UnlinkObjects(objects);

	std::vector<CGroup*> parents;
	CGroup::ForEachParentOf(objects, [&parents](CGroup* pGroup, std::vector<CBaseObject*>& children)
	{
		// First detach, then delete. Not the other way around
		if (pGroup)
		{
		  parents.push_back(pGroup);
		  bool bSuspended(pGroup->SuspendUpdate());
		  pGroup->RemoveMembers(children, true, true);
		  if (bSuspended)
		  {
		    pGroup->ResumeUpdate();
		  }
		}
	});

	CObjectLayer::ForEachLayerOf(objects, [this](CObjectLayer* pLayer, const std::vector<CBaseObject*>& layerObjects)
	{
		signalBeforeObjectsDeleted(*pLayer, layerObjects);
	});

	for (auto pObject : objects)
	{
		NotifyObjectListeners(pObject, OBJECT_ON_PREDELETE);
		pObject->NotifyListeners(OBJECT_ON_PREDELETE);
	}

	for (auto pObject : objects)
	{
		// Must be after object DetachAll to support restoring Parent/Child relations.
		if (CUndo::IsRecording())
		{
			// Store undo for all child objects.
			for (int i = 0; i < pObject->GetChildCount(); i++)
			{
				pObject->GetChild(i)->StoreUndo("DeleteParent");
			}
			CUndo::Record(new CUndoBaseObjectDelete(pObject));
		}

		OnObjectModified(pObject, true, false);
	}

	GetIEditorImpl()->GetAIManager()->OnAreaModified(GetBoundingBoxForObjects(objects));

	//TODO : Most likely this should be called if we sent predelete: //pObject->NotifyListeners(OBJECT_ON_DELETE);
	//right now it is called within pObject->Done() which seems like a really obfuscated approach and probably wrong
	for (auto pObject : objects)
	{
		pObject->Done();
	}

	RemoveObjectsAndNotify(objects);

	UpdateParentsOfDeletedObjects(parents);

	batchProcessDispatcher.Finish();
}

void CObjectManager::FilterOutDeletedObjects(std::vector<CBaseObject*>& objects)
{
	objects.erase(std::remove_if(objects.begin(), objects.end(), [](CBaseObject* pObject)
	{
		return !pObject || pObject->CheckFlags(OBJFLAG_DELETED);
	}), objects.end());
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::DeleteAllObjects()
{
	LOADING_TIME_PROFILE_SECTION
	CPrefabManager::SkipPrefabUpdate skipUpdates;

	ClearSelection();

	InvalidateVisibleList();

	TBaseObjects rootObjects;
	GetAllObjects(rootObjects);

	auto it = std::partition(rootObjects.begin(), rootObjects.end(), [](auto pObject)
	{
		return pObject->GetParent() == nullptr;
	});
	rootObjects.erase(it, rootObjects.end());

	for (auto pRootObject : rootObjects)
	{
		pRootObject->Done();
	}

	// Clear map.
	m_objects.clear();
	//! Delete object instances.
	rootObjects.clear();

	// Clear name map.
	m_nameNumbersMap.clear();
}

CBaseObject* CObjectManager::CloneObject(CBaseObject* obj)
{
	CRY_ASSERT(obj);

	CBaseObject* clone = NewObject(obj->GetClassDesc(), obj);
	return clone;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::FindObject(const CryGUID& guid) const
{
	CBaseObject* result = stl::find_in_map(m_objects, guid, (CBaseObject*)0);
	return result;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::FindObject(const char* objectName, CRuntimeClass* pRuntimeClass) const
{
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* pObj = it->second;
		if (strcmp(pObj->GetName(), objectName) == 0)
		{
			if (pRuntimeClass)
			{
				if (pObj->IsKindOf(pRuntimeClass))
					return pObj;
				else
					return NULL;
			}
			return pObj;
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::FindObjectsOfType(const CRuntimeClass* pClass, std::vector<CBaseObject*>& result)
{
	result.clear();

	CBaseObjectsArray objects;
	GetObjects(objects);

	for (size_t i = 0, n = objects.size(); i < n; ++i)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->IsKindOf(pClass))
		{
			result.push_back(pObject);
		}
	}
}

void CObjectManager::FindObjectsOfType(ObjectType type, std::vector<CBaseObject*>& result)
{
	result.clear();

	CBaseObjectsArray objects;
	GetObjects(objects);

	for (size_t i = 0, n = objects.size(); i < n; ++i)
	{
		if (objects[i]->GetType() == type)
			result.push_back(objects[i]);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::FindObjectsInAABB(const AABB& aabb, std::vector<CBaseObject*>& result) const
{
	result.clear();

	CBaseObjectsArray objects;
	GetObjects(objects);

	for (size_t i = 0, n = objects.size(); i < n; ++i)
	{
		CBaseObject* pObject = objects[i];
		AABB aabbObj;
		pObject->GetBoundBox(aabbObj);
		if (aabb.IsIntersectBox(aabbObj))
		{
			result.push_back(pObject);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::AddObject(CBaseObject* obj)
{
	CBaseObjectPtr p = stl::find_in_map(m_objects, obj->GetId(), 0);
	if (p)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "New Object %s has Duplicate GUID %s, New Object Ignored %s",
		           (const char*)obj->GetName(), obj->GetId().ToString(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", obj->GetName()));

		return false;
	}
	m_objects[obj->GetId()] = obj;
	RegisterObjectName(obj->GetName());
	InvalidateVisibleList();
	NotifyObjectListeners(obj, OBJECT_ON_ADD);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RemoveObject(CBaseObject* pObject)
{
	CRY_ASSERT(pObject != 0);
	RemoveObjects({ pObject });
}

void CObjectManager::RemoveObjects(const std::vector<CBaseObject*>& objects)
{
	UnselectObjects(objects);
	InvalidateVisibleList();

	for (auto pObject : objects)
	{
		m_objects.erase(pObject->GetId());
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GetAllObjects(TBaseObjects& objects) const
{
	objects.clear();
	objects.reserve(m_objects.size());
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		objects.push_back(it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ChangeObjectId(const CryGUID& oldGuid, const CryGUID& newGuid)
{
	Objects::iterator it = m_objects.find(oldGuid);
	if (it != m_objects.end())
	{
		CBaseObjectPtr pRemappedObject = (*it).second;
		pRemappedObject->SetId(newGuid);
		m_objects.erase(it);
		m_objects.insert(std::make_pair(newGuid, pRemappedObject));
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::NotifyPrefabObjectChanged(CBaseObject* pObject)
{
	NotifyObjectListeners(pObject, OBJECT_ON_PREFAB_CHANGED);
	// When an object is added or removed from a prefab we must update the current list of visible objects
	InvalidateVisibleList();
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::IsInvalidObjectName(const string& newName) const
{
	// Object names are not allowed to contain '%'
	std::regex invalidObjectName(".*%.*");
	return std::regex_match(newName.GetString(), invalidObjectName);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ShowDuplicationMsgWarning(CBaseObject* obj, const string& newName, bool bShowMsgBox) const
{
	CBaseObject* pExisting = FindObject(newName);
	if (pExisting)
	{
		string sRenameWarning("");
		sRenameWarning.Format
		(
		  "%s \"%s\" was NOT renamed to \"%s\" because %s with the same name already exists.",
		  obj->GetClassDesc()->ClassName(),
		  obj->GetName().GetString(),
		  newName.GetString(),
		  pExisting->GetClassDesc()->ClassName()
		);

		if (bShowMsgBox)
		{
			CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(sRenameWarning));
		}

		// CryWarning uses printf internally
		sRenameWarning.Replace("%", "%%");
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, sRenameWarning);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ShowInvalidNameMsgWarning(CBaseObject* obj, const string& newName, bool bShowMsgBox) const
{
	string sRenameWarning("");
	sRenameWarning.Format
	(
	  "%s \"%s\" was NOT renamed to \"%s\" because that name is invalid.",
	  obj->GetClassDesc()->ClassName(),
	  obj->GetName().GetString(),
	  newName.GetString()
	);

	if (bShowMsgBox)
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(sRenameWarning));
	}

	// CryWarning uses printf internally
	sRenameWarning.Replace("%", "%%");
	CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, sRenameWarning);
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::ChangeObjectName(CBaseObject* pObject, const string& newName, bool bGenerateUnique /* = false*/)
{
	CRY_ASSERT(pObject);

	if (newName != pObject->GetName())
	{
		if (IsInvalidObjectName(newName))
		{
			ShowInvalidNameMsgWarning(pObject, newName, true);
			return false;
		}

		string name = newName;
		if (IsDuplicateObjectName(name))
		{
			if (bGenerateUnique)
			{
				name = GenUniqObjectName(name);
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Name %s already in use", newName);
				return false;
			}
		}

		pObject->SetName(name);

		NotifyObjectListeners(pObject, OBJECT_ON_RENAME);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
int CObjectManager::GetObjectCount() const
{
	return m_objects.size();
}

void CObjectManager::GetObjects(CBaseObjectsArray& objects, const AABB& box) const
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	AABB objectBox;
	objects.clear();
	objects.reserve(m_objects.size());
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		it->second->GetBoundBox(objectBox);
		if (box.IsIntersectBox(objectBox))
			objects.push_back(it->second);
	}
}

std::vector<CObjectLayer*> CObjectManager::GetUniqueLayersRelatedToObjects(const std::vector<CBaseObject*>& objects) const
{
	std::vector<CObjectLayer*> layers;
	for (CBaseObject* pObj : objects)
	{
		if (std::find(layers.cbegin(), layers.cend(), pObj->GetLayer()) == layers.cend())
		{
			CObjectLayer* pLayer = static_cast<CObjectLayer*>(pObj->GetLayer());
			layers.push_back(pLayer);
		}
	}
	return layers;
}

void CObjectManager::GetObjectsRepeatFast(CBaseObjectsArray& objects, const AABB& box)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// cache objects for area bigger than necessary and update it only when inout box touches cached area bounds
	if (m_cachedAreaBox.IsReset() || !m_cachedAreaBox.ContainsBox(box))
	{
		m_cachedAreaBox = box;
		m_cachedAreaBox.Expand(box.GetSize() + Vec3(8.f));
		GetIEditorImpl()->GetObjectManager()->GetObjects(m_cachedAreaObjects, m_cachedAreaBox);
	}

	AABB objectBox;
	objects.clear();
	objects.reserve(m_objects.size());
	for (CBaseObject* obj : m_cachedAreaObjects)
	{
		obj->GetBoundBox(objectBox);
		if (box.IsIntersectBox(objectBox))
			objects.push_back(obj);
	}
}

bool CObjectManager::IsAncestortInSet(CBaseObject* pObject, const std::set<const CBaseObject*>& objectsSet) const
{
	CBaseObject* pParent = pObject->GetParent();
	if (!pParent)
		pParent = pObject->GetLinkedTo();

	if (!pParent)
		return false;

	while (pParent)
	{
		if (objectsSet.find(pParent) != objectsSet.end())
			return true;

		if (pParent->GetParent())
			pParent = pParent->GetParent();
		else
			pParent = pParent->GetLinkedTo();
	}

	return false;
}

void CObjectManager::FilterParents(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const
{
	std::set<const CBaseObject*> objectsSet;
	for (const CBaseObject* object : objects)
		objectsSet.insert(object);

	out.reserve(objects.size());
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (!IsAncestortInSet(pObject, objectsSet))
			out.push_back(pObject);
	}
}

void CObjectManager::FilterParents(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const
{
	std::set<const CBaseObject*> objectsSet;
	for (const CBaseObject* object : objects)
		objectsSet.insert(object);

	out.reserve(objects.size());
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		if (!IsAncestortInSet(pObject, objectsSet))
			out.push_back(pObject);
	}
}

bool CObjectManager::IsLinkedAncestorInSet(CBaseObject* pObject, const std::set<const CBaseObject*>& objectsSet) const
{
	CBaseObject* pLinkedObject = pObject->GetLinkedTo();
	if (!pLinkedObject)
		return false;

	while (pLinkedObject)
	{
		if (objectsSet.find(pLinkedObject) != objectsSet.end())
			return true;

		pLinkedObject = pLinkedObject->GetLinkedTo();
	}

	return false;
}

void CObjectManager::RemoveObjectsAndNotify(const std::vector<CBaseObject*>& objects)
{
	std::vector<CBaseObjectPtr> tempObjects;
	tempObjects.reserve(objects.size());
	for (auto pObject : objects)
	{
		tempObjects.emplace_back(pObject);
	}
	RemoveObjects(objects);

	CObjectLayer::ForEachLayerOf(objects, [this](CObjectLayer* pLayer, const std::vector<CBaseObject*>& layerObjects)
	{
		pLayer->SetModified();
		signalObjectsDeleted(*pLayer, layerObjects);
	});

	for (auto pObject : objects)
	{
		pObject->m_layer = nullptr;
		NotifyObjectListeners(pObject, OBJECT_ON_DELETE);
	}
}

void CObjectManager::UnlinkObjects(const std::vector<CBaseObject*>& objects)
{
	for (auto pObject : objects)
	{
		// Unlink all children
		pObject->UnLinkAll();

		if (pObject->GetLinkedTo())
		{
			pObject->UnLink();
		}
	}
}

void CObjectManager::FilterLinkedObjects(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const
{
	std::set<const CBaseObject*> objectsSet;
	for (const CBaseObject* object : objects)
		objectsSet.insert(object);

	out.reserve(objects.size());
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];

		if (!IsLinkedAncestorInSet(pObject, objectsSet))
			out.push_back(pObject);
	}
}

void CObjectManager::FilterLinkedObjects(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const
{
	std::set<const CBaseObject*> objectsSet;
	for (const CBaseObject* object : objects)
		objectsSet.insert(object);

	out.reserve(objects.size());
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];

		if (!IsLinkedAncestorInSet(pObject, objectsSet))
			out.push_back(pObject);
	}
}

void CObjectManager::LinkSelectionTo(CBaseObject* pLinkTo, const char* szTargetName)
{
	if (!pLinkTo)
		return;

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection || !pSelection->GetCount())
		return;

	std::vector<CBaseObject*> objects;
	objects.reserve(pSelection->GetCount());
	for (int i = 0; i < pSelection->GetCount(); i++)
		objects.push_back(pSelection->GetObject(i));

	CUndo undo("Link Object(s)");
	Link(objects, pLinkTo, "");
}

void CObjectManager::Link(CBaseObject* pObject, CBaseObject* pLinkTo, const char* szTargetName)
{
	if (!pLinkTo || !pObject)
		return;

	const CSelectionGroup* pSelectionGroup = GetIEditorImpl()->GetSelection();
	if (pSelectionGroup && pSelectionGroup->GetCount() > 0 && pSelectionGroup->IsContainObject(pObject))
		LinkSelectionTo(pLinkTo);
	else
		Link(std::vector<CBaseObject*>({ pObject }), pLinkTo, szTargetName);
}

void CObjectManager::Link(const std::vector<CBaseObject*> objects, CBaseObject* pLinkTo, const char* szTargetName)
{
	if (!pLinkTo)
		return;

	ISkeletonPose* pSkeletonPose = NULL;
#if defined(USE_GEOM_CACHES)
	IGeomCacheRenderNode* pGeomCacheRenderNode = NULL;
#endif

	if (pLinkTo->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pLinkTo);
		IEntity* pIEntity = pEntity->GetIEntity();
		if (pIEntity)
		{
			ICharacterInstance* pCharacter = pIEntity->GetCharacter(0);
			if (pCharacter)
			{
				pSkeletonPose = pCharacter->GetISkeletonPose();
			}
#if defined(USE_GEOM_CACHES)
			else if (szTargetName && pLinkTo->IsKindOf(RUNTIME_CLASS(CGeomCacheEntity)))
			{
				pGeomCacheRenderNode = pIEntity->GetGeomCacheRenderNode(0);
			}
#endif
		}
	}

	if (pSkeletonPose)
	{
		LinkToBone(objects, static_cast<CEntityObject*>(pLinkTo));
	}
#if defined(USE_GEOM_CACHES)
	else if (pGeomCacheRenderNode && szTargetName)
	{
		LinkToGeomCacheNode(objects, static_cast<CGeomCacheEntity*>(pLinkTo), szTargetName);
	}
#endif
	else
	{
		LinkToObject(objects, pLinkTo);
	}
}

void CObjectManager::LinkToObject(CBaseObject* pObject, CBaseObject* pLinkTo)
{
	if (!pObject->CanLinkTo(pLinkTo))
		return;

	CUndo undo("Link Object");

	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		static_cast<CEntityObject*>(pObject)->SetAttachTarget("");
		static_cast<CEntityObject*>(pObject)->SetAttachType(CEntityObject::eAT_Pivot);
	}

	pObject->LinkTo(pLinkTo);
}

void CObjectManager::LinkToObject(const std::vector<CBaseObject*> objects, CBaseObject* pLinkTo)
{
	for (CBaseObject* pObject : objects)
		LinkToObject(pObject, pLinkTo);
}

void CObjectManager::LinkToBone(const std::vector<CBaseObject*> objects, CEntityObject* pLinkTo)
{
	bool bIsValidLink = false;
	for (CBaseObject* pObject : objects)
	{
		if (pObject->CanLinkTo(pLinkTo))
			bIsValidLink = true;
	}

	if (!bIsValidLink)
		return;

	IEntity* pIEntity = pLinkTo->GetIEntity();
	ICharacterInstance* pCharacter = pIEntity->GetCharacter(0);
	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
	const uint32 numJoints = rIDefaultSkeleton.GetJointCount();

	// Show a sorted pop-up menu for selecting a bone.
	QMenu* menu = new QMenu();
	QObject::connect(menu->addAction(QtUtil::ToQString(pLinkTo->GetName())), &QAction::triggered, [this, objects, pLinkTo]()
	{
		LinkToObject(objects, pLinkTo);
	});

	std::map<string, uint32> joints;
	for (uint32 i = 0; i < numJoints; ++i)
	{
		joints[rIDefaultSkeleton.GetJointNameByID(i)] = i;
	}

	for (auto it = joints.begin(); it != joints.end(); ++it)
	{
		auto idx = (*it).second;
		QObject::connect(menu->addAction(rIDefaultSkeleton.GetJointNameByID(idx)), &QAction::triggered, [this, objects, pLinkTo, &rIDefaultSkeleton, idx]()
		{
			for (CBaseObject* pObject : objects)
			{
			  // If the current object is not an entity, then fall back to linking to object
			  if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			  {
			    LinkToObject(pObject, pLinkTo);
			    continue;
			  }

			  if (!pObject->CanLinkTo(pLinkTo))
					continue;

			  CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);

			  CUndo undo("Link to Bone");
			  pEntityObject->SetAttachTarget(rIDefaultSkeleton.GetJointNameByID(idx));
			  pEntityObject->SetAttachType(CEntityObject::eAT_CharacterBone);
			  pEntityObject->LinkTo(pLinkTo, true);
			}
		});
	}

	menu->exec(QCursor::pos());
}

#if defined(USE_GEOM_CACHES)
void CObjectManager::LinkToGeomCacheNode(const std::vector<CBaseObject*> objects, CGeomCacheEntity* pLinkTo, const char* target)
{
	for (CBaseObject* pObject : objects)
	{
		// If the current object is not an entity, then fall back to linking to object
		if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			LinkToObject(pObject, pLinkTo);
			continue;
		}

		if (!pObject->CanLinkTo(pLinkTo))
			continue;

		CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);
		CUndo undo("Link to Geometry Cache Node");

		pEntityObject->SetAttachTarget(target);
		pEntityObject->SetAttachType(CEntityObject::eAT_GeomCacheNode);
		pEntityObject->LinkTo(pLinkTo, true);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GetObjects(CBaseObjectsArray& objects, const CObjectLayer* layer) const
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	objects.clear();
	objects.reserve(m_objects.size());
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		if (layer == 0 || it->second->GetLayer() == layer)
			objects.push_back(it->second);
	}
}

void CObjectManager::GetObjects(DynArray<CBaseObject*>& objects, const CObjectLayer* layer) const
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	CBaseObjectsArray objectArray;
	GetObjects(objectArray, layer);
	objects.clear();
	for (int i = 0, iCount(objectArray.size()); i < iCount; ++i)
		objects.push_back(objectArray[i]);
}

void CObjectManager::GetObjects(CBaseObjectsArray& objects, BaseObjectFilterFunctor const& filter) const
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	objects.clear();
	objects.reserve(m_objects.size());
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CRY_ASSERT(it->second);
		if (filter.first(*it->second, filter.second))
			objects.push_back(it->second);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GetCameras(std::vector<CEntityObject*>& objects)
{
	objects.clear();
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* object = it->second;
		if (object->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntityObject = static_cast<CEntityObject*>(object);

			// Legacy cameras
			if (pEntityObject->IsKindOf(RUNTIME_CLASS(CCameraObject)))
			{
				// Only consider camera sources.
				if (pEntityObject->IsLookAtTarget())
					continue;
				objects.push_back(pEntityObject);
			}
			else if (IEntity* pEntity = pEntityObject->GetIEntity())
			{
				if (Cry::DefaultComponents::CCameraComponent* pCameraComponent = pEntity->GetComponent<Cry::DefaultComponents::CCameraComponent>())
				{
					objects.push_back(pEntityObject);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SendEvent(ObjectEvent event)
{
	if (event == EVENT_RELOAD_ENTITY)
	{
		GetIEditorImpl()->GetFlowGraphManager()->SetGUIControlsProcessEvents(false, false);
	}

	if (event == EVENT_INGAME || event == EVENT_OUTOFGAME || event == EVENT_RELOAD_ENTITY)
		GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(true);

	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		if (obj->GetGroup() && event != EVENT_PRE_EXPORT)
			continue;
		obj->OnEvent(event);
	}

	if (event == EVENT_INGAME || event == EVENT_OUTOFGAME || event == EVENT_RELOAD_ENTITY)
		GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(false);

	if (event == EVENT_RELOAD_ENTITY)
	{
		GetIEditorImpl()->GetFlowGraphManager()->SetGUIControlsProcessEvents(true, false);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SendEvent(ObjectEvent event, const AABB& bounds)
{
	AABB box;
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		if (obj->GetGroup())
			continue;
		obj->GetBoundBox(box);
		if (bounds.IsIntersectBox(box))
			obj->OnEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_pPhysicsManager->Update();

	UpdateAttachedEntities();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::HideObject(CBaseObject* obj, bool hide)
{
	CRY_ASSERT(obj != 0);
	// Remove object from main object set and put it to hidden set.
	obj->SetHidden(hide);
	InvalidateVisibleList();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ToggleHideAllBut(CBaseObject* pObject)
{
	CBaseObjectsArray objects;
	GetObjects(objects, (CObjectLayer*)pObject->GetLayer());

	bool hideAll = false;
	for (CBaseObject* pObject : objects)
	{
		if (pObject->IsVisible())
		{
			hideAll = true;
			break;
		}
	}

	if (hideAll)
	{
		m_pLayerManager->SetAllVisible(false);
		pObject->SetVisible(true);
	}
	else
	{
		m_pLayerManager->SetAllVisible(true);
		for (CBaseObject* pObject : objects)
		{
			pObject->SetVisible(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UnhideAll()
{
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		IObjectLayer* pLayer = obj->GetLayer();

		// Only unhide objects that are not in hidden layers
		bool modifyHidden = true;
		do
		{
			if (!pLayer->IsVisible())
			{
				modifyHidden = false;
				break;
			}
		}
		while (pLayer = pLayer->GetParentIObjectLayer());

		if (modifyHidden)
			obj->SetHidden(false);
	}
	InvalidateVisibleList();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::FreezeObject(CBaseObject* obj, bool freeze)
{
	CRY_ASSERT(obj != 0);
	// Remove object from main object set and put it to hidden set.
	obj->SetFrozen(freeze);
	InvalidateVisibleList();
}

void CObjectManager::ToggleFreezeAllBut(CBaseObject* pObject)
{
	CBaseObjectsArray objects;
	GetObjects(objects, (CObjectLayer*)pObject->GetLayer());

	bool freezeAll = false;
	for (CBaseObject* pObject : objects)
	{
		if (!pObject->IsFrozen())
		{
			freezeAll = true;
			break;
		}
	}

	if (freezeAll)
	{
		m_pLayerManager->SetAllFrozen(true);
		pObject->SetFrozen(false);
	}
	else
	{
		m_pLayerManager->SetAllFrozen(false);
		for (CBaseObject* pObject : objects)
		{
			pObject->SetFrozen(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UnfreezeAll()
{
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		obj->SetFrozen(false);
	}
	InvalidateVisibleList();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SelectObject(CBaseObject* pObj)
{
	SelectObjects({ pObj });
}

void CObjectManager::NotifySelectionChanges(const std::vector<CBaseObject*>& selected, const std::vector<CBaseObject*>& deselected)
{
	GetIEditorImpl()->Notify(eNotify_OnSelectionChange);

	signalSelectionChanged(selected, deselected);

	EmitPopulateInspectorEvent();
}

void CObjectManager::EmitPopulateInspectorEvent() const
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	CBroadcastManager* const pBroadcastManager = CBroadcastManager::Get(CEditorMainFrame::GetInstance());
	if (pBroadcastManager)
	{
		const size_t objectCount = m_currSelection.GetCount();

		QString szTitle;
		if (objectCount == 1)
		{
			szTitle = m_currSelection.GetObject(0)->GetName().GetString();
		}
		else if (objectCount > 1)
		{
			char numString[30];
			snprintf(numString, arraysize(numString), "%d", objectCount);
			szTitle = numString;
			szTitle += " Selected Objects";
		}

		PopulateInspectorEvent popEvent([](CInspector& inspector)
		{
			const CSelectionGroup* pSelectionGroup = GetIEditorImpl()->GetObjectManager()->GetSelection();
			CInspectorWidgetCreator creator;

			for (size_t i = 0, n = pSelectionGroup->GetCount(); i < n; ++i)
			{
			  CBaseObject* pObject = pSelectionGroup->GetObject(i);

			  pObject->CreateInspectorWidgets(creator);

			  creator.EndScope();
			}

			// Add the queued widgets to the inspector
			creator.AddWidgetsToInspector(inspector);
		}, szTitle.toUtf8());

		pBroadcastManager->Broadcast(popEvent);
	}
}

void CObjectManager::UnselectObject(CBaseObject* pObject)
{
	CRY_ASSERT(pObject != 0);
	UnselectObjects({ pObject });
}

bool CObjectManager::MarkSelectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& selected)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	selected.reserve(objects.size());
	m_currSelection.Reserve(objects.size());

	GetIEditorImpl()->GetIUndoManager()->Suspend();

	for (CBaseObject* pObject : objects)
	{
		if (SetObjectSelected(pObject, true))
			selected.push_back(pObject);
	}

	GetIEditorImpl()->GetIUndoManager()->Resume();

	if (!selected.empty() && CUndo::IsRecording())
	{
		CUndo::Record(new CUndoBaseObjectSelect(true, selected));
	}

	return selected.empty() == false;
}

void CObjectManager::SelectObjects(const std::vector<CBaseObject*>& objects)
{
	std::vector<CBaseObject*> selected;
	if (MarkSelectObjects(objects, selected))
	{
		NotifySelectionChanges(selected, {});
	}
}

bool CObjectManager::MarkUnselectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& deselected)
{
	deselected.reserve(objects.size());

	GetIEditorImpl()->GetIUndoManager()->Suspend();

	for (auto pObject : objects)
	{
		if (SetObjectSelected(pObject, false))
			deselected.push_back(pObject);
	}

	GetIEditorImpl()->GetIUndoManager()->Resume();

	if (!deselected.empty() && CUndo::IsRecording())
	{
		CUndo::Record(new CUndoBaseObjectSelect(false, deselected));
	}

	return deselected.empty() == false;
}

void CObjectManager::UnselectObjects(const std::vector<CBaseObject*>& objects)
{
	std::vector<CBaseObject*> deselected;
	if (MarkUnselectObjects(objects, deselected))
	{
		NotifySelectionChanges({}, deselected);
	}
}

void CObjectManager::MarkToggleSelectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& selected, std::vector<CBaseObject*>& deselected)
{
	std::vector<CBaseObject*> select;
	std::vector<CBaseObject*> deselect;

	for (CBaseObject* pObject : objects)
	{
		if (pObject == nullptr)
			continue;

		if (pObject->IsSelected())
		{
			deselect.push_back(pObject);
		}
		else
		{
			select.push_back(pObject);
		}
	}

	//Note: this should maybe expose if we should use the selection mask or not
	MarkUnselectObjects(deselect, deselected);
	MarkSelectObjects(select, selected);
}

void CObjectManager::ToggleSelectObjects(const std::vector<CBaseObject*>& objects)
{
	if (objects.empty())
		return;

	std::vector<CBaseObject*> selected;
	std::vector<CBaseObject*> deselected;

	MarkToggleSelectObjects(objects, selected, deselected);
	NotifySelectionChanges(selected, deselected);
}

void CObjectManager::SelectAndUnselectObjects(const std::vector<CBaseObject*>& selectObjects, const std::vector<CBaseObject*>& unselectObjects)
{
	std::vector<CBaseObject*> selected;
	std::vector<CBaseObject*> deselected;

	if (!unselectObjects.empty())
	{
		MarkUnselectObjects(unselectObjects, deselected);
	}
	if (!selectObjects.empty())
	{
		MarkSelectObjects(selectObjects, selected);
	}
	if (!selected.empty() || !deselected.empty())
	{
		NotifySelectionChanges(selected, deselected);
	}
}

void CObjectManager::SelectAll()
{
	std::vector<CBaseObject*> objects;
	objects.reserve(m_objects.size());

	for (Objects::const_iterator ite = m_objects.begin(); ite != m_objects.end(); ++ite)
	{
		objects.push_back(ite->second);
	}

	SelectObjects(objects);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ClearSelection()
{
	// Make sure to unlock selection.
	GetIEditorImpl()->LockSelection(false);

	std::vector<CBaseObject*> objects;
	objects.reserve(m_currSelection.GetCount());
	for (int i = 0; i < m_currSelection.GetCount(); ++i)
	{
		objects.push_back(m_currSelection.GetObject(i));
	}

	UnselectObjects(objects);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::InvertSelection()
{
	if (m_objects.empty())
		return;

	std::vector<CBaseObject*> objects;
	objects.reserve(m_objects.size());

	for (Objects::const_iterator ite = m_objects.begin(); ite != m_objects.end(); ++ite)
	{
		objects.push_back(ite->second);
	}

	std::vector<CBaseObject*> selected;
	std::vector<CBaseObject*> deselected;

	MarkToggleSelectObjects(objects, selected, deselected);
	NotifySelectionChanges(selected, deselected);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Display(CObjectRenderHelper& objRenderHelper)
{
	Vec3 org;
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	const SRenderingPassInfo& passInfo = objRenderHelper.GetPassInfo();

	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (!m_bVisibleObjectValid || gViewportDebugPreferences.GetObjectHideMask() != m_lastHideMask)
	{
		m_lastHideMask = gViewportDebugPreferences.GetObjectHideMask();
		UpdateVisibilityList();
	}

	if (GetIEditor()->IsHelpersDisplayed() || dc.view->GetVisibleObjectsCache()->GetObjectCount() == 0 ||
	    dc.flags & DISPLAY_SELECTION_HELPERS) // is display helpers or list is not populated (needed to select objects)
	{
		FindDisplayableObjects(dc, &passInfo, true);
	}
}

void CObjectManager::ForceUpdateVisibleObjectCache(DisplayContext& dc)
{
	FindDisplayableObjects(dc, nullptr, false);
}

void CObjectManager::FindDisplayableObjects(DisplayContext& dc, const SRenderingPassInfo* passInfo, bool bDisplay)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	CRY_ASSERT_MESSAGE(!bDisplay || passInfo, "a valid passinfo is required when objects need to be displayed.");

	CBaseObjectsCache* pDispayedViewObjects = dc.view->GetVisibleObjectsCache();

	const CCamera& camera = GetIEditorImpl()->GetSystem()->GetViewCamera();
	AABB bbox;
	bbox.min.zero();
	bbox.max.zero();

	pDispayedViewObjects->ClearObjects();
	pDispayedViewObjects->Reserve(m_visibleObjects.size());

	CEditTool* pEditTool = GetIEditorImpl()->GetEditTool();

	if (dc.flags & DISPLAY_2D)
	{
		int numVis = m_visibleObjects.size();
		for (int i = 0; i < numVis; i++)
		{
			CBaseObject* obj = m_visibleObjects[i];

			obj->GetDisplayBoundBox(bbox);
			if (dc.box.IsIntersectBox(bbox))
			{
				pDispayedViewObjects->AddObject(obj);

				if (bDisplay && GetIEditor()->IsHelpersDisplayed() && (gViewportPreferences.showFrozenHelpers || !obj->IsFrozen()))
				{
					obj->Display(CObjectRenderHelper{ dc, *passInfo });
					if (pEditTool)
					{
						pEditTool->DrawObjectHelpers(obj, dc);
					}
				}
			}
		}
	}
	else
	{
		const CSelectionGroup* pSelection = GetSelection();
		if (pSelection && pSelection->GetCount() > 1)
		{
			AABB mergedAABB;
			mergedAABB.Reset();
			bool bAllSolids = true;
			for (int i = 0, iCount(pSelection->GetCount()); i < iCount; ++i)
			{
				CBaseObject* pObj(pSelection->GetObject(i));
				if (pObj == NULL)
					continue;
				AABB aabb;
				pObj->GetBoundBox(aabb);
				mergedAABB.Add(aabb);
				if (bAllSolids && pObj->GetType() != OBJTYPE_SOLID)
					bAllSolids = false;
			}

			if (!bAllSolids)
				pSelection->GetObject(0)->CBaseObject::DrawDimensions(dc, &mergedAABB);
			else
				pSelection->GetObject(0)->DrawDimensions(dc, &mergedAABB);
		}

		int numVis = m_visibleObjects.size();
		for (int i = 0; i < numVis; i++)
		{
			CBaseObject* obj = m_visibleObjects[i];

			if (obj && obj->IsInCameraView(camera))
			{
				// Check if object is too far.
				float visRatio = obj->GetCameraVisRatio(camera);
				if (visRatio > m_maxObjectViewDistRatio || (dc.flags & DISPLAY_SELECTION_HELPERS) || obj->IsSelected())
				{
					pDispayedViewObjects->AddObject(obj);

					AABB objAABB;
					obj->GetDisplayBoundBox(objAABB);

					float distanceSquared = objAABB.GetDistanceSqr(camera.GetPosition());

					if (distanceSquared > gViewportPreferences.objectHelperMaxDistSquaredDisplay)
					{
						continue;
					}
					if (bDisplay && (GetIEditor()->IsHelpersDisplayed() || dc.flags & DISPLAY_SELECTION_HELPERS) && (gViewportPreferences.showFrozenHelpers || !obj->IsFrozen()) && !obj->CheckFlags(OBJFLAG_HIDE_HELPERS))
					{
						obj->Display(CObjectRenderHelper{ dc, *passInfo });
						if (pEditTool)
						{
							pEditTool->DrawObjectHelpers(obj, dc);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CObjectManager::MoveObjects(const AABB& box, const Vec3& offset, int nRot, bool bIsCopy)
{
	AABB objBounds;

	Vec3 src = (box.min + box.max) / 2;
	Vec3 dst = src + offset;
	float alpha = 3.141592653589793f / 2 * nRot;
	float cosa = cos(alpha);
	float sina = sin(alpha);

	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;

		if (obj->GetParent())
			continue;

		if (obj->GetGroup())
			continue;

		obj->GetBoundBox(objBounds);
		if (box.IsIntersectBox(objBounds))
		{
			if (nRot == 0)
			{
				obj->SetPos(obj->GetPos() - src + dst);
			}
			else
			{
				Vec3 pos = obj->GetPos() - src;
				Vec3 newPos(pos);
				newPos.x = cosa * pos.x - sina * pos.y;
				newPos.y = sina * pos.x + cosa * pos.y;
				obj->SetPos(newPos + dst);
				Quat q;
				obj->SetRotation(q.CreateRotationZ(alpha) * obj->GetRotation());
			}
		}
	}
	return 0;
}

bool CObjectManager::IsObjectDeletionAllowed(CBaseObject* pObject)
{
	if (!pObject)
		return false;

	// Test AI object against AI/Physics activation
	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntityObj = (CEntityObject*)pObject;

			if (pEntityObj)
			{
				IEntity* pIEntity = pEntityObj->GetIEntity();
				if (pIEntity)
				{
					if (pIEntity->HasAI())
					{
						string msg("");
						msg.Format(_T("AI object %s cannot be deleted when AI/Physics mode is activated."), (const char*)pObject->GetName());
						CQuestionDialog::SWarning(QObject::tr(""), QObject::tr(msg));
						return false;
					}
				}
			}
		}
	}

	return true;
};

//////////////////////////////////////////////////////////////////////////
void CObjectManager::DeleteSelection()
{
	std::vector<CBaseObject*> selectedObjects;
	selectedObjects.reserve(m_currSelection.GetCount());

	for (int i = 0; i < m_currSelection.GetCount(); i++)
	{
		// Check condition(s) if object could be deleted
		if (!IsObjectDeletionAllowed(m_currSelection.GetObject(i)))
		{
			return;
		}

		selectedObjects.push_back(m_currSelection.GetObject(i));
	}

	ClearSelection();
	DeleteObjects(selectedObjects);

	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE);
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::HitTestObject(CBaseObject* obj, HitContext& hc)
{
	if (obj->IsFrozen())
		return false;

	if (obj->IsHidden())
		return false;

	// This object is rejected by deep selection.
	if (obj->CheckFlags(OBJFLAG_NO_HITTEST))
		return false;

	ObjectType objType = obj->GetType();

	// Check if this object type is masked for selection.
	if (!(objType & gViewportSelectionPreferences.objectSelectMask))
	{
		return obj->IsKindOf(RUNTIME_CLASS(CGroup)) && obj->HitTest(hc);
	}

	const bool bSelectionHelperHit = obj->HitHelperTest(hc);

	if (hc.bUseSelectionHelpers && !bSelectionHelperHit)
		return false;

	if (!bSelectionHelperHit)
	{
		// Fast checking.
		if (hc.camera && !obj->IsInCameraView(*hc.camera))
		{
			return false;
		}
		else if (hc.bounds && !obj->IntersectRectBounds(*hc.bounds))
		{
			return false;
		}

		// Do 2D space testing.
		if (hc.nSubObjFlags == 0)
		{
			Ray ray(hc.raySrc, hc.rayDir);
			if (!obj->IntersectRayBounds(ray))
				return false;
		}
		else if (!obj->HitTestRect(hc))
		{
			return false;
		}

		CEditTool* pEditTool = GetIEditorImpl()->GetEditTool();
		if (pEditTool && pEditTool->HitTest(obj, hc))
		{
			return true;
		}
	}

	return (bSelectionHelperHit || obj->HitTest(hc));
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::HitTest(HitContext& hitInfo)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	hitInfo.object = 0;
	hitInfo.dist = FLT_MAX;
	hitInfo.axis = 0;

	HitContext hc = hitInfo;
	if (hc.view)
	{
		hc.view->GetPerpendicularAxis(0, &hc.b2DViewport);
	}
	hc.rayDir = hc.rayDir.GetNormalized();

	float mindist = FLT_MAX;

	// Only HitTest objects, that where previously Displayed.
	CBaseObjectsCache* pDispayedViewObjects = hitInfo.view->GetVisibleObjectsCache();

	CBaseObject* selected = 0;
	const char* name = nullptr;
	int numVis = pDispayedViewObjects->GetObjectCount();
	for (int i = 0; i < numVis; i++)
	{
		CBaseObject* obj = pDispayedViewObjects->GetObject(i);

		if (obj == hitInfo.pExcludedObject)
			continue;

		// Skip objects in groups, since the group should be in the cache as well.
		if (obj->GetGroup() != nullptr)
			continue;

		if (HitTestObject(obj, hc))
		{
			if (hc.object == NULL)
				hc.object = obj;

			// Check if this object is nearest.
			if (hc.axis != 0)
			{
				hitInfo.object = obj;
				hitInfo.axis = hc.axis;
				hitInfo.dist = hc.dist;
				return true;
			}

			if (hc.dist < hitInfo.dist)
			{
				// If collided object specified, accept it, otherwise take tested object itself.
				CBaseObject* hitObj = hc.object;
				if (!hitObj)
					hitObj = obj;

				hc.object = 0;

				hitInfo.object = hitObj;
				hitInfo.dist = hc.dist;
				hitInfo.name = hc.name;
			}

			// If use deep selection
			if (hitInfo.pDeepSelection)
			{
				hitInfo.pDeepSelection->AddObject(hc.dist, obj);
			}
		}
	}

	return hitInfo.object != nullptr;
}

void CObjectManager::FindObjectsInRect(CViewport* view, const CRect& rect, std::vector<CryGUID>& guids)
{
	if (rect.Width() < 1 || rect.Height() < 1)
		return;

	HitContext hc;
	hc.view = view;
	hc.b2DViewport = view->GetType() != ET_ViewportCamera;
	hc.rect = rect;
	hc.bUseSelectionHelpers = view->GetAdvancedSelectModeFlag();

	guids.clear();

	CBaseObjectsCache* pDispayedViewObjects = view->GetVisibleObjectsCache();

	int numVis = pDispayedViewObjects->GetObjectCount();
	for (int i = 0; i < numVis; ++i)
	{
		CBaseObject* pObj = pDispayedViewObjects->GetObject(i);

		HitTestObjectAgainstRect(pObj, view, hc, guids);
	}
}
//////////////////////////////////////////////////////////////////////////
void CObjectManager::SelectObjectsInRect(CViewport* view, const CRect& rect, ESelectOp eSelect)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Ignore too small rectangles.
	if (rect.Width() < 1 || rect.Height() < 1)
		return;

	std::vector<CBaseObject*> selected;
	std::vector<CBaseObject*> deselected;

	CUndo undo("Select Object(s)");
	{
		std::vector<CBaseObject*> objectsInRect;
		FindSelectableObjectsInRect(view, rect, objectsInRect);
		if (eSelect == ESelectOp::eSelect)
		{
			MarkSelectObjects(objectsInRect, selected);
		}
		else if (eSelect == ESelectOp::eUnselect)
		{
			MarkUnselectObjects(objectsInRect, deselected);
		}
		else if (eSelect == ESelectOp::eToggle)
		{
			MarkToggleSelectObjects(objectsInRect, selected, deselected);
		}
	}

	NotifySelectionChanges(selected, deselected);
}

void CObjectManager::FindSelectableObjectsInRect(CViewport* view, const CRect& rect, std::vector<CBaseObject*>& selectableObjects)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	HitContext hc;
	hc.view = view;
	hc.b2DViewport = view->GetType() != ET_ViewportCamera;
	hc.rect = rect;
	hc.bUseSelectionHelpers = view->GetAdvancedSelectModeFlag();

	CBaseObjectsCache* pDispayedViewObjects = view->GetVisibleObjectsCache();
	int numVis = pDispayedViewObjects->GetObjectCount();
	selectableObjects.reserve(numVis);
	for (int i = 0; i < numVis; ++i)
	{
		using namespace Private_ObjectManager;
		FindSelectableObjectsInRectRecursive(this, view, pDispayedViewObjects->GetObject(i), hc, selectableObjects);
	}
}

bool CObjectManager::IsObjectSelectableInHitContext(CViewport* view, HitContext& hc, CBaseObject* pObj)
{
	if (!pObj->IsSelectable())
		return false;

	AABB box;
	// Retrieve world space bound box.
	pObj->GetBoundBox(box);

	// Check if object visible in viewport.
	if (!view->IsBoundsVisible(box))
		return false;

	if (pObj->HitTestRect(hc))
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
uint16 FindPossibleObjectNameNumber(std::set<uint16>& numberSet)
{
	const int LIMIT = 65535;
	size_t nSetSize = numberSet.size();
	for (uint16 i = 1; i < LIMIT; ++i)
	{
		uint16 candidateNumber = (i + nSetSize) % LIMIT;
		if (numberSet.find(candidateNumber) == numberSet.end())
		{
			numberSet.insert(candidateNumber);
			return candidateNumber;
		}
	}
	return 0;
}

void CObjectManager::RegisterObjectName(const string& name)
{
	// Remove all numbers from the end of typename.
	string typeName = name;
	int len = typeName.GetLength();
	uint16 num = 1;
	while (len > 0)
	{
		if (isdigit(typeName[len - 1]))
		{
			--len;
			continue;
		}
		else if (typeName[len - 1] == '-')
		{
			num = (uint16)atoi((const char*)name + len) + 0;
			typeName = typeName.Left(--len);
		}

		break;
	}

	NameNumbersMap::iterator iNameNumber = m_nameNumbersMap.find(typeName);
	if (iNameNumber == m_nameNumbersMap.end())
	{
		std::set<uint16> numberSet;
		numberSet.insert(num);
		m_nameNumbersMap[typeName] = numberSet;
	}
	else
	{
		std::set<uint16>& numberSet = iNameNumber->second;
		numberSet.insert(num);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UpdateRegisterObjectName(const string& name)
{
	// Remove all numbers from the end of typename.
	string typeName = name;
	int nameLen = typeName.GetLength();
	int len = nameLen;

	uint16 num = 1;
	while (len > 0)
	{
		if (isdigit(typeName[len - 1]))
		{
			--len;
			continue;
		}
		else if (typeName[len - 1] == '-')
		{
			num = (uint16)atoi((const char*)name + len) + 0;
			typeName = typeName.Left(--len);
		}

		break;
	}

	NameNumbersMap::iterator it = m_nameNumbersMap.find(typeName);

	if (it != m_nameNumbersMap.end())
	{
		if (it->second.end() != it->second.find(num))
		{
			it->second.erase(num);
			if (it->second.empty())
				m_nameNumbersMap.erase(it);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
string CObjectManager::GenUniqObjectName(const string& theTypeName)
{
	string typeName = theTypeName;
	if (const char* subName = ::strstr(theTypeName, "::"))
	{
		size_t length = ::strlen(subName);
		if (length > 2)
			typeName = subName + 2;
	}

	// Remove all numbers from the end of typename.
	int len = typeName.GetLength();

	while (len > 0)
	{
		if (isdigit(typeName[len - 1]))
		{
			--len;
			continue;
		}
		else if (typeName[len - 1] == '-')
			typeName = typeName.Left(--len);

		break;
	}

	NameNumbersMap::iterator ii = m_nameNumbersMap.find(typeName);
	uint16 lastNumber = 1;
	if (ii != m_nameNumbersMap.end())
	{
		lastNumber = FindPossibleObjectNameNumber(ii->second);
	}
	else
	{
		std::set<uint16> numberSet;
		numberSet.insert(lastNumber);
		m_nameNumbersMap[typeName] = numberSet;
	}

	string str;
	str.Format("%s-%d", (const char*)typeName, lastNumber);

	return str;
}

//////////////////////////////////////////////////////////////////////////
CObjectClassDesc* CObjectManager::FindClass(const string& className)
{
	IClassDesc* cls = GetIEditorImpl()->GetClassFactory()->FindClass(className);
	if (cls != NULL && cls->SystemClassID() == ESYSTEM_CLASS_OBJECT)
	{
		return (CObjectClassDesc*)cls;
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GetClassCategories(std::vector<string>& categories)
{
	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_OBJECT, classes);
	std::set<string> cset;
	for (int i = 0; i < classes.size(); i++)
	{
		const char* category = classes[i]->Category();
		if (strlen(category) > 0 && static_cast<CObjectClassDesc*>(classes[i])->IsCreatable())
			cset.insert(category);
	}
	categories.clear();
	categories.reserve(cset.size());
	for (std::set<string>::iterator cit = cset.begin(); cit != cset.end(); ++cit)
	{
		categories.push_back(*cit);
	}
}

void CObjectManager::GetClassCategoryToolClassNamePairs(std::vector<std::pair<string, string>>& categoryToolClassNamePairs)
{
	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_OBJECT, classes);
	std::set<std::pair<string, string>> cset;
	for (int i = 0; i < classes.size(); i++)
	{
		const char* category = classes[i]->Category();
		const char* toolClassName = ((CObjectClassDesc*)classes[i])->GetToolClassName();
		if (strlen(category) > 0)
			cset.insert(std::pair<string, string>(category, toolClassName));
	}
	categoryToolClassNamePairs.clear();
	categoryToolClassNamePairs.reserve(cset.size());
	for (std::set<std::pair<string, string>>::iterator cit = cset.begin(); cit != cset.end(); ++cit)
	{
		categoryToolClassNamePairs.push_back(*cit);
	}
}

void CObjectManager::GetCreatableClassTypes(const string& category, std::vector<CObjectClassDesc*>& types)
{
	std::vector<IClassDesc*> classes;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_OBJECT, classes);

	for (int i = 0; i < classes.size(); ++i)
	{
		CObjectClassDesc* cldesc = static_cast<CObjectClassDesc*>(classes[i]);

		if (!cldesc->IsCreatable())
		{
			continue;
		}

		const char* cat = cldesc->Category();

		if (stricmp(cat, category) == 0)
		{
			types.push_back(cldesc);
		}
	}
}

std::vector<string> CObjectManager::GetAllClasses() const
{
	std::vector<IClassDesc*> classDescs;
	GetIEditorImpl()->GetClassFactory()->GetClassesBySystemID(ESYSTEM_CLASS_OBJECT, classDescs);

	std::vector<string> classes;
	classes.reserve(classDescs.size());
	for (auto classDesc : classDescs)
	{
		classes.push_back(classDesc->ClassName());
	}

	return classes;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RegisterClassTemplate(XmlNodeRef& templ)
{
	string typeName = templ->getTag();
	string superTypeName;
	if (!templ->getAttr("SuperType", superTypeName))
		return;

	CObjectClassDesc* superType = FindClass(superTypeName);
	if (!superType)
		return;

	CXMLObjectClassDesc* classDesc = new CXMLObjectClassDesc;
	classDesc->superType = superType;
	classDesc->type = typeName;

	templ->getAttr("Category", classDesc->category);
	templ->getAttr("File", classDesc->fileSpec);
	templ->getAttr("DataFilter", classDesc->dataFilter);

	GetIEditorImpl()->GetClassFactory()->RegisterClass(classDesc);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::LoadClassTemplates(const string& path)
{
	string dir = PathUtil::AddBackslash(path.GetString());

	std::vector<CFileUtil::FileDesc> files;
	CFileUtil::ScanDirectory(dir, "*.xml", files, false);

	for (int k = 0; k < files.size(); k++)
	{
		// Construct the full filepath of the current file
		XmlNodeRef node = XmlHelpers::LoadXmlFromFile(dir + files[k].filename.GetString());
		if (node != 0 && node->isTag("ObjectTemplates"))
		{
			string name;
			for (int i = 0; i < node->getChildCount(); i++)
			{
				RegisterClassTemplate(node->getChild(i));
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Serialize(XmlNodeRef& xmlNode, bool bLoading, int flags)
{
	if (!xmlNode)
		return;

	if (bLoading)
	{
		m_loadedObjects = 0;

		if (flags == SERIALIZE_ONLY_NOTSHARED)
			DeleteNotSharedObjects();
		else if (flags == SERIALIZE_ONLY_SHARED)
			DeleteSharedObjects();
		else
			DeleteAllObjects();

		XmlNodeRef root = xmlNode->findChild("Objects");

		int totalObjects = 0;

		if (root)
			root->getAttr("NumObjects", totalObjects);

		StartObjectsLoading(totalObjects);

		// Load layers.
		CObjectArchive ar(this, xmlNode, true);

		// Load layers.
		m_pLayerManager->Serialize(ar);

		// Loading.
		if (root)
		{
			ar.node = root;
			LoadObjects(ar, false);
		}
		EndObjectsLoading();
	}
	else
	{
		// Saving.
		XmlNodeRef root = xmlNode->newChild("Objects");
		CObjectArchive ar(this, root, false);

		// Save layers.
		ar.node = xmlNode;
		m_pLayerManager->Serialize(ar);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::LoadObjects(CObjectArchive& objectArchive, bool bSelect)
{
	LOADING_TIME_PROFILE_SECTION;
	m_bLoadingObjects = true;

	// Prevent the prefab manager from updating prefab instances
	CPrefabManager::SkipPrefabUpdate skipUpdates;

	XmlNodeRef objectsNode = objectArchive.node;
	int numObjects = objectsNode->getChildCount();
	std::vector<CBaseObject*> objects;
	objects.reserve(numObjects);
	for (int i = 0; i < numObjects; i++)
	{
		objectArchive.node = objectsNode->getChild(i);
		CBaseObject* obj = objectArchive.LoadObject(objectsNode->getChild(i));
		if (obj && bSelect)
		{
			objects.push_back(obj);
		}
	}
	CBatchProcessDispatcher batchProcessDispatcher;
	batchProcessDispatcher.Start(objects, true);
	SelectObjects(objects);
	EndObjectsLoading(); // End progress bar, here, Resolve objects have his own.
	objectArchive.ResolveObjects(true);

	InvalidateVisibleList();

	m_bLoadingObjects = false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Export(const string& levelPath, XmlNodeRef& rootNode, bool onlyShared)
{
	// Clear export files.
	DeleteFile(levelPath + "TagPoints.ini");
	DeleteFile(levelPath + "Volumes.ini");

	// Entities need to be exported in the order of hierarchy (parents first)
	// Therefore we need to sort them based on the number of parents, ensuring that parents are loaded first - but not necessarily sequentially.
	struct SOrderedObject
	{
		CBaseObject* pObject;
		int          sortOrder;
	};

	std::vector<SOrderedObject> sortedObjects;
	// Change reservation if we extend this sorted export behavior to other types!
	sortedObjects.reserve(gEnv->pEntitySystem->GetNumEntities());

	// Save all objects to XML.
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* pObject = it->second;
		CObjectLayer* pLayer = static_cast<CObjectLayer*>(pObject->GetLayer());
		if (!pLayer->IsExportable())
			continue;
		// Export Only shared objects.
		if ((pObject->CheckFlags(OBJFLAG_SHARED) && onlyShared) ||
		    (!pObject->CheckFlags(OBJFLAG_SHARED) && !onlyShared))
		{
			if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
			{
				int sortOrder = 0;
				CBaseObject* pSortedObject = pObject;
				while (pSortedObject)
				{
					sortOrder++;

					if (pSortedObject->GetLinkedTo() != nullptr)
					{
						pSortedObject = pSortedObject->GetLinkedTo();
					}
					else
					{
						pSortedObject = pSortedObject->GetParent();
					}
				}

				auto upperBoundIt = std::upper_bound(sortedObjects.begin(), sortedObjects.end(), sortOrder, [](const int sortOrder, const SOrderedObject& other) -> bool { return sortOrder < other.sortOrder; });
				auto it = sortedObjects.insert(upperBoundIt, SOrderedObject { pObject, sortOrder });
			}
			else
			{
				pObject->Export(levelPath, rootNode);
			}
		}
	}

	for (const SOrderedObject& orderedObject : sortedObjects)
	{
		orderedObject.pObject->Export(levelPath, rootNode);
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::DeleteNotSharedObjects()
{
	TBaseObjects objects;
	GetAllObjects(objects);
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* obj = objects[i];
		if (!obj->CheckFlags(OBJFLAG_SHARED))
		{
			DeleteObject(obj);
		}
	}
}

void CObjectManager::DeleteSharedObjects()
{
	TBaseObjects objects;
	GetAllObjects(objects);
	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* obj = objects[i];
		if (obj->CheckFlags(OBJFLAG_SHARED))
		{
			DeleteObject(obj);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::InvalidateVisibleList()
{
	m_cachedAreaBox = AABB::RESET;
	m_cachedAreaObjects.clear();

	if (m_isUpdateVisibilityList)
		return;
	m_bVisibleObjectValid = false;
	m_visibleObjects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UpdateVisibilityList()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	m_isUpdateVisibilityList = true;
	m_visibleObjects.clear();
	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		bool visible = obj->IsPotentiallyVisible();
		obj->UpdateVisibility(visible);
		if (visible)
		{
			// Prefabs are not added into visible list.
			if (!obj->CheckFlags(OBJFLAG_PREFAB))
				m_visibleObjects.push_back(obj);
		}
	}
	m_bVisibleObjectValid = true;
	m_isUpdateVisibilityList = false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RegisterTypeConverter(CRuntimeClass* pSource, const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc)
{
	m_typeConverters.emplace_back(SConverter { pSource, szTargetName, conversionFunc });
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::IterateTypeConverters(CRuntimeClass* pSource, std::function<void(const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc)> callback)
{
	for (const SConverter& converter : m_typeConverters)
	{
		if (converter.pSource == pSource || pSource->IsDerivedFrom(converter.pSource))
		{
			callback(converter.targetName.c_str(), converter.conversionFunc);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::SetObjectSelected(CBaseObject* pObject, bool bSelect, bool shouldNotify /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	CRY_ASSERT(pObject);

	// Only select/unselect once. And only select objects types that are selectable (not masked)
	if (pObject->IsSelected() == bSelect || (bSelect && (pObject->GetType() & ~gViewportSelectionPreferences.objectSelectMask)))
		return false;

	// Store selection undo.
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoBaseObjectSelect(pObject));

	pObject->SetSelected(bSelect);

	if (bSelect)
	{
		m_currSelection.AddObject(pObject);
	}
	else
	{
		m_currSelection.RemoveObject(pObject);
	}

	NotifyObjectListeners(pObject, bSelect ? OBJECT_ON_SELECT : OBJECT_ON_UNSELECT);

	return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CObjectManager::AddObjectEventListener(const EventCallback& cb)
{
	// legacy style notification wrapper. For such registration we are only interested in the calling object
	// New notification code should make sure to initialize CObjectEvent properly with the calling object
	signalObjectChanged.Connect([=](CObjectEvent& data)
	{
		cb(data.m_pObj, data.m_type);
	}, reinterpret_cast<uintptr_t>(cb.getCallee()));   // Use ID of callee so we know whom to unregister
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RemoveObjectEventListener(const EventCallback& cb)
{
	signalObjectChanged.DisconnectById(reinterpret_cast<uintptr_t>(cb.getCallee()));
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::NotifyObjectListeners(CBaseObject* pObject, EObjectListenerEvent event)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	// Legacy style notification only uses a basic event with just the firing object
	CObjectEvent e(event, pObject);
	signalObjectChanged(e);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::StartObjectsLoading(int numObjects)
{
	if (m_pLoadProgress)
		return;
	m_pLoadProgress = new CWaitProgress(_T("Loading Objects"));
	m_totalObjectsToLoad = numObjects;
	m_loadedObjects = 0;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::EndObjectsLoading()
{
	if (m_pLoadProgress)
		delete m_pLoadProgress;
	m_pLoadProgress = 0;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GatherUsedResources(CUsedResources& resources, CObjectLayer* pLayer)
{
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects, pLayer);

	for (int i = 0; i < objects.size(); i++)
	{
		CBaseObject* pObject = objects[i];
		pObject->GatherUsedResources(resources);
	}
}

//////////////////////////////////////////////////////////////////////////
IStatObj* CObjectManager::GetGeometryFromObject(CBaseObject* pObject)
{
	CRY_ASSERT(pObject);

	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrushObj = (CBrushObject*)pObject;
		return pBrushObj->GetIStatObj();
	}
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObj = (CEntityObject*)pObject;
		if (pEntityObj->GetIEntity())
		{
			IEntity* pGameEntity = pEntityObj->GetIEntity();
			for (int i = 0; pGameEntity != NULL && i < pGameEntity->GetSlotCount(); i++)
			{
				if (pGameEntity->GetStatObj(i))
					return pGameEntity->GetStatObj(i);
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
ICharacterInstance* CObjectManager::GetCharacterFromObject(CBaseObject* pObject)
{
	CRY_ASSERT(pObject);
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntityObj = (CEntityObject*)pObject;
		if (pEntityObj->GetIEntity())
		{
			IEntity* pGameEntity = pEntityObj->GetIEntity();
			for (int i = 0; pGameEntity != NULL && i < pGameEntity->GetSlotCount(); i++)
			{
				if (pGameEntity->GetCharacter(i))
					return pGameEntity->GetCharacter(i);
			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
CBaseObject* CObjectManager::FindPhysicalObjectOwner(IPhysicalEntity* pPhysicalEntity)
{
	if (!pPhysicalEntity)
		return 0;

	int itype = pPhysicalEntity->GetiForeignData();
	switch (itype)
	{
	case PHYS_FOREIGN_ID_ROPE:
		{
			IRopeRenderNode* pRenderNode = (IRopeRenderNode*)pPhysicalEntity->GetForeignData(itype);
			if (pRenderNode)
			{
				EntityId id = pRenderNode->GetOwnerEntity() ? pRenderNode->GetOwnerEntity()->GetId() : INVALID_ENTITYID;
				CEntityObject* pEntity = CEntityObject::FindFromEntityId(id);
				return pEntity;
			}
		}
		break;
	case PHYS_FOREIGN_ID_ENTITY:
		{
			IEntity* pIEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhysicalEntity);
			if (pIEntity)
			{
				return CEntityObject::FindFromEntityId(pIEntity->GetId());
			}
		}
		break;
	case PHYS_FOREIGN_ID_STATIC:
		{
			IRopeRenderNode* pRenderNode = (IRopeRenderNode*)pPhysicalEntity->GetForeignData(itype);
			if (pRenderNode)
			{
				// Find brush who created this render node.

			}
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::OnObjectModified(CBaseObject* pObject, bool bDelete, bool boModifiedTransformOnly)
{
	if (IRenderNode* pRenderNode = pObject->GetEngineNode())
		GetIEditorImpl()->Get3DEngine()->OnObjectModified(pRenderNode, pRenderNode->GetRndFlags());
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UnregisterNoExported()
{
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* pObj = it->second;
		CObjectLayer* pLayer = (CObjectLayer*)pObj->GetLayer();
		if (pLayer && !pLayer->IsExportable())
		{
			pObj->UnRegisterFromEngine();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RegisterNoExported()
{
	for (Objects::const_iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* pObj = it->second;
		CObjectLayer* pLayer = (CObjectLayer*)pObj->GetLayer();
		if (pLayer && !pLayer->IsExportable())
		{
			pObj->RegisterOnEngine();
		}
	}
}

void CObjectManager::FindAndRenameProperty2(const char* property2Name, const string& oldValue, const string& newValue)
{
	CBaseObjectsArray objects;
	GetObjects(objects);

	for (size_t i = 0, n = objects.size(); i < n; ++i)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
			CVarBlock* pProperties2 = pEntity->GetProperties2();
			if (pProperties2)
			{
				IVariable* pVariable = pProperties2->FindVariable(property2Name);
				if (pVariable)
				{
					string sValue;
					pVariable->Get(sValue);
					if (sValue == oldValue)
					{
						pEntity->StoreUndo("Rename Property2");

						pVariable->Set(newValue);

						// Special case
#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
						if ((property2Name == "aiterritory_Territory") && ((newValue == "<None>") || (newValue != oldValue)))
#else
						if ((property2Name == "aiterritory_Territory") && ((newValue == "<Auto>") || (newValue == "<None>") || (newValue != oldValue)))
#endif
						{
							IVariable* pVariableWave = pProperties2->FindVariable("aiwave_Wave");
							if (pVariableWave)
							{
								pVariableWave->Set("<None>");
							}
						}
					}
				}
			}
		}
	}
}

void CObjectManager::FindAndRenameProperty2If(const char* property2Name, const string& oldValue, const string& newValue, const char* otherProperty2Name, const string& otherValue)
{
	CBaseObjectsArray objects;
	GetObjects(objects);

	for (size_t i = 0, n = objects.size(); i < n; ++i)
	{
		CBaseObject* pObject = objects[i];
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
			CVarBlock* pProperties2 = pEntity->GetProperties2();
			if (pProperties2)
			{
				IVariable* pVariable = pProperties2->FindVariable(property2Name);
				IVariable* pOtherVariable = pProperties2->FindVariable(otherProperty2Name);
				if (pVariable && pOtherVariable)
				{
					string sValue;
					pVariable->Get(sValue);

					string sOtherValue;
					pOtherVariable->Get(sOtherValue);

					if ((sValue == oldValue) && (sOtherValue == otherValue))
					{
						pEntity->StoreUndo("Rename Property2 If");

						pVariable->Set(newValue);

						// Special case
#ifdef USE_SIMPLIFIED_AI_TERRITORY_SHAPE
						if ((property2Name == "aiterritory_Territory") && (newValue == "<None>"))
#else
						if ((property2Name == "aiterritory_Territory") && ((newValue == "<Auto>") || (newValue == "<None>")))
#endif
						{
							IVariable* pVariableWave = pProperties2->FindVariable("aiwave_Wave");
							if (pVariableWave)
							{
								pVariableWave->Set("<None>");
							}
						}
					}
				}
			}
		}
	}
}

void CObjectManager::ResolveMissingObjects()
{
	enum
	{
		eInit      = 0,
		eNoForAll  = 1,
		eNo        = 2,
		eYesForAll = 3,
		eYes       = 4
	};

	typedef std::map<string, string> LocationMap;
	LocationMap locationMap;

	int locationState = eInit;
	bool isUpdated = false;

	Log("Resolving missed objects...");

	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;

		CGeomEntity* pGeomEntity = 0;
		CSimpleEntity* pSimpleEntity = 0;
		CBrushObject* pBrush = 0;
		IEntity* pEntity = 0;
		string geometryFile;
		IVariable* pModelVar = 0;

		if (obj->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
		{
			IEntity* pEntityObj = ((CGeomEntity*)obj)->GetIEntity();
			if (pEntityObj && pEntityObj->GetStatObj(0) && pEntityObj->GetStatObj(0)->IsDefaultObject())
			{
				pGeomEntity = (CGeomEntity*)obj;
				geometryFile = pGeomEntity->GetGeometryFile();
			}
		}
		else if (obj->IsKindOf(RUNTIME_CLASS(CSimpleEntity)))
		{
			IEntity* pEntityObj = ((CSimpleEntity*)obj)->GetIEntity();
			if (pEntityObj && pEntityObj->GetStatObj(0) && pEntityObj->GetStatObj(0)->IsDefaultObject())
			{
				pSimpleEntity = (CSimpleEntity*)obj;
				geometryFile = pSimpleEntity->GetGeometryFile();
			}
		}
		else if (obj->IsKindOf(RUNTIME_CLASS(CBrushObject)))
		{
			CBrushObject* pBrushObj = (CBrushObject*)obj;
			if (pBrushObj->GetGeometry() && ((CEdMesh*)pBrushObj->GetGeometry())->IsDefaultObject())
			{
				pBrush = (CBrushObject*)obj;
				geometryFile = pBrush->GetGeometryFile();
			}
		}
		else if (obj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			IEntity* pEntityObj = ((CEntityObject*)obj)->GetIEntity();
			if (pEntityObj && pEntityObj->GetStatObj(0) && pEntityObj->GetStatObj(0)->IsDefaultObject())
			{
				CVarBlock* pVars = ((CEntityObject*)obj)->GetProperties();
				if (pVars)
				{
					for (int i = 0; i < pVars->GetNumVariables(); i++)
					{
						pModelVar = pVars->GetVariable(i);
						if (pModelVar && pModelVar->GetDataType() == IVariable::DT_FILE)
						{
							pModelVar->Get(geometryFile);
							const char* ext = PathUtil::GetExt(geometryFile);
							if (stricmp(ext, CRY_GEOMETRY_FILE_EXT) == 0 || stricmp(ext, CRY_SKEL_FILE_EXT) == 0 || stricmp(ext, CRY_CHARACTER_DEFINITION_FILE_EXT) == 0 || stricmp(ext, CRY_ANIM_GEOMETRY_FILE_EXT) == 0)
							{
								if (!gEnv->pCryPak->IsFileExist(geometryFile))
								{
									pEntity = pEntityObj;
									break;
								}
							}
						}
					}
				}
			}
		}

		if (!pGeomEntity && !pSimpleEntity && !pBrush && !pEntity)
			continue;

		string newFilename = stl::find_in_map(locationMap, geometryFile, string(""));
		if (newFilename != "")
		{
			if (pGeomEntity)
			{
				pGeomEntity->SetGeometryFile(newFilename);
			}
			else if (pSimpleEntity)
			{
				pSimpleEntity->SetGeometryFile(newFilename);
			}
			else if (pBrush)
			{
				pBrush->CreateBrushFromMesh(newFilename);
				if (pBrush->GetGeometry() && !((CEdMesh*)pBrush->GetGeometry())->IsDefaultObject())
				{
					pBrush->SetGeometryFile(newFilename);
				}
			}
			else if (pEntity)
			{
				pModelVar->Set(newFilename);
			}
			Log("%s: %s <- %s", obj->GetName(), geometryFile, newFilename);
			continue;
		}

		if (locationState == eNoForAll)
			continue;

		QDialogButtonBox::StandardButtons result = QDialogButtonBox::Ok;
		if (locationState != eYesForAll) // Skip, if "Yes for All" pressed before
		{
			string mes;
			mes.Format(_T("Geometry file for object \"%s\" is missing/removed. \r\nFile: %s\r\nAttempt to locate this file?"), (const char*)obj->GetName(), (const char*)geometryFile);
			result = CQuestionDialog::SQuestion(QObject::tr("Object missing"), mes.GetString(), QDialogButtonBox::NoToAll | QDialogButtonBox::No | QDialogButtonBox::YesToAll | QDialogButtonBox::Yes);

			if (result == QDialogButtonBox::NoToAll && locationMap.size() == 0)
				break;

			if (result == QDialogButtonBox::NoToAll)
				locationState = eNoForAll;

			if (result == QDialogButtonBox::YesToAll)
				locationState = eYesForAll;
		}

		if (result == QDialogButtonBox::Yes || locationState == eYesForAll)
		{
			CFileUtil::FileArray cFiles;
			string filemask = PathUtil::GetFile(geometryFile);
			CFileUtil::ScanDirectory(PathUtil::GetGameFolder().c_str(), filemask, cFiles, true);

			if (cFiles.size())
			{
				string newFilename = cFiles[0].filename;
				if (pGeomEntity)
				{
					pGeomEntity->SetGeometryFile(newFilename);
				}
				else if (pSimpleEntity)
				{
					pSimpleEntity->SetGeometryFile(newFilename);
				}
				else if (pBrush)
				{
					pBrush->CreateBrushFromMesh(newFilename);
					if (pBrush->GetGeometry() && !((CEdMesh*)pBrush->GetGeometry())->IsDefaultObject())
					{
						pBrush->SetGeometryFile(newFilename);
					}
				}
				else if (pEntity)
				{
					pModelVar->Set(newFilename);
				}
				locationMap[geometryFile] = newFilename;
				Log("%s: %s <- %s", obj->GetName(), geometryFile, newFilename);
				isUpdated = true;
			}
			else
				GetIEditorImpl()->GetSystem()->GetILog()->LogWarning("Can't resolve object: %s: %s", obj->GetName(), geometryFile);
		}
	}
	if (isUpdated)
		GetIEditorImpl()->SetModifiedFlag();
	else
		Log("No objects has been resolved.");

	ResolveMissingMaterials();
}

void CObjectManager::ResolveMissingMaterials()
{
	enum
	{
		eInit      = 0,
		eNoForAll  = 1,
		eNo        = 2,
		eYesForAll = 3,
		eYes       = 4
	};

	typedef std::map<string, string> LocationMap;
	LocationMap locationMap;

	int locationState = eInit;
	bool isUpdated = false;

	string oldFilename;

	Log("Resolving missed materials...");

	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;

		CMaterial* pMat = (CMaterial*)obj->GetMaterial();

		if (pMat && pMat->GetMatInfo() && pMat->GetMatInfo()->IsDefault())
		{
			oldFilename = pMat->GetFilename();
		}
		else
			continue;

		string newFilename = stl::find_in_map(locationMap, oldFilename, string(""));
		if (newFilename != "")
		{
			CMaterial* pNewMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(newFilename);
			if (pNewMaterial)
			{
				obj->SetMaterial(pNewMaterial);
				Log("%s: %s <- %s", pMat->GetName(), oldFilename, newFilename);
			}
			continue;
		}

		if (locationState == eNoForAll)
			continue;

		QDialogButtonBox::StandardButtons result = QDialogButtonBox::Ok;
		if (locationState != eYesForAll) // Skip, if "Yes for All" pressed before
		{
			string mes;
			mes.Format(_T("Material for object \"%s\" is missing/removed. \r\nFile: %s\r\nAttempt to locate this file?"), (const char*)obj->GetName(), (const char*)oldFilename);
			result = CQuestionDialog::SQuestion(QObject::tr("Material missing"), mes.GetString(), QDialogButtonBox::NoToAll | QDialogButtonBox::No | QDialogButtonBox::YesToAll | QDialogButtonBox::Yes);

			if (result == QDialogButtonBox::NoToAll && locationMap.size() == 0)
				break;

			if (result == QDialogButtonBox::NoToAll)
				locationState = eNoForAll;

			if (result == QDialogButtonBox::YesToAll)
				locationState = eYesForAll;
		}

		if (result == QDialogButtonBox::Yes || locationState == eYesForAll)
		{
			CFileUtil::FileArray cFiles;
			string filemask = PathUtil::GetFile(oldFilename);
			CFileUtil::ScanDirectory(PathUtil::GetGameFolder().c_str(), filemask, cFiles, true);

			if (cFiles.size())
			{
				string newFilename = cFiles[0].filename;

				CMaterial* pNewMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(newFilename);
				if (pNewMaterial)
				{
					obj->SetMaterial(pNewMaterial);
					locationMap[oldFilename] = newFilename;
					Log("%s: %s <- %s", pMat->GetName(), oldFilename, newFilename);
					isUpdated = true;
				}
			}
			else
				GetIEditorImpl()->GetSystem()->GetILog()->LogWarning("Can't resolve material: %s: %s", pMat->GetName(), oldFilename);
		}

	}
	if (isUpdated)
		GetIEditorImpl()->SetModifiedFlag();
	else
		Log("No materials has been resolved.");
}

void CObjectManager::SetSelectionMask(int mask) const
{
	gViewportSelectionPreferences.objectSelectMask = mask;
	signalSelectionMaskChanged(mask);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::UpdateAttachedEntities()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	if (!GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
		return;

	for (Objects::iterator it = m_objects.begin(); it != m_objects.end(); ++it)
	{
		CBaseObject* obj = it->second;
		if (obj->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = static_cast<CEntityObject*>(obj);
			pEntity->UpdateTransform();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::AssignLayerIDsToRenderNodes()
{
	m_pLayerManager->AssignLayerIDsToRenderNodes();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SaveEntitiesInternalState(struct IDataWriteStream& writer) const
{
	// save layer state
	{
		const auto& layers = m_pLayerManager->GetLayers();

		uint32 numVisibleLayers = 0;
		for (uint32 i = 0; i < layers.size(); ++i)
		{
			CObjectLayer* pLayer = layers[i];
			if (pLayer->IsVisible())
			{
				numVisibleLayers += 1;
			}
		}

		writer.WriteUint32(numVisibleLayers);
		for (uint32 i = 0; i < layers.size(); ++i)
		{
			CObjectLayer* pLayer = layers[i];
			if (pLayer->IsVisible())
			{
				writer.WriteString(pLayer->GetName());
			}
		}
	}

	// get current object list
	GetLayersManager()->AssignLayerIDsToRenderNodes();

	// export entities
	CBaseObjectsArray allObjects;
	GetObjects(allObjects);
	std::vector<XmlNodeRef> exportedXml;
	std::vector<const CEntityObject*> exportedEntities;
	for (uint32 i = 0; i < allObjects.size(); ++i)
	{
		const CBaseObject* pObject = allObjects[i];
		if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			const CEntityObject& entity = static_cast<const CEntityObject&>(*pObject);

			XmlNodeRef xmlNode = gEnv->pSystem->CreateXmlNode();
			if (NULL != xmlNode)
			{
				// export the entity to XML
				const_cast<CEntityObject&>(entity).Export("", xmlNode);
				if (xmlNode->getChildCount() > 0)
				{
					XmlNodeRef xmlEntity = xmlNode->getChild(0);

					// remove the EntityID from the entry
					xmlEntity->delAttr("EntityID");

					// make sure the GUID attribute is set
					CryGUID m_guid;
					if (!xmlEntity->getAttr("EntityGUID", m_guid))
					{
						gEnv->pLog->LogError("Entity '%s' has no GUID and cannot be sent via LiveCreate.", entity.GetName());
						continue;
					}

					// add to the save list
					exportedEntities.push_back(&entity);
					exportedXml.push_back(xmlEntity);
				}
			}
		}
	}

	// write entities
	writer.WriteUint32(exportedXml.size());
	for (uint32 i = 0; i < exportedXml.size(); ++i)
	{
		const CEntityObject* pEntity = exportedEntities[i];
		string guidStr = pEntity->GetIEntity()->GetGuid().ToString();
		writer.WriteString(guidStr);
		writer.WriteString(exportedXml[i]->getXML().c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::HitTestObjectAgainstRect(CBaseObject* pObj, CViewport* view, HitContext hc, std::vector<CryGUID>& guids)
{
	if (!pObj->IsSelectable())
		return;

	AABB box;

	// Retrieve world space bound box.
	pObj->GetBoundBox(box);

	// Check if object visible in viewport.
	if (!view->IsBoundsVisible(box))
		return;

	if (pObj->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		CGroup* pGroup = static_cast<CGroup*>(pObj);
		// If the group is open check children
		if (pGroup->IsOpen())
		{
			for (int i = 0; i < pGroup->GetChildCount(); ++i)
			{
				HitTestObjectAgainstRect(pGroup->GetChild(i), view, hc, guids);
			}
		}
	}

	if (pObj->HitTestRect(hc))
		stl::push_back_unique(guids, pObj->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SelectObjectInRect(CBaseObject* pObj, CViewport* view, HitContext hc, ESelectOp eSelect)
{
	if (!pObj->IsSelectable())
		return;

	AABB box;

	// Retrieve world space bound box.
	pObj->GetBoundBox(box);

	// Check if object visible in viewport.
	if (!view->IsBoundsVisible(box))
		return;

	if (pObj->IsKindOf(RUNTIME_CLASS(CGroup)))
	{
		CGroup* pGroup = static_cast<CGroup*>(pObj);
		// If the group is open check children
		if (pGroup->IsOpen())
		{
			for (int i = 0; i < pGroup->GetChildCount(); ++i)
			{
				SelectObjectInRect(pGroup->GetChild(i), view, hc, eSelect);
			}
		}
	}

	if (pObj->HitTestRect(hc))
	{
		if (eSelect == ESelectOp::eSelect)
		{
			SelectObject(pObj);
		}
		else if (eSelect == ESelectOp::eUnselect)
		{
			UnselectObject(pObj);
		}
		else if (eSelect == ESelectOp::eToggle)
		{
			if (pObj->IsSelected())
			{
				UnselectObject(pObj);
			}
			else
			{
				SelectObject(pObj);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
namespace
{
std::vector<std::string> PyGetAllObjects(const string& className, const string& layerName)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	CObjectLayer* pLayer = NULL;
	if (layerName.IsEmpty() == false)
		pLayer = pObjMgr->GetLayersManager()->FindLayerByName(layerName);
	CBaseObjectsArray objects;
	pObjMgr->GetObjects(objects, pLayer);
	int count = pObjMgr->GetObjectCount();
	std::vector<std::string> result;
	for (int i = 0; i < count; ++i)
	{
		if (className.IsEmpty() || objects[i]->GetTypeDescription() == className)
			result.push_back(objects[i]->GetName().GetString());
	}
	return result;
}

std::vector<std::string> PyGetAllLayers()
{
	CObjectLayerManager* pLayerMgr = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	std::vector<std::string> result;
	const auto& layers = pLayerMgr->GetLayers();
	for (size_t i = 0; i < layers.size(); ++i)
		result.push_back(layers[i]->GetName().GetString());
	return result;
}

std::vector<std::string> PyGetNamesOfSelectedObjects()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	std::vector<std::string> result;
	const int selectionCount = pSel->GetCount();
	result.reserve(selectionCount);
	for (int i = 0; i < selectionCount; i++)
	{
		result.push_back(pSel->GetObject(i)->GetName().GetString());
	}
	return result;
}

void PySelectObject(const char* objName)
{
	CUndo undo("Select Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
}

void PySelectAndGoToObject(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);

	if (pObject)
	{
		CUndo undo("Select Object");
		GetIEditorImpl()->GetObjectManager()->ClearSelection();
		GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
		CCryEditApp::GetInstance()->OnGotoSelected();
	}
}

void PyUnselectObjects(const std::vector<std::string>& names)
{
	CUndo undo("Unselect Objects");

	std::vector<CBaseObject*> pBaseObjects;
	for (int i = 0; i < names.size(); i++)
	{
		if (!GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str()))
		{
			throw std::logic_error(string("\"") + names[i].c_str() + "\" is an invalid entity.");
		}
		pBaseObjects.push_back(GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str()));
	}

	for (int i = 0; i < pBaseObjects.size(); i++)
	{
		GetIEditorImpl()->GetObjectManager()->UnselectObject(pBaseObjects[i]);
	}
}

void PySelectObjects(const std::vector<std::string>& names)
{
	CUndo undo("Select Objects");
	CBaseObject* pObject;
	for (size_t i = 0; i < names.size(); ++i)
	{
		pObject = GetIEditorImpl()->GetObjectManager()->FindObject(names[i].c_str());
		if (!pObject)
		{
			throw std::logic_error(string("\"") + names[i].c_str() + "\" is an invalid entity.");
		}
		GetIEditorImpl()->GetObjectManager()->SelectObject(pObject);
	}
}

bool PyIsObjectHidden(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + objName + "\" is an invalid object name.");
	}
	return pObject->IsHidden();
}

void PyHideAllObjects()
{
	CBaseObjectsArray baseObjects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(baseObjects);

	if (baseObjects.size() <= 0)
	{
		throw std::logic_error("Objects not found.");
	}

	CUndo undo("Hide All Objects");
	for (int i = 0; i < baseObjects.size(); i++)
	{
		GetIEditorImpl()->GetObjectManager()->HideObject(baseObjects[i], true);
	}
}

void PyUnHideAllObjects()
{
	CUndo undo("Unhide All Objects");
	GetIEditorImpl()->GetObjectManager()->UnhideAll();
}

void PyHideObject(const char* objName)
{
	CUndo undo("Hide Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->HideObject(pObject, true);
}

void PyUnhideObject(const char* objName)
{
	CUndo undo("Unhide Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->HideObject(pObject, false);
}

void PyFreezeObject(const char* objName)
{
	CUndo undo("Freeze Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->FreezeObject(pObject, true);
}

void PyUnfreezeObject(const char* objName)
{
	CUndo undo("Unfreeze Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->FreezeObject(pObject, false);
}

void PyDeleteObject(const char* objName)
{
	CUndo undo("Delete Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->DeleteObject(pObject);
}

void PyClearSelection()
{
	CUndo undo("Clear Selection");
	GetIEditorImpl()->GetObjectManager()->ClearSelection();
}

int PyGetNumSelectedObjects()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		return pGroup->GetCount();
	}

	return 0;
}

boost::python::tuple PyGetSelectionCenter()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		if (pGroup->GetCount() == 0)
		{
			throw std::runtime_error("Nothing selected");
		}

		const Vec3 center = pGroup->GetCenter();
		return boost::python::make_tuple(center.x, center.y, center.z);
	}

	throw std::runtime_error("Nothing selected");
}

boost::python::tuple PyGetSelectionAABB()
{
	if (const CSelectionGroup* pGroup = GetIEditorImpl()->GetObjectManager()->GetSelection())
	{
		if (pGroup->GetCount() == 0)
		{
			throw std::runtime_error("Nothing selected");
		}

		const AABB aabb = pGroup->GetBounds();
		return boost::python::make_tuple(aabb.min.x, aabb.min.y, aabb.min.z, aabb.max.x, aabb.max.y, aabb.max.z);
	}

	throw std::runtime_error("Nothing selected");
}

string PyGetEntityGeometryFile(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return "";

	string result = "";
	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		result = static_cast<CBrushObject*>(pObject)->GetGeometryFile();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		result = static_cast<CGeomEntity*>(pObject)->GetGeometryFile();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		result = static_cast<CEntityObject*>(pObject)->GetEntityPropertyString("object_Model");
	}

	result.MakeLower();
	result.Replace("/", "\\");
	return result;
}

void PySetEntityGeometryFile(const char* objName, const char* filePath)
{
	CUndo undo("Set entity geometry file");
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return;

	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		static_cast<CBrushObject*>(pObject)->SetGeometryFile(filePath);
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		static_cast<CGeomEntity*>(pObject)->SetGeometryFile(filePath);
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		static_cast<CEntityObject*>(pObject)->SetEntityPropertyString("object_Model", filePath);
	}
}

string PyGetDefaultMaterial(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return "";

	string matName = "";
	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrush = static_cast<CBrushObject*>(pObject);

		if (pBrush->GetIStatObj() == NULL)
			return "";

		if (pBrush->GetIStatObj()->GetMaterial() == NULL)
			return "";

		matName = pBrush->GetIStatObj()->GetMaterial()->GetName();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		IRenderNode* pEngineNode = pEntity->GetEngineNode();

		if (pEngineNode == NULL)
			return "";

		IStatObj* pEntityStatObj = pEngineNode->GetEntityStatObj();
		if (pEntityStatObj == NULL)
			return "";

		matName = pEntityStatObj->GetMaterial()->GetName();
	}

	matName.MakeLower();
	matName.Replace("/", "\\");
	return matName;
}

string PyGetCustomMaterial(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return "";

	CMaterial* pMtl = (CMaterial*)pObject->GetMaterial();
	if (pMtl == NULL)
		return "";

	string matName = pMtl->GetName();
	matName.MakeLower();
	matName.Replace("/", "\\");
	return matName;
}

string PyGetAssignedMaterial(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::runtime_error("Invalid object name.");
	}

	string assignedMaterialName = PyGetCustomMaterial(pName);

	if (assignedMaterialName.GetLength() == 0)
	{
		assignedMaterialName = PyGetDefaultMaterial(pName);
	}

	return assignedMaterialName;
}

void PySetCustomMaterial(const char* objName, const char* matName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return;

	CUndo undo("Set Custom Material");
	pObject->SetMaterial(matName);
}

std::vector<std::string> PyGetFlowGraphsUsingThis(const char* objName)
{
	std::vector<std::string> list;
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject == NULL)
		return list;

	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		std::vector<CHyperFlowGraph*> flowgraphs;
		CHyperFlowGraph* pEntityFG = 0;
		FlowGraphHelpers::FindGraphsForEntity(pEntity, flowgraphs, pEntityFG);
		for (size_t i = 0; i < flowgraphs.size(); ++i)
		{
			CString name;
			FlowGraphHelpers::GetHumanName(flowgraphs[i], name);
			list.push_back(name.GetString());
		}
	}

	return list;
}

boost::python::tuple PyGetObjectPosition(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	Vec3 position = pObject->GetPos();
	return boost::python::make_tuple(position.x, position.y, position.z);
}

boost::python::tuple PyGetWorldObjectPosition(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	Vec3 position = pObject->GetWorldPos();
	return boost::python::make_tuple(position.x, position.y, position.z);
}

void PySetObjectPosition(const char* pName, float fValueX, float fValueY, float fValueZ)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	CUndo undo("Set Object Base Position");
	pObject->SetPos(Vec3(fValueX, fValueY, fValueZ));
}

boost::python::tuple PyGetObjectRotation(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	Ang3 ang = RAD2DEG(Ang3(pObject->GetRotation()));
	return boost::python::make_tuple(ang.x, ang.y, ang.z);
}

void PySetObjectRotation(const char* pName, float fValueX, float fValueY, float fValueZ)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	CUndo undo("Set Object Rotation");
	pObject->SetRotation(Quat(DEG2RAD(Ang3(fValueX, fValueY, fValueZ))));
}

boost::python::tuple PyGetObjectScale(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	Vec3 scaleVec3 = pObject->GetScale();
	return boost::python::make_tuple(scaleVec3.x, scaleVec3.y, scaleVec3.z);
}

void PySetObjectScale(const char* pName, float fValueX, float fValueY, float fValueZ)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + pName + "\" is an invalid object.");
	}
	CUndo undo("Set Object Scale");
	pObject->SetScale(Vec3(fValueX, fValueY, fValueZ));
}

std::vector<std::string> PyGetObjectLayer(const std::vector<std::string>& names)
{
	std::vector<std::string> result;
	std::set<std::string> tempSet;
	CBaseObjectsArray objectArray;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objectArray);

	for (int i = 0; i < objectArray.size(); i++)
	{
		for (int j = 0; j < names.size(); j++)
		{
			if ((objectArray.at(i))->GetName() == names[j].c_str())
			{
				tempSet.insert(std::string((objectArray.at(i))->GetLayer()->GetName()));
			}
		}
	}
	std::set<std::string>::iterator it;
	for (it = tempSet.begin(); it != tempSet.end(); it++)
	{
		result.push_back(it->c_str());
	}
	return result;
}

void PySetObjectLayer(const std::vector<std::string>& names, const char* pLayerName)
{
	CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayerByName(pLayerName);
	if (!pLayer)
	{
		throw std::logic_error("Invalid layer.");
	}

	CBaseObjectsArray objectArray;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objectArray);
	bool isLayerChanged(false);
	CUndo undo("Set Object Layer");
	for (int i = 0; i < objectArray.size(); i++)
	{
		for (int j = 0; j < names.size(); j++)
		{
			if ((objectArray.at(i))->GetName() == names[j].c_str())
			{
				(objectArray.at(i))->SetLayer(pLayer);
				isLayerChanged = true;
			}
		}
	}
	if (isLayerChanged)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, pLayer).Send();
	}
}

std::vector<std::string> PyGetAllObjectsOnLayer(const char* pLayerName)
{
	CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayerByName(pLayerName);
	if (!pLayer)
	{
		throw std::logic_error("Invalid layer.");
	}

	CBaseObjectsArray objectArray;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objectArray);

	std::vector<std::string> vectorObjects;

	for (int i = 0; i < objectArray.size(); i++)
	{
		if ((objectArray.at(i))->GetLayer()->GetName() == string(pLayerName))
		{
			vectorObjects.push_back(static_cast<std::string>((objectArray.at(i))->GetName()));
		}
	}

	return vectorObjects;
}

const char* PyGetObjectParent(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::runtime_error(string("\"") + pName + "\" is an invalid object.");
	}

	CBaseObject* pParentObject = pObject->GetParent();
	if (!pParentObject)
	{
		return "";
	}
	return pParentObject->GetName();
}

std::vector<std::string> PyGetObjectChildren(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::runtime_error(string("\"") + pName + "\" is an invalid object.");
	}
	std::vector<_smart_ptr<CBaseObject>> objectVector;
	std::vector<std::string> result;
	pObject->GetAllChildren(objectVector);
	if (objectVector.empty())
	{
		return result;
	}

	for (std::vector<_smart_ptr<CBaseObject>>::iterator it = objectVector.begin(); it != objectVector.end(); it++)
	{
		result.push_back(static_cast<std::string>(it->get()->GetName()));
	}
	return result;
}

void PyGenerateCubemap(const char* pObjectName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjectName);
	if (!pObject)
	{
		throw std::runtime_error("Invalid object.");
	}

	if (pObject->GetTypeName() != "EnvironmentProbe")
	{
		throw std::runtime_error("Invalid environment probe.");
	}
	static_cast<CEnvironementProbeObject*>(pObject)->GenerateCubemap();
}

void PyGenerateAllCubemaps()
{
	CubemapUtils::RegenerateAllEnvironmentProbeCubemaps();
}

string PyGetObjectType(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::runtime_error("Invalid object.");
	}

	return pObject->GetTypeName();
}

int PyGetObjectLodsCount(const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pName);
	if (!pObject)
	{
		throw std::runtime_error("Invalid object name.");
	}

	if (!pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		throw std::runtime_error("Invalid object type.");
	}

	IStatObj* pStatObj = pObject->GetIStatObj();
	if (!pObject)
	{
		throw std::runtime_error("Invalid stat object");
	}

	IStatObj::SStatistics objectStats;
	pStatObj->GetStatistics(objectStats);

	return objectStats.nLods;
}

void PyRenameObject(const char* pOldName, const char* pNewName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pOldName);
	if (!pObject)
	{
		throw std::runtime_error("Could not find object");
	}

	if (strcmp(pNewName, "") == 0 || GetIEditorImpl()->GetObjectManager()->FindObject(pNewName))
	{
		throw std::runtime_error("Invalid object name.");
	}

	CUndo undo("Rename object");
	pObject->SetName(pNewName);
}

void PyAttachObject(const char* pParent, const char* pChild, const char* pAttachmentType, const char* pAttachmentTarget)
{
	CBaseObject* pParentObject = GetIEditorImpl()->GetObjectManager()->FindObject(pParent);
	if (!pParentObject)
	{
		throw std::runtime_error("Could not find parent object");
	}

	CBaseObject* pChildObject = GetIEditorImpl()->GetObjectManager()->FindObject(pChild);
	if (!pChildObject)
	{
		throw std::runtime_error("Could not find child object");
	}

	CEntityObject::EAttachmentType attachmentType = CEntityObject::eAT_Pivot;
	if (strcmp(pAttachmentType, "CharacterBone") == 0)
	{
		attachmentType = CEntityObject::eAT_CharacterBone;
	}
	else if (strcmp(pAttachmentType, "GeomCacheNode") == 0)
	{
		attachmentType = CEntityObject::eAT_GeomCacheNode;
	}
	else if (strcmp(pAttachmentType, "") != 0)
	{
		throw std::runtime_error("Invalid attachment type");
	}

	if (attachmentType != CEntityObject::eAT_Pivot)
	{
		if (!pParentObject->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !pChildObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			throw std::runtime_error("Both parent and child must be entities if attaching to bone or node");
		}

		if (strcmp(pAttachmentTarget, "") == 0)
		{
			throw std::runtime_error("Please specify a target");
		}

		CEntityObject* pChildEntity = static_cast<CEntityObject*>(pChildObject);
		pChildEntity->SetAttachType(attachmentType);
		pChildEntity->SetAttachTarget(pAttachmentTarget);
	}

	CUndo undo("Attach object");
	pParentObject->AttachChild(pChildObject);
}

void PyDetachObject(const char* pObjectName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjectName);
	if (!pObject)
	{
		throw std::runtime_error("Could not find object");
	}

	if (!pObject->GetParent())
	{
		throw std::runtime_error("Object has no parent");
	}

	CUndo undo("Detach object");
	pObject->DetachThis(true);
}

void PyAddEntityLink(const char* pObjectName, const char* pTargetName, const char* pName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(pObjectName);
	if (!pObject)
	{
		throw std::runtime_error("Could not find object");
	}

	CBaseObject* pTargetObject = GetIEditorImpl()->GetObjectManager()->FindObject(pTargetName);
	if (!pTargetObject)
	{
		throw std::runtime_error("Could not find target object");
	}

	if (!pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !pTargetObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		throw std::runtime_error("Both object and target must be entities");
	}

	if (strcmp(pName, "") == 0)
	{
		throw std::runtime_error("Please specify a name");
	}

	CUndo undo("Add entity link");
	CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
	pEntity->AddEntityLink(pName, pTargetObject->GetId());
}
}

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetAllObjects, general, get_all_objects,
                                          "Gets the name list of all objects of a certain type in a specific layer or in the whole level if an invalid layer name given.",
                                          "general.get_all_objects(str className, str layerName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetAllLayers, general, get_all_layers,
                                          "Gets the list of all layer names in the level.",
                                          "general.get_all_layers()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetNamesOfSelectedObjects, general, get_names_of_selected_objects,
                                          "Get the name from selected object/objects.",
                                          "general.get_names_of_selected_objects()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySelectObject, general, select_object,
                                     "Selects a specified object.",
                                     "general.select_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySelectAndGoToObject, general, select_and_go_to_object,
                                     "Selects and focuses camera on specified object.",
                                     "general.select_and_go_to_object(str objectName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyUnselectObjects, general, unselect_objects,
                                          "Unselects a list of objects.",
                                          "general.unselect_objects(list [str objectName,])");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySelectObjects, general, select_objects,
                                          "Selects a list of objects.",
                                          "general.select_objects(list [str objectName,])");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyIsObjectHidden, general, is_object_hidden,
                                     "Checks if object is hidden and returns a bool value.",
                                     "general.is_object_hidden(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyHideAllObjects, general, hide_all_objects,
                                     "Hides all objects.",
                                     "general.hide_all_object()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyUnHideAllObjects, general, unhide_all_objects,
                                     "Unhides all object.",
                                     "general.unhide_all_object()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyHideObject, general, hide_object,
                                     "Hides a specified object.",
                                     "general.hide_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyUnhideObject, general, unhide_object,
                                     "Unhides a specified object.",
                                     "general.unhide_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyFreezeObject, general, freeze_object,
                                     "Freezes a specified object.",
                                     "general.freeze_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyUnfreezeObject, general, unfreeze_object,
                                     "Unfreezes a specified object.",
                                     "general.unfreeze_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyDeleteObject, general, delete_object,
                                     "Deletes a specified object.",
                                     "general.delete_object(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyClearSelection, general, clear_selection,
                                     "Clears selection.",
                                     "general.clear_selection()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetNumSelectedObjects, general, get_num_selected,
                                     "Returns the number of selected objects",
                                     "general.get_num_selected()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetSelectionCenter, general, get_selection_center,
                                     "Returns the center point of the selection group",
                                     "general.selection_center()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetSelectionAABB, general, get_selection_aabb,
                                     "Returns the aabb of the selection group",
                                     "general.selection_aabb()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetEntityGeometryFile, general, get_entity_geometry_file,
                                     "Gets the geometry file name of a given entity.",
                                     "general.get_entity_geometry_file(str geometryName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetEntityGeometryFile, general, set_entity_geometry_file,
                                     "Sets the geometry file name of a given entity.",
                                     "general.set_entity_geometry_file(str geometryName, str cgfName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetDefaultMaterial, general, get_default_material,
                                     "Gets the default material of a given object geometry.",
                                     "general.get_default_material(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetCustomMaterial, general, get_custom_material,
                                     "Gets the user material assigned to a given object geometry.",
                                     "general.get_custom_material(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetCustomMaterial, general, set_custom_material,
                                     "Assigns a user material to a given object geometry.",
                                     "general.set_custom_material(str objectName, str materialName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetFlowGraphsUsingThis, general, get_flowgraphs_using_this,
                                          "Gets the name list of all flow graphs which control this object.",
                                          "general.get_flowgraphs_using_this");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectPosition, general, get_position,
                                     "Gets the position of an object.",
                                     "general.get_position(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetWorldObjectPosition, general, get_world_position,
                                     "Gets the world position of an object.",
                                     "general.get_world_position(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetObjectPosition, general, set_position,
                                     "Sets the position of an object.",
                                     "general.set_position(str objectName, float xValue, float yValue, float zValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectRotation, general, get_rotation,
                                     "Gets the rotation of an object.",
                                     "general.get_rotation(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetObjectRotation, general, set_rotation,
                                     "Sets the rotation of the object.",
                                     "general.set_rotation(str objectName, float xValue, float yValue, float zValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectScale, general, get_scale,
                                     "Gets the scale of an object.",
                                     "general.get_scale(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySetObjectScale, general, set_scale,
                                     "Sets the scale of ab object.",
                                     "general.set_scale(str objectName, float xValue, float yValue, float zValue)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateAllCubemaps, general, generate_all_cubemaps,
                                     "Generates cubemaps for all environment probes in level.",
                                     "general.generate_all_cubemaps()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectLayer, general, get_object_layer,
                                          "Gets the name of the layer of an object.",
                                          "general.get_object_layer(list [str objectName,])");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PySetObjectLayer, general, set_object_layer,
                                          "Moves an object to an other layer.",
                                          "general.set_object_layer(list [str objectName,], str layerName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetAllObjectsOnLayer, general, get_all_objects_of_layer,
                                          "Gets all objects of a layer.",
                                          "general.get_all_objects_of_layer(str layerName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectParent, general, get_object_parent,
                                          "Gets parent name of an object.",
                                          "general.get_object_parent(str objectName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectChildren, general, get_object_children,
                                          "Gets children names of an object.",
                                          "general.get_object_children(str objectName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGenerateCubemap, general, generate_cubemap,
                                          "Generates a cubemap (only for environment probes).",
                                          "general.generate_cubemap(str environmentProbeName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectType, general, get_object_type,
                                          "Gets the type of an object as a string.",
                                          "general.get_object_type(str objectName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetAssignedMaterial, general, get_assigned_material,
                                          "Gets the name of assigned material.",
                                          "general.get_assigned_material(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyGetObjectLodsCount, general, get_object_lods_count,
                                     "Gets the number of lods of the material of an object.",
                                     "general.get_object_lods_count(str objectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRenameObject, general, rename_object,
                                     "Renames object with oldObjectName to newObjectName",
                                     "general.rename_object(str oldObjectName, str newObjectName)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyAttachObject, general, attach_object,
                                     "Attaches object with childObjectName to parentObjectName. If attachmentType is 'CharacterBone' or 'GeomCacheNode' attachmentTarget specifies the bone or node path",
                                     "general.attach_object(str parentObjectName, str childObjectName, str attachmentType, str attachmentTarget)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyDetachObject, general, detach_object,
                                     "Detaches object from its parent",
                                     "general.detach_object(str object)");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyAddEntityLink, general, add_entity_link,
                                     "Adds an entity link to objectName to targetName with name",
                                     "general.add_entity_link(str objectName, str targetName, str name)");

REGISTER_PYTHON_ENUM_BEGIN(ObjectType, general, object_type)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_GROUP, group)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_TAGPOINT, tagpoint)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_AIPOINT, aipoint)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_ENTITY, entity)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_SHAPE, shape)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_VOLUME, volume)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_BRUSH, brush)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_PREFAB, prefab)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_SOLID, solid)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_CLOUD, cloud)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_ROAD, road)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_OTHER, other)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_DECAL, decal)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_DISTANCECLOUD, distanceclound)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_TELEMETRY, telemetry)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_REFPICTURE, refpicture)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_GEOMCACHE, geomcache)
REGISTER_PYTHON_ENUM_ITEM(OBJTYPE_ANY, any)
REGISTER_PYTHON_ENUM_END

