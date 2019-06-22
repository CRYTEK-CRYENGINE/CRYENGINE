// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

// Sandbox
#include "IEditorImpl.h"
#include "Objects/ObjectManager.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/EnvironmentProbeObject.h"
#include "Objects/GeomEntity.h"
#include "Material/Material.h"
#include "HyperGraph/FlowGraphHelpers.h"
#include "Util/CubemapUtils.h"
#include "Vegetation/VegetationMap.h"
#include "Dialogs/DuplicatedObjectsHandlerDlg.h"

// EditorCommon
#include <Commands/ICommandManager.h>
#include <IUndoObject.h>
#include <Controls/QuestionDialog.h>
#include <FileDialogs/SystemFileDialog.h>
#include <Objects/ObjectLoader.h>

// CryCommon
#include <CrySystem/ICryLink.h>
#include <Cry3DEngine/IStatObj.h>
#include <Cry3DEngine/IRenderNode.h>

//////////////////////////////////////////////////////////////////////////
namespace Private_ObjectCommands
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

bool PyIsObjectHidden(const char* objName)
{
	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (!pObject)
	{
		throw std::logic_error(string("\"") + objName + "\" is an invalid object name.");
	}
	return pObject->IsHidden();
}

void HideAllObjects()
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

void UnHideAllObjects()
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

void UnlockAllObjects()
{
	CUndo undo("Unlock All Objects");
	GetIEditorImpl()->GetObjectManager()->UnfreezeAll();
}

void PyLockObject(const char* objName)
{
	CUndo undo("Lock Object");

	CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(objName);
	if (pObject)
		GetIEditorImpl()->GetObjectManager()->FreezeObject(pObject, true);
}

void PyUnlockObject(const char* objName)
{
	CUndo undo("Unlock Object");

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
	for (const auto& str : tempSet)
	{
		result.push_back(str);
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
	std::vector<_smart_ptr<CBaseObject>> descendants;
	std::vector<std::string> result;
	pObject->GetAllDescendants(descendants);
	if (descendants.empty())
	{
		return result;
	}

	for (std::vector<_smart_ptr<CBaseObject>>::iterator it = descendants.begin(); it != descendants.end(); ++it)
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

	CUndo undo("Link Object");
	pParentObject->LinkTo(pChildObject);
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

void ValidatePositions()
{
	IObjectManager* objMan = GetIEditorImpl()->GetObjectManager();

	if (!objMan)
		return;

	int objCount = objMan->GetObjectCount();
	AABB bbox1;
	AABB bbox2;
	int bugNo = 0;

	std::vector<CBaseObject*> objects;
	objMan->GetObjects(objects);

	std::vector<CBaseObject*> foundObjects;

	std::vector<CryGUID> objIDs;

	for (int i1 = 0; i1 < objCount; ++i1)
	{
		CBaseObject* pObj1 = objects[i1];

		if (!pObj1)
			continue;

		// Ignore groups in search
		if (pObj1->GetType() == OBJTYPE_GROUP)
			continue;

		// Object must have geometry
		if (!pObj1->GetGeometry())
			continue;

		// Ignore solids
		if (pObj1->GetType() == OBJTYPE_SOLID)
			continue;

		pObj1->GetBoundBox(bbox1);

		// Check if object has vegetation inside its bbox
		CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		if (pVegetationMap && !pVegetationMap->IsAreaEmpty(bbox1))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s has vegetation object(s) inside %s", pObj1->GetName(),
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "selection.select_and_go_to %s", pObj1->GetName()));
		}

		// Check if object has other objects inside its bbox
		foundObjects.clear();
		objMan->FindObjectsInAABB(bbox1, foundObjects);

		for (int i2 = 0; i2 < foundObjects.size(); ++i2)
		{
			CBaseObject* pObj2 = objects[i2];
			if (!pObj2)
				continue;

			if (pObj2->GetId() == pObj1->GetId())
				continue;

			if (pObj2->GetParent())
				continue;

			if (pObj2->GetType() == OBJTYPE_SOLID)
				continue;

			if (stl::find(objIDs, pObj2->GetId()))
				continue;

			if ((!pObj2->GetGeometry() || pObj2->GetType() == OBJTYPE_SOLID) && (pObj2->GetType()))
				continue;

			pObj2->GetBoundBox(bbox2);

			if (!bbox1.IsContainPoint(bbox2.max))
				continue;

			if (!bbox1.IsContainPoint(bbox2.min))
				continue;

			objIDs.push_back(pObj2->GetId());
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s inside %s object %s", (const char*)pObj2->GetName(), pObj1->GetName(),
			           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "selection.select_and_go_to %s", pObj2->GetName()));
			++bugNo;
		}
	}
}

void ResolveMissingObjects()
{
	GetIEditorImpl()->GetObjectManager()->ResolveMissingObjects();
}

void SaveObjectsToGrpFile()
{
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters << CExtensionFilter("Object Group Files (*.grp)", "grp");

	QString filePath = CSystemFileDialog::RunExportFile(runParams, nullptr);
	if (!filePath.isEmpty())
	{
		CWaitCursor wait;
		const CSelectionGroup* sel = GetIEditorImpl()->GetSelection();

		XmlNodeRef root = XmlHelpers::CreateXmlNode("Objects");
		CObjectArchive ar(GetIEditorImpl()->GetObjectManager(), root, false);
		// Save all objects to XML.
		for (int i = 0; i < sel->GetCount(); i++)
		{
			ar.SaveObject(sel->GetObject(i));
		}
		XmlHelpers::SaveXmlNode(root, filePath.toLocal8Bit().data());
	}
}

struct SDuplicatedObject
{
	SDuplicatedObject(const string& name, const CryGUID& id)
	{
		m_name = name;
		m_id = id;
	}
	string  m_name;
	CryGUID m_id;
};

void GatherAllObjects(XmlNodeRef node, std::vector<SDuplicatedObject>& outDuplicatedObjects)
{
	if (!stricmp(node->getTag(), "Object"))
	{
		CryGUID guid;
		if (node->getAttr("Id", guid))
		{
			if (GetIEditorImpl()->GetObjectManager()->FindObject(guid))
			{
				string name;
				node->getAttr("Name", name);
				outDuplicatedObjects.push_back(SDuplicatedObject(name, guid));
			}
		}
	}

	for (int i = 0, nChildCount(node->getChildCount()); i < nChildCount; ++i)
	{
		XmlNodeRef childNode = node->getChild(i);
		if (childNode == NULL)
			continue;
		GatherAllObjects(childNode, outDuplicatedObjects);
	}
}

void LoadObjectsFromGrpFile()
{
	// Load objects from .grp file.
	CSystemFileDialog::RunParams runParams;
	runParams.extensionFilters << CExtensionFilter("Object Group Files (*.grp)", "grp");

	QString filePath = CSystemFileDialog::RunImportFile(runParams, nullptr);
	if (filePath.isEmpty())
	{
		return;
	}

	CWaitCursor wait;

	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filePath.toLocal8Bit().data());
	if (!root)
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Error at loading group file."));
		return;
	}

	std::vector<SDuplicatedObject> duplicatedObjects;
	GatherAllObjects(root, duplicatedObjects);

	CDuplicatedObjectsHandlerDlg::EResult result(CDuplicatedObjectsHandlerDlg::eResult_None);
	int nDuplicatedObjectSize(duplicatedObjects.size());

	if (!duplicatedObjects.empty())
	{
		string msg;
		msg.Format("The following object(s) already exist(s) in the level.\r\n\r\n");

		for (int i = 0; i < nDuplicatedObjectSize; ++i)
		{
			msg += "\t";
			msg += duplicatedObjects[i].m_name;
			if (i < nDuplicatedObjectSize - 1)
				msg += "\r\n";
		}

		CDuplicatedObjectsHandlerDlg dlg(msg);
		if (dlg.DoModal() == IDCANCEL)
			return;
		result = dlg.GetResult();
	}

	CUndo undo("Load Objects");
	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	pObjectManager->ClearSelection();

	CObjectArchive ar(pObjectManager, root, true);

	CRandomUniqueGuidProvider guidProvider;
	if (result == CDuplicatedObjectsHandlerDlg::eResult_Override)
	{
		for (int i = 0; i < nDuplicatedObjectSize; ++i)
		{
			CBaseObject* pObj = pObjectManager->FindObject(duplicatedObjects[i].m_id);
			if (pObj)
				pObjectManager->DeleteObject(pObj);
		}
	}
	else if (result == CDuplicatedObjectsHandlerDlg::eResult_CreateCopies)
	{
		ar.SetGuidProvider(&guidProvider);
	}

	ar.LoadInCurrentLayer(true);
	pObjectManager->CreateAndSelectObjects(ar);
	GetIEditorImpl()->SetModifiedFlag();
}
}

DECLARE_PYTHON_MODULE(object);

// Visibility
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyIsObjectHidden, object, is_hidden,
                                     "Checks if object is hidden and returns a bool value.",
                                     "object.is_hidden(str objectName)");
REGISTER_COMMAND_REMAPPING(general, is_object_hidden, object, is_hidden)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::HideAllObjects, object, hide_all,
                                   CCommandDescription("Hides all objects"))
REGISTER_EDITOR_UI_COMMAND_DESC(object, hide_all, "Hide All Objects", "", "icons:General/Visibility_False.ico", false)
REGISTER_COMMAND_REMAPPING(general, hide_all_objects, object, hide_all)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::UnHideAllObjects, object, show_all,
                                   CCommandDescription("Shows all objects"));
REGISTER_EDITOR_UI_COMMAND_DESC(object, show_all, "Unhide All Objects", "Shift+H", "icons:General/Visibility_True.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionUnhide_All, object, show_all)
REGISTER_COMMAND_REMAPPING(general, unhide_all_objects, object, show_all)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyHideObject, object, hide,
                                     "Hides a specified object.",
                                     "object.hide(str objectName)");
REGISTER_COMMAND_REMAPPING(general, hide_object, object, hide)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyUnhideObject, object, show,
                                     "Unhides a specified object.",
                                     "object.show(str objectName)");
REGISTER_COMMAND_REMAPPING(general, unhide_object, object, show)

// Editability
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyLockObject, object, lock,
                                     "Locks specified object",
                                     "object.lock(str objectName)");
REGISTER_COMMAND_REMAPPING(general, freeze_object, object, lock)
REGISTER_COMMAND_REMAPPING(object, make_uneditable, object, lock)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyUnlockObject, object, unlock,
                                     "Unlocks specified object",
                                     "object.unlock(str objectName)");
REGISTER_COMMAND_REMAPPING(general, unfreeze_object, object, unlock)
REGISTER_COMMAND_REMAPPING(object, make_editable, object, unlock)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::UnlockAllObjects, object, unlock_all,
                                   CCommandDescription("Unlocks all objects"));
REGISTER_EDITOR_UI_COMMAND_DESC(object, unlock_all, "Unlock All Objects", "Shift+F", "icons:general_lock_false.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionUnfreeze_All, object, unlock_all)
REGISTER_COMMAND_REMAPPING(object, make_all_editable, object, unlock_all)

// General
REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::PyGenerateAllCubemaps, object, generate_all_cubemaps,
                                   CCommandDescription("Generates cubemaps for all environment probes in level."))
REGISTER_EDITOR_UI_COMMAND_DESC(object, generate_all_cubemaps, "Regenerate All Cubemaps", "", "", false)
REGISTER_COMMAND_REMAPPING(general, generate_all_cubemaps, object, generate_all_cubemaps)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyDeleteObject, object, delete,
                                     "Deletes a specified object.",
                                     "object.delete(str objectName)");
REGISTER_COMMAND_REMAPPING(general, delete_object, object, delete)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetAllObjects, object, get_all_objects,
                                          "Gets the name list of all objects of a certain type in a specific layer or in the whole level if an invalid layer name given.",
                                          "object.get_all_objects(str className, str layerName)");
REGISTER_COMMAND_REMAPPING(general, get_all_objects, object, get_all_objects)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetDefaultMaterial, object, get_default_material,
                                     "Gets the default material of a given object geometry.",
                                     "object.get_default_material(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_default_material, object, get_default_material)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetCustomMaterial, object, get_custom_material,
                                     "Gets the user material assigned to a given object geometry.",
                                     "object.get_custom_material(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_custom_material, object, get_custom_material)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PySetCustomMaterial, object, set_custom_material,
                                     "Assigns a user material to a given object geometry.",
                                     "object.set_custom_material(str objectName, str materialName)");
REGISTER_COMMAND_REMAPPING(general, set_custom_material, object, set_custom_material)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetFlowGraphsUsingThis, object, get_flowgraphs_using_this,
                                          "Gets the name list of all flow graphs which control this object.",
                                          "object.get_flowgraphs_using_this");
REGISTER_COMMAND_REMAPPING(general, get_flowgraphs_using_this, object, get_flowgraphs_using_this)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectPosition, object, get_position,
                                     "Gets the position of an object.",
                                     "object.get_position(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_position, object, get_position)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetWorldObjectPosition, object, get_world_position,
                                     "Gets the world position of an object.",
                                     "object.get_world_position(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_world_position, object, get_world_position)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PySetObjectPosition, object, set_position,
                                     "Sets the position of an object.",
                                     "object.set_position(str objectName, float xValue, float yValue, float zValue)");
REGISTER_COMMAND_REMAPPING(general, set_position, object, set_position)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectRotation, object, get_rotation,
                                     "Gets the rotation of an object.",
                                     "object.get_rotation(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_rotation, object, get_rotation)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PySetObjectRotation, object, set_rotation,
                                     "Sets the rotation of the object.",
                                     "object.set_rotation(str objectName, float xValue, float yValue, float zValue)");
REGISTER_COMMAND_REMAPPING(general, set_rotation, object, set_rotation)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectScale, object, get_scale,
                                     "Gets the scale of an object.",
                                     "object.get_scale(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_scale, object, get_scale)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PySetObjectScale, object, set_scale,
                                     "Sets the scale of ab object.",
                                     "object.set_scale(str objectName, float xValue, float yValue, float zValue)");
REGISTER_COMMAND_REMAPPING(general, set_scale, object, set_scale)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectLayer, object, get_object_layer,
                                          "Gets the name of the layer of an object.",
                                          "object.get_object_layer(list [str objectName,])");
REGISTER_COMMAND_REMAPPING(general, get_object_layer, object, get_object_layer)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PySetObjectLayer, object, set_object_layer,
                                          "Moves an object to an other layer.",
                                          "object.set_object_layer(list [str objectName,], str layerName)");
REGISTER_COMMAND_REMAPPING(general, set_object_layer, object, set_object_layer)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetAllObjectsOnLayer, object, get_all_objects_of_layer,
                                          "Gets all objects of a layer.",
                                          "object.get_all_objects_of_layer(str layerName)");
REGISTER_COMMAND_REMAPPING(general, get_all_objects_of_layer, object, get_all_objects_of_layer)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectParent, object, get_object_parent,
                                          "Gets parent name of an object.",
                                          "object.get_object_parent(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_object_parent, object, get_object_parent)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectChildren, object, get_object_children,
                                          "Gets children names of an object.",
                                          "object.get_object_children(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_object_children, object, get_object_children)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGenerateCubemap, object, generate_cubemap,
                                          "Generates a cubemap (only for environment probes).",
                                          "object.generate_cubemap(str environmentProbeName)");
REGISTER_COMMAND_REMAPPING(general, generate_cubemap, object, generate_cubemap)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectType, object, get_object_type,
                                          "Gets the type of an object as a string.",
                                          "object.get_object_type(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_object_type, object, get_object_type)

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetAssignedMaterial, object, get_assigned_material,
                                          "Gets the name of assigned material.",
                                          "object.get_assigned_material(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_assigned_material, object, get_assigned_material)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyGetObjectLodsCount, object, get_object_lods_count,
                                     "Gets the number of lods of the material of an object.",
                                     "object.get_object_lods_count(str objectName)");
REGISTER_COMMAND_REMAPPING(general, get_object_lods_count, object, get_object_lods_count)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyRenameObject, object, rename_object,
                                     "Renames object with oldObjectName to newObjectName",
                                     "object.rename_object(str oldObjectName, str newObjectName)");
REGISTER_COMMAND_REMAPPING(general, rename_object, object, rename_object)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyAttachObject, object, attach_object,
                                     "Attaches object with childObjectName to parentObjectName. If attachmentType is 'CharacterBone' or 'GeomCacheNode' attachmentTarget specifies the bone or node path",
                                     "object.attach_object(str parentObjectName, str childObjectName, str attachmentType, str attachmentTarget)");
REGISTER_COMMAND_REMAPPING(general, attach_object, object, attach_object)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_ObjectCommands::PyDetachObject, object, detach_object,
                                     "Detaches object from its parent",
                                     "object.detach_object(str object)");
REGISTER_COMMAND_REMAPPING(general, detach_object, object, detach_object)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::ValidatePositions, object, validate_positions,
                                   CCommandDescription("Checks if there are objects placed within other objects that might be obstructed"))
REGISTER_EDITOR_UI_COMMAND_DESC(object, validate_positions, "Check Object Positions", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionCheck_Object_Positions, object, validate_positions)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::ResolveMissingObjects, object, resolve_missing_objects_materials,
                                   CCommandDescription("Resolve missing objects and materials"))
REGISTER_EDITOR_UI_COMMAND_DESC(object, resolve_missing_objects_materials, "Resolve Missing Objects/Materials", "", "", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionResolve_Missing_Objects, object, resolve_missing_objects_materials)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::SaveObjectsToGrpFile, object, save_to_grp,
                                   CCommandDescription("Saves selected objects into grp file"))
REGISTER_EDITOR_UI_COMMAND_DESC(object, save_to_grp, "Save Selected Objects", "Ctrl+Alt+S", "icons:MaterialEditor/Material_Save.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionSave_Selected_Objects, object, save_to_grp)

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_ObjectCommands::LoadObjectsFromGrpFile, object, load_from_grp,
                                   CCommandDescription("Load objects from grp file"))
REGISTER_EDITOR_UI_COMMAND_DESC(object, load_from_grp, "Load Object Group", "Ctrl+Alt+L", "icons:MaterialEditor/Material_Load.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionLoad_Selected_Objects, object, load_from_grp)

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
