// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectLayerManager.h"
#include "Objects/ObjectLoader.h"
#include "GameEngine.h"
#include "Dialogs/CheckOutDialog.h"

#include <CryGame/IGameFramework.h>
#include <CrySystem/ICryLink.h>
#include <ISourceControl.h>
#include "EntityObject.h"
#include "SplineObject.h"
#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/FlowGraph.h"
#include "HyperGraph/FlowGraphNode.h"

#include "Controls/QuestionDialog.h"
#include "Util/BoostPythonHelpers.h"
#include "Material/Material.h"
#include <CryString/StringUtils.h>
#include "RoadObject.h"
#include <FilePathUtil.h>
#include <QtUtil.h>

#include <EditorFramework/Editor.h>
#include <EditorFramework/Preferences.h>
#include <Preferences/GeneralPreferences.h>

namespace Private_ObjectLayerManager
{

//! Undo Create/Delete Layer
class CUndoLayerCreateDelete : public IUndoObject
{
public:
	CUndoLayerCreateDelete(CObjectLayer* pLayer, bool bCreate)
		: m_bCreate(bCreate)
	{
		m_layerGuid = pLayer->GetGUID();
		m_undo = XmlHelpers::CreateXmlNode("Undo");
		pLayer->SerializeBase(m_undo, false);
		pLayer->Serialize(m_undo, false);
	}
protected:
	virtual const char* GetDescription() { return m_bCreate ? "Create Layer" : "Delete Layer"; };

	virtual void Undo(bool bUndo) override
	{
		if (m_bCreate)
			DeleteLayer();
		else
			CreateLayer();
	}

	virtual void Redo() override
	{
		if (m_bCreate)
			CreateLayer();
		else
			DeleteLayer();
	}

private:
	void CreateLayer()
	{
		auto pLayersManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		auto pLayer = pLayersManager->CreateLayerInstance();
		pLayer->SerializeBase(m_undo, true);
		pLayer->Serialize(m_undo, true);

		if (!pLayersManager->FindLayer(m_layerGuid))
			pLayersManager->AddLayer(pLayer);

		pLayersManager->ResolveLayerParents(true, false);
	}

	void DeleteLayer()
	{
		auto pLayersManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		auto pLayer = pLayersManager->FindLayer(m_layerGuid);
		if (pLayer)
			pLayersManager->DeleteLayer(pLayer);
	}

	CryGUID    m_layerGuid;
	XmlNodeRef m_undo;
	bool       m_bCreate;
};

class CUndoCurrentLayer : public IUndoObject
{
public:
	CUndoCurrentLayer(CObjectLayer* pLayer)
	{
		m_undoName = pLayer->GetName();
	}
protected:
	virtual const char* GetDescription() { return "Select Layer"; };

	virtual void        Undo(bool bUndo)
	{
		CObjectLayerManager* pLayerMan = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		if (bUndo)
		{
			CObjectLayer* pLayer = pLayerMan->GetCurrentLayer();
			if (pLayer)
				m_redoName = pLayer->GetName();
		}

		CObjectLayer* pLayer = pLayerMan->FindLayerByName(m_undoName);
		if (pLayer)
			pLayerMan->SetCurrentLayer(pLayer);
	}

	virtual void Redo()
	{
		CObjectLayerManager* pLayerMan = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		CObjectLayer* pLayer = pLayerMan->FindLayerByName(m_redoName);
		if (pLayer)
			pLayerMan->SetCurrentLayer(pLayer);
	}

private:
	string m_undoName;
	string m_redoName;
};

};

void CLayerChangeEvent::Send() const
{
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->NotifyLayerChange(*this);
}

CObjectLayerManager::CObjectLayerManager(CObjectManager* pObjectManager) :
	m_pObjectManager(pObjectManager),
	m_layersPath(LAYER_PATH),
	m_bCanModifyLayers(true),
	m_bOverwriteDuplicates(false),
	m_visibleSetLayer(CryGUID::Null())
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CObjectLayerManager::~CObjectLayerManager()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CObjectLayerManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneOpen:
		m_bCanModifyLayers = false;
		break;
	case eNotify_OnEndNewScene:
	case eNotify_OnEndSceneOpen:
		m_bCanModifyLayers = true;
		break;
	}
}

bool CObjectLayerManager::CanModifyLayers() const
{
	// If loading a level or creating a level backup, don't allow changing modify state of layers
	return m_bCanModifyLayers;
}

const std::vector<CObjectLayer*>& CObjectLayerManager::GetLayers() const
{
	if (m_layersCache.empty())
	{
		m_layersCache.reserve(m_layersMap.size());
		for (auto it = m_layersMap.cbegin(); it != m_layersMap.cend(); ++it)
		{
			m_layersCache.push_back(it->second);
		}
	}
	return m_layersCache;
}

void CObjectLayerManager::ClearLayers(bool bNotify /*= true*/)
{
	//TODO : is this all layers ? This should notify UPDATE_ALL
	// Erase all removable layers.
	LayersMap::iterator it, itnext;
	for (LayersMap::iterator it = m_layersMap.begin(); it != m_layersMap.end(); it = itnext)
	{
		CObjectLayer* pLayer = it->second;

		//CLayerChangeEvent(CLayerChangeEvent::LE_BEFORE_REMOVE, pLayer).Send();

		itnext = it;
		itnext++;
		m_layersMap.erase(it);

		//CLayerChangeEvent(CLayerChangeEvent::LE_AFTER_REMOVE, pLayer).Send();
	}

	m_layersCache.clear();

	m_pCurrentLayer = 0;
	if (!m_layersMap.empty())
	{
		SetCurrentLayer(m_layersMap.begin()->second);
	}

	if (bNotify)
		CLayerChangeEvent(CLayerChangeEvent::LE_UPDATE_ALL).Send();
}

uint CObjectLayerManager::HasLayers() const
{
	return !m_layersMap.empty();
}

CObjectLayer* CObjectLayerManager::CreateLayer(EObjectLayerType layerType/*= eObjectLayerType_Layer*/, CObjectLayer* pParent/*= nullptr*/)
{
	string name;
	if (layerType == eObjectLayerType_Layer)
		name = "New_Layer";
	else if (layerType == eObjectLayerType_Folder)
		name = "New_Folder";
	else if (layerType == eObjectLayerType_Terrain)
		name = "Terrain";
	else
		CRY_ASSERT_MESSAGE(0, "Please define a default name for this layer type");

	string finalName = name;

	int count = 0;
	while (FindAnyLayerByName(finalName.c_str()))
		finalName = name + "_" + CryStringUtils::toString(++count);

	return CreateLayer(finalName.c_str(), layerType, pParent);
}

CObjectLayer* CObjectLayerManager::CreateLayer(const char* szName, EObjectLayerType layerType/*= eObjectLayerType_Layer*/, CObjectLayer* pParent/*= nullptr*/)
{
	using namespace Private_ObjectLayerManager;
	CRY_ASSERT((!pParent || pParent->GetLayerType() == eObjectLayerType_Folder) && layerType != eObjectLayerType_Size);

	bool isOnlyLayer = m_layersMap.empty();

	CObjectLayer* pLayer = CObjectLayer::Create(szName, layerType);
	AddLayer(pLayer);
	if (pParent)
		pParent->AddChild(pLayer);

	pLayer->SetVisible(true);

	// If it's the only layer in the level or there's no active layer
	if (isOnlyLayer || !GetCurrentLayer())
		SetCurrentLayer(pLayer);

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoLayerCreateDelete(pLayer, true));
	return pLayer;
}

void CObjectLayerManager::AddLayer(CObjectLayer* pLayer, bool bNotify /*= true*/, bool loading /* = false */)
{
	assert(pLayer);

	if (!m_bOverwriteDuplicates)
	{
		CObjectLayer* pPrevLayer;

		// during loading we allow duplicate folder / layer names for backward compatibility reasons,
		// but system still does not support duplicate names in general (a warning should be emmited during loading)
		if (loading)
		{
			pPrevLayer = FindLayerByNameAndType(pLayer->GetName(), pLayer->GetLayerType());
		}
		else
		{
			pPrevLayer = FindAnyLayerByName(pLayer->GetName());
		}

		if (pPrevLayer)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "%s layer already exists. Layer names must be unique", (const char*)pLayer->GetName());
			return;
		}

		if (m_layersMap.find(pLayer->GetGUID()) != m_layersMap.end())
		{
			pPrevLayer = FindLayer(pLayer->GetGUID());
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Duplicate Layer GUID,Layer <%s> collides with layer: %s",
			           (const char*)pPrevLayer->GetName(), (const char*)pLayer->GetName());
			return;
		}
	}

	if (bNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_BEFORE_ADD, pLayer).Send();
	}

	m_layersMap[pLayer->GetGUID()] = pLayer;
	m_layersCache.clear();
	// Remove added layer from list of layers to be deleted on save
	m_toBeDeleted.erase(pLayer->GetLayerFilepath().c_str());

	if (bNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_AFTER_ADD, pLayer).Send();
	}
}

bool CObjectLayerManager::CanDeleteLayer(CObjectLayer* pLayer)
{
	assert(pLayer);

	//TODO : it feels like the restrictions here are a bit arbitrary.
	//I would argue that it should be possible to not have an object layer at the root, i.e. everything lives in folders.
	//Another solution could be to create a blank layer if all are deleted.
	if (pLayer->GetParent() || pLayer->GetLayerType() != eObjectLayerType_Layer)
		return true;

	// one another root layer must exist.
	for (auto it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pCmpLayer = it->second;
		if (pCmpLayer != pLayer && !pCmpLayer->GetParent() && pCmpLayer->GetLayerType() == eObjectLayerType_Layer)
			return true;
	}

	return false;
}

void CObjectLayerManager::DeleteLayer(CObjectLayer* pLayer, bool bNotify /*= true*/)
{
	using namespace Private_ObjectLayerManager;
	assert(pLayer);

	if (!CanDeleteLayer(pLayer))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot delete layer %s", pLayer->GetName());
		return;
	}

	if (bNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_BEFORE_REMOVE, pLayer).Send();
	}

	// First delete all child layers.
	while (pLayer->GetChildCount())
	{
		DeleteLayer(pLayer->GetChild(0), false);
	}

	// prevent reference counted layer to be released before this function ends.
	TSmartPtr<CObjectLayer> pLayerHolder = pLayer;

	// Delete all objects for this layer.
	//std::vector<CBaseObjectPtr> objects;
	CBaseObjectsArray objects;
	m_pObjectManager->GetObjects(objects, pLayer);
	objects.erase(std::remove_if(objects.begin(), objects.end(), [](CBaseObject* pObject)
	{
		return pObject->CheckFlags(OBJFLAG_PREFAB);
	}), objects.end());

	m_pObjectManager->DeleteObjects(objects);

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoLayerCreateDelete(pLayer, false));


	// Insert layer path in list of files to be deleted on save
	m_toBeDeleted.insert(pLayer->GetLayerFilepath().c_str());

	bool bIsActive = pLayer == GetCurrentLayer();

	// remove child from parent after serialization for storing Parent GUID
	CObjectLayer* pParent = pLayer->GetParent();
	if (pParent)
		pParent->RemoveChild(pLayer, false);

	m_layersMap.erase(pLayer->GetGUID());
	m_layersCache.clear();

	if (bIsActive)
	{
		assert(!m_layersMap.empty());
		SetCurrentLayer(m_layersMap.begin()->second);
	}

	if (bNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_AFTER_REMOVE, pLayer).Send();
	}

	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_INVALIDATE);
}

bool CObjectLayerManager::CanRename(const CObjectLayer* pLayer, const char* szNewName) const
{
	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetParent() == pLayer->GetParent()
		    && layer.second->GetLayerType() == pLayer->GetLayerType()
		    && strcmp(layer.second->GetName(), szNewName) == 0)
		{
			return false;
		}
	}
	return true;
}

void CObjectLayerManager::SetLayerName(CObjectLayer* pLayer, const char* szNewName, bool IsUpdateDepends)
{
	const string& oldLayerName = pLayer->GetName();

	// Check if the layer had a valid name and add it's path to the list of layers to be deleted on save
	if (!oldLayerName.empty())
	{
		string oldFilePath = pLayer->GetLayerFilepath();
		m_toBeDeleted.insert(oldFilePath.c_str());
	}

	pLayer->SetNameImpl(szNewName, IsUpdateDepends);

	auto ite = m_toBeDeleted.find(pLayer->GetLayerFilepath().c_str());
	if (ite != m_toBeDeleted.end())
		m_toBeDeleted.erase(ite);
}

bool CObjectLayerManager::CanMove(const CObjectLayer* pLayer, const CObjectLayer* pTargetParent) const
{
	if (pLayer == pTargetParent || pLayer->GetParent() == pTargetParent || (pTargetParent && pTargetParent->IsChildOf(pLayer)))
		return false;

	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetParent() == pTargetParent
		    && layer.second->GetLayerType() == pLayer->GetLayerType()
		    && strcmp(layer.second->GetName(), pLayer->GetName()) == 0)
		{
			return false;
		}
	}
	return true;
}

CObjectLayer* CObjectLayerManager::FindLayer(CryGUID guid) const
{
	CObjectLayer* pLayer = stl::find_in_map(m_layersMap, guid, (CObjectLayer*)0);
	return pLayer;
}

CObjectLayer* CObjectLayerManager::FindLayerByFullName(const string& layerFullName) const
{
	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetLayerType() == eObjectLayerType_Layer && stricmp(layer.second->GetFullName(), layerFullName) == 0)
		{
			return layer.second;
		}
	}
	return nullptr;
}

CObjectLayer* CObjectLayerManager::FindLayerByName(const string& layerName) const
{
	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetLayerType() == eObjectLayerType_Layer && stricmp(layer.second->GetName(), layerName) == 0)
		{
			return layer.second;
		}
	}
	return nullptr;
}

CObjectLayer* CObjectLayerManager::FindFolderByName(const string& layerName) const
{
	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetLayerType() == eObjectLayerType_Folder && stricmp(layer.second->GetName(), layerName) == 0)
		{
			return layer.second;
		}
	}
	return nullptr;
}

CObjectLayer* CObjectLayerManager::FindAnyLayerByName(const string& layerName) const
{
	for (auto& layer : m_layersMap)
	{
		if (stricmp(layer.second->GetName(), layerName) == 0)
		{
			return layer.second;
		}
	}
	return nullptr;
}

CObjectLayer* CObjectLayerManager::FindLayerByNameAndType(const string& layerName, EObjectLayerType type) const
{
	for (auto& layer : m_layersMap)
	{
		if (layer.second->GetLayerType() == type && stricmp(layer.second->GetName(), layerName) == 0)
		{
			return layer.second;
		}
	}
	return nullptr;
}

bool CObjectLayerManager::IsAnyLayerOfType(EObjectLayerType type) const
{
	return std::any_of(m_layersMap.begin(), m_layersMap.end(), [](const auto& x)
	{
		return x.second->GetLayerType() == eObjectLayerType_Terrain;
	});
}


void CObjectLayerManager::NotifyLayerChange(const CLayerChangeEvent& event)
{
	signalChangeEvent(event);
}

CObjectLayer* CObjectLayerManager::CreateLayersFromPath(const string& fullPathName, const string& name, std::set<string>& createdLayers, bool bNotify /*=true*/)
{
	std::vector<string> segments = PathUtil::SplitIntoSegments(fullPathName.GetString());

	CObjectLayer* pPrevFolder = nullptr;
	CObjectLayer* pFolder = nullptr;

	bool warnDuplicateLayer = true;

	// test for duplicate folder first
	for (auto layer : m_layersMap)
	{
		if ((layer.second->GetLayerType() == eObjectLayerType_Folder) && (stricmp(layer.second->GetName(), name) == 0))
		{
			if (createdLayers.find(name) == createdLayers.end())
			{
				// we have an existing folder with the same name. This can happen when children get registered before parent
				// This is limitation due to Engine, but we only allow unique folder names here due to export not supporting
				// arbitrary filesystem names correctly
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,
				           "The layer %s contains child layers, and has been split into a stand-alone layer and a folder with the child layers. "
				           "It is strongly recommended that you rename the layer, or delete it if does not directly contain any objects.", name);

				// insert the name to created layers, so we don't warn again
				createdLayers.insert(name);
				break;
			}
		}
	}

	for (auto& segment : segments)
	{
		pFolder = nullptr;

		const char* szName = (const char*)segment;
		for (auto layer : m_layersMap)
		{
			if (stricmp(layer.second->GetName(), szName) == 0)
			{
				if (layer.second->GetLayerType() == eObjectLayerType_Folder)
				{
					pFolder = layer.second;
					break;
				}
				else if (warnDuplicateLayer)
				{
					if (createdLayers.find(layer.second->GetName()) == createdLayers.end())
					{
						// we have an existing layer which is not a folder. This can happen when children get registered after parent
						// This is limitation due to Engine, but we only allow unique folder names here due to export not supporting
						// arbitrary filesystem names correctly
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING,
						           "The layer %s contains child layers, and has been split into a stand-alone layer and a folder with the child layers. "
						           "It is strongly recommended that you rename the layer, or delete it if does not directly contain any objects.", szName);

						// insert the name to created layers, so we don't warn again
						createdLayers.insert(szName);
					}
					warnDuplicateLayer = false;
				}
			}
		}

		if (!pFolder)
		{
			pFolder = CreateLayer(szName, eObjectLayerType_Folder);
			if (!pFolder)
				return nullptr;
		}

		if (pPrevFolder)
		{
			pPrevFolder->AddChild(pFolder, false);
		}
		pPrevFolder = pFolder;
	}
	return pFolder;
}

void CObjectLayerManager::Serialize(CObjectArchive& ar)
{
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		ClearLayers();

		XmlNodeRef layers = xmlNode->findChild("ObjectLayers");
		if (layers)
		{
			int nVers = 0;
			layers->getAttr("LayerVersion", nVers);

			if (nVers >= 2)
			{
				std::vector<string> filenames;
				std::set<string> createdLayers;

				const string layersFolder = PathUtil::Make(GetIEditorImpl()->GetGameEngine()->GetLevelPath(), m_layersPath);

				if (layers->getChildCount())
				{
					for (int i = 0; i < layers->getChildCount(); i++)
					{
						XmlNodeRef layerNode = layers->getChild(i);

						bool bExternal = true;
						layerNode->getAttr("External", bExternal);
						if (bExternal)
						{
							string fullPathName;
							string name;
							layerNode->getAttr("FullName", fullPathName);
							filenames.push_back(fullPathName + LAYER_FILE_EXTENSION);
						}
						else
						{
							// Backwards compatibility with the "Main" layer being stored in the .cry file
							CObjectLayer* pLayer = new CObjectLayer();
							if (pLayer)
							{
								pLayer->SerializeBase(layerNode, true);
								pLayer->Serialize(layerNode, true);
								AddLayer(pLayer, false, true);
								pLayer->SetModified(true);
							}
						}
					}
				}
				else // Detect layers automatically based on file structure (Starting from 5.5.0 the xml layer collection is empty).
				{
					SDirectoryEnumeratorHelper enumerator;
					enumerator.ScanDirectoryRecursive(layersFolder, "", "*.lyr", filenames);
				}

				for (const string& filename : filenames)
				{
					const string filepath = PathUtil::Make(layersFolder, filename);
					m_bOverwriteDuplicates = true;
					CObjectLayer* pLayer = ImportLayerFromFile(filepath.c_str(), false, &ar);
					m_bOverwriteDuplicates = false;
					if (pLayer)
					{
						const string fullName = PathUtil::GetPathWithoutFilename(filename);
						const string name = PathUtil::GetFileName(filename);
						CObjectLayer* pParentLayer = CreateLayersFromPath(fullName, name, createdLayers, false);
						if (pParentLayer)
						{
							pParentLayer->AddChild(pLayer, false);
							pLayer->SetModified(false);
						}
					}
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Trying to load layers with unsupported format version %d", nVers);
				return;
			}

			CryGUID currentLayerGUID;
			if (layers->getAttr("CurrentGUID", currentLayerGUID))
			{
				CObjectLayer* pLayer = FindLayer(currentLayerGUID);
				if (pLayer)
					SetCurrentLayer(pLayer);
			}

			// We need an active layer always for the editor to work robustly!
			// Guard against this here, just in case user has messed up with layer folders
			// Usually this should never happen!
			if (!GetCurrentLayer())
			{
				for (auto layer : m_layersMap)
				{
					if (layer.second->GetLayerType() == eObjectLayerType_Layer)
					{
						SetCurrentLayer(layer.second);
						break;
					}
				}

				// if still no layer, fire up the alarms
				if (!GetCurrentLayer())
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Sandbox needs an active layer to work correctly");
				}
			}
		}

		ResolveLayerParents(false, false);
		CLayerChangeEvent(CLayerChangeEvent::LE_UPDATE_ALL).Send();
	}
	else
	{
		// Saving to archive.
		XmlNodeRef layersNode = xmlNode->newChild("ObjectLayers");
		int nVers = 2;
		layersNode->setAttr("LayerVersion", nVers);

		//! Store currently selected layer.
		if (m_pCurrentLayer)
		{
			layersNode->setAttr("Current", m_pCurrentLayer->GetName());
			layersNode->setAttr("CurrentGUID", m_pCurrentLayer->GetGUID());
		}

		CAutoCheckOutDialogEnableForAll enableForAll;
		for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
		{
			CObjectLayer* pLayer = it->second;
			if (pLayer->GetLayerType() == eObjectLayerType_Folder)
			{
				continue;
			}

			if (!gEditorGeneralPreferences.saveOnlyModified() || pLayer->IsModified() || !PathUtil::FileExists(pLayer->GetLayerFilepath()))
			{
				// Save external level to file.
				SaveLayer(&ar, pLayer);
			}
		}

		// Remove all stale layers that were renamed
		for (auto path : m_toBeDeleted)
		{
			PathUtil::Remove(path.c_str());

			// If it's an actual layer and not a folder, then also remove all .bak files
			if (strcmp(path.c_str() + path.size() - strlen(LAYER_FILE_EXTENSION), LAYER_FILE_EXTENSION) == 0)
			{
				PathUtil::Remove(PathUtil::ReplaceExtension(path.c_str(), "bak").c_str());
				PathUtil::Remove(PathUtil::ReplaceExtension(path.c_str(), "bak2").c_str());
			}
		}

		m_toBeDeleted.clear();
	}
	ar.node = xmlNode;
}

void CObjectLayerManager::SaveLayer(CObjectArchive* pArchive, CObjectLayer* pLayer)
{
	// Form file name from layer name.
	string path = PathUtil::Make(GetIEditorImpl()->GetGameEngine()->GetLevelPath(), m_layersPath);
	string file = pLayer->GetLayerFilepath();

	if (CFileUtil::OverwriteFile(file))
	{
		CFileUtil::CreateDirectory(path);

		// Make a backup of file.
		if (gEditorFilePreferences.filesBackup)
			CFileUtil::BackupFile(file);

		// Serialize this layer.
		XmlNodeRef rootFileNode = XmlHelpers::CreateXmlNode("ObjectLayer");
		XmlNodeRef layerNode = rootFileNode->newChild("Layer");

		CObjectArchive tempArchive(m_pObjectManager, layerNode, FALSE);

		if (pArchive)
			pArchive->node = layerNode;

		ExportLayer((pArchive ? *pArchive : tempArchive), pLayer, false); // Also save all childs but not external layers.
		// Save xml file to disk.
		XmlHelpers::SaveXmlNode(rootFileNode, file);
		pLayer->SetModified(false);
	}
}

void CObjectLayerManager::SaveLayer(CObjectLayer* pLayer)
{
	SaveLayer(nullptr, pLayer);
}

void CObjectLayerManager::ExportLayer(CObjectArchive& ar, CObjectLayer* pLayer, bool bExportExternalChilds)
{
	pLayer->SerializeBase(ar.node, false);
	pLayer->Serialize(ar.node, false);

	XmlNodeRef orgNode = ar.node;

	CBaseObjectsArray objects;
	m_pObjectManager->GetObjects(objects, pLayer);
	size_t numObjects = objects.size();

	if (numObjects)
	{
		XmlNodeRef layerObjects = orgNode->newChild("LayerObjects");
		ar.node = layerObjects;

		for (int i = 0; i < numObjects; ++i)
			ar.SaveObject(objects[i]);
	}
	ar.node = orgNode;
}

CObjectLayer* CObjectLayerManager::ImportLayer(CObjectArchive& ar, bool bNotify /*= true*/)
{
	TSmartPtr<CObjectLayer> pLayer(new CObjectLayer());

	XmlNodeRef layerNode = ar.node;
	
	pLayer->SerializeBase(layerNode, true);
	if (pLayer->GetLayerType() == eObjectLayerType_Terrain)
	{
		pLayer.reset(CObjectLayer::Create(pLayer->GetName(), pLayer->GetLayerType()));
		pLayer->SerializeBase(layerNode, true);

		// TODO: find a better way to sync up the terrain.
		pLayer->SetVisible(pLayer->IsVisible());
	}
	pLayer->Serialize(layerNode, true);

	CObjectLayer* pPrevLayer = FindLayer(pLayer->GetGUID());
	if (pPrevLayer)
	{
		if (m_bOverwriteDuplicates)
		{
			pLayer.reset(pPrevLayer);
			pLayer->Serialize(layerNode, true); // Serialize it again.
		}
		else
		{
			// Duplicate layer imported.
			QString question = QObject::tr("Replace Layer %1?\r\nAll object of replaced layer will be deleted.").arg(QtUtil::ToQString(pPrevLayer->GetName()));
			if (CQuestionDialog::SQuestion(QObject::tr("Confirm Replace Layer"), question) != QDialogButtonBox::Yes)
			{
				return nullptr;
			}
			DeleteLayer(pPrevLayer, bNotify);
		}
	}

	AddLayer(pLayer, bNotify);

	XmlNodeRef layerObjects = layerNode->findChild("LayerObjects");
	if (layerObjects)
	{
		int numObjects = layerObjects->getChildCount();

		TSmartPtr<CObjectLayer> pCurLayer = m_pCurrentLayer;
		m_pCurrentLayer = pLayer;

		ar.LoadObjects(layerObjects);
		m_pCurrentLayer = pCurLayer;
	}
	return pLayer;
}

CObjectLayer* CObjectLayerManager::ImportLayerFromFile(const char* filename, bool bNotify /*= true*/, CObjectArchive* globalArchive /* = nullptr */)
{
	if (CFileUtil::FileExists(filename))
	{
		XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
		if (root)
		{
			CObjectArchive localArchive(GetIEditorImpl()->GetObjectManager(), root, true);
			CObjectArchive* archive = (globalArchive) ? globalArchive : &localArchive;
			XmlNodeRef layerDesc = root->findChild("Layer");

			if (layerDesc)
			{
				XmlNodeRef prevNove = archive->node;
				archive->node = layerDesc;

				CObjectLayer* pLayer = ImportLayer(*archive, bNotify);
				if (pLayer)
				{

					uint32 attr = CFileUtil::GetAttributes(filename);
					if (!gEditorGeneralPreferences.freezeReadOnly() && attr != SCC_FILE_ATTRIBUTE_INVALID && (attr & SCC_FILE_ATTRIBUTE_READONLY))
					{
						pLayer->SetFrozen(true);
					}

					// Only resolve if archive is local, else leave resolution to archive above
					// layers should also get resolved above
					if (!globalArchive)
					{
						ResolveLayerParents(false, false);
						archive->ResolveObjects();
					}
				}
				archive->node = prevNove;
				return pLayer;
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Failed to import layer file '%s'", filename);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Layer file '%s' not found", filename);
	}

	return nullptr;
}

void CObjectLayerManager::SetCurrentLayer(CObjectLayer* pCurrLayer)
{
	using namespace Private_ObjectLayerManager;
	assert(pCurrLayer);
	if (pCurrLayer == m_pCurrentLayer)
		return;

	if (m_pCurrentLayer != nullptr)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoCurrentLayer(m_pCurrentLayer));
	}

	m_pCurrentLayer = pCurrLayer;
	CLayerChangeEvent(CLayerChangeEvent::LE_SELECT, m_pCurrentLayer).Send();
}

CObjectLayer* CObjectLayerManager::GetCurrentLayer() const
{
	if (m_pCurrentLayer)
	{
		if (m_pCurrentLayer->GetLayerType() == eObjectLayerType_Layer)
		{
			return m_pCurrentLayer;
		}
	}
	return nullptr;
}

void CObjectLayerManager::ResolveLayerParents(bool bNotifyAtomic /*= false*/, bool bNotifyUpdateAll /*= true*/)
{
	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pLayer = it->second;

		// Try to connect to parent layer.
		CObjectLayer* pNewParent = FindLayer(pLayer->GetParentGUID());

		if (pLayer->GetParent() != NULL && pLayer->GetParent() != pNewParent)
		{
			// Deatch from old parent layer.
			pLayer->GetParent()->RemoveChild(pLayer, bNotifyAtomic);
		}
		if (pNewParent)
		{
			// Attach to new parent layer.
			pNewParent->AddChild(pLayer, bNotifyAtomic);
		}
	}

	if (bNotifyUpdateAll)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_UPDATE_ALL).Send();
	}
}

bool CObjectLayerManager::InitLayerSwitches(bool isOnlyClear)
{
	if (isOnlyClear && !m_layerSwitches.empty())
	{
		std::vector<CBaseObjectPtr> objects;
		m_pObjectManager->GetAllObjects(objects);

		for (std::vector<CObjectLayer*>::iterator it = m_layerSwitches.begin(); it != m_layerSwitches.end(); ++it)
		{
			CObjectLayer* pLayer = (*it);

			if (!pLayer->GetLayerID())
				continue;

			if (gEnv->pEntitySystem)
				gEnv->pEntitySystem->EnableLayer(pLayer->GetName(), true);

			for (int k = 0; k < objects.size(); k++)
			{
				CBaseObject* pObj = objects[k];
				if (pObj->GetLayer() == pLayer)
				{
					if (pObj->IsKindOf(RUNTIME_CLASS(CSplineObject)))
					{
						CSplineObject* pSpline = (CSplineObject*)pObj;
						pSpline->SetLayerId(0);
						if (!pLayer->HasPhysics())
							pSpline->SetPhysics(true); // restore init state
					}
					else if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !((CEntityObject*)pObj)->GetIEntity())
					{
						IRenderNode* pRenderNode = pObj->GetEngineNode();
						if (pRenderNode)
						{
							pRenderNode->SetLayerId(0);
							pRenderNode->SetRndFlags(pRenderNode->GetRndFlags() & ~ERF_NO_PHYSICS);
						}
					}
				}
			}
			pLayer->SetLayerID(0);
		}
	}

	m_layerSwitches.clear();
	if (isOnlyClear)
		return true;

	uint16 curLayerId = 0;
	bool bRet = false;
	std::map<string, uint16> layerNameMap;
	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pLayer = it->second;
		std::map<string, uint16>::const_iterator it2 = layerNameMap.find(pLayer->GetName());
		if (it2 != layerNameMap.cend())
		{
			pLayer->SetLayerID(it2->second);
		}
		else
		{
			pLayer->SetLayerID(++curLayerId);
			layerNameMap.insert(std::make_pair(pLayer->GetName(), curLayerId));
		}
		m_layerSwitches.push_back(pLayer);
		bRet = true;
	}
	return bRet;
}

void CObjectLayerManager::ExportLayerSwitches(XmlNodeRef& node)
{
	if (m_layerSwitches.size() == 0)
		return;

	XmlNodeRef layersNode = node->newChild("Layers");
	std::set<string> seenNames;
	for (std::vector<CObjectLayer*>::iterator it = m_layerSwitches.begin(); it != m_layerSwitches.end(); ++it)
	{
		CObjectLayer* pLayer = *it;

		if (seenNames.find(pLayer->GetName()) != seenNames.end())
		{
			continue;
		}
		seenNames.insert(pLayer->GetName());
		XmlNodeRef layerNode = layersNode->newChild("Layer");
		layerNode->setAttr("Name", pLayer->GetName());
		layerNode->setAttr("Parent", pLayer->GetParent() ? pLayer->GetParent()->GetName() : "");
		layerNode->setAttr("Id", pLayer->GetLayerID());
		layerNode->setAttr("Specs", pLayer->GetSpecs());
		if (!pLayer->HasPhysics())
			layerNode->setAttr("Physics", 0);
		if (pLayer->IsDefaultLoaded())
			layerNode->setAttr("DefaultLoaded", 1);
	}
}

void CObjectLayerManager::AssignLayerIDsToRenderNodes()
{
	std::vector<CBaseObjectPtr> objects;
	m_pObjectManager->GetAllObjects(objects);

	for (int k = 0; k < objects.size(); k++)
	{
		CBaseObject* pObj = objects[k];
		CObjectLayer* pLayer = (CObjectLayer*)pObj->GetLayer();
		if (pLayer != NULL)
		{
			if (pObj->IsKindOf(RUNTIME_CLASS(CRoadObject)))
			{
				CRoadObject* pRoad = (CRoadObject*)pObj;
				pRoad->SetLayerId(pLayer->GetLayerID());
			}
			else if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !((CEntityObject*)pObj)->GetIEntity())
			{
				IRenderNode* pRenderNode = pObj->GetEngineNode();
				if (NULL != pRenderNode)
				{
					pRenderNode->SetLayerId(pLayer->GetLayerID());
				}
			}
		}
	}
}

void CObjectLayerManager::SetupLayerSwitches(bool isOnlyClear, bool isOnlyRenderNodes)
{
	if (!isOnlyRenderNodes)
	{
		if (!gEnv->pEntitySystem)
			return;

		gEnv->pEntitySystem->ClearLayers();
	}
	std::vector<CBaseObjectPtr> objects;
	m_pObjectManager->GetAllObjects(objects);

	if (!isOnlyRenderNodes && isOnlyClear)
	{
		// Show brashes in the end of game mode
		gEnv->p3DEngine->ActivateObjectsLayer(~0, true, true, true, true, "ALL_OBJECTS");

		// Update visibility for hidden brushes
		for (int k = 0; k < objects.size(); k++)
		{
			CBaseObject* pObj = objects[k];
			if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)) && pObj->IsHidden())
			{
				pObj->UpdateVisibility(true); // need for invalidation
				pObj->UpdateVisibility(false);
			}
		}
		return;
	}

	for (std::vector<CObjectLayer*>::iterator it = m_layerSwitches.begin(); it != m_layerSwitches.end(); ++it)
	{
		CObjectLayer* pLayer = (*it);

		for (int k = 0; k < objects.size(); k++)
		{
			CBaseObject* pObj = objects[k];

			// Skip hidden brushes in Game mode
			if (!isOnlyRenderNodes && (pObj->IsHidden() || pObj->IsHiddenBySpec()))
				continue;

			if (pObj->GetLayer() == pLayer)
			{
				if (pObj->IsKindOf(RUNTIME_CLASS(CSplineObject)))
				{
					CSplineObject* pSpline = (CSplineObject*)pObj;
					pSpline->SetLayerId(pLayer->GetLayerID());
					if (!pLayer->HasPhysics())
						pSpline->SetPhysics(false);
				}
				else if (!pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)) || !((CEntityObject*)pObj)->GetIEntity())
				{
					IRenderNode* pRenderNode = pObj->GetEngineNode();
					if (pRenderNode)
					{
						pRenderNode->SetLayerId(pLayer->GetLayerID());
						if (!pLayer->HasPhysics())
							pRenderNode->SetRndFlags(pRenderNode->GetRndFlags() | ERF_NO_PHYSICS);
					}
				}
			}
		}

		if (!isOnlyRenderNodes)
		{
			gEnv->pEntitySystem->AddLayer(pLayer->GetName(), pLayer->GetParent() ? pLayer->GetParent()->GetName() : "", pLayer->GetLayerID(), pLayer->HasPhysics(), pLayer->GetSpecs(), pLayer->IsDefaultLoaded());

			for (int k = 0; k < objects.size(); k++)
			{
				CBaseObject* pObj = objects[k];
				if (pObj->GetLayer() == pLayer)
				{
					if (pObj->IsKindOf(RUNTIME_CLASS(CEntityObject)) && ((CEntityObject*)pObj)->GetIEntity())
					{
						if (IRenderNode* node = pObj->GetEngineNode())
						{
							node->SetLayerId(pLayer->GetLayerID());
						}

						gEnv->pEntitySystem->AddEntityToLayer(pLayer->GetName(), ((CEntityObject*)pObj)->GetEntityId());
					}
				}
			}

			gEnv->pEntitySystem->EnableLayer(pLayer->GetName(), false);
		}
	}
	// make sure parent - child relation is valid in editor
	gEnv->pEntitySystem->LinkLayerChildren();

	// Hide brashes when Game mode is started
	if (!isOnlyRenderNodes)
	{
		gEnv->p3DEngine->ActivateObjectsLayer(~0, false, true, true, true, "ALL_OBJECTS");
		gEnv->pEntitySystem->EnableDefaultLayers();
	}
}

void CObjectLayerManager::SetGameMode(bool inGame)
{
	if (InitLayerSwitches(!inGame))
		SetupLayerSwitches(!inGame);
}

void CObjectLayerManager::FreezeROnly()
{
	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pLayer = it->second;
		if (!pLayer->IsFrozen())
		{
			string file = pLayer->GetLayerFilepath();
			uint32 attr = CFileUtil::GetAttributes(file);
			if (attr != SCC_FILE_ATTRIBUTE_INVALID && (attr & SCC_FILE_ATTRIBUTE_READONLY))
			{
				pLayer->SetFrozen(true);
			}
		}
	}
}
void CObjectLayerManager::ToggleFreezeAllBut(CObjectLayer* pLayer)
{
	std::unordered_set<CObjectLayer*> layerHierarchy;
	layerHierarchy.insert(pLayer);
	pLayer->SetVisible(true);
	pLayer->SetFrozen(false);

	CObjectLayer* pParent = pLayer->GetParent();
	while (pParent)
	{
		layerHierarchy.insert(pParent);
		pParent->SetFrozen(false);
		pParent = pParent->GetParent();
	}

	bool freezeAll = false;
	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pObjectLayer = it->second;
		if (layerHierarchy.find(pObjectLayer) == layerHierarchy.end() && pObjectLayer->GetLayerType() != eObjectLayerType_Terrain)
		{
			if (!pObjectLayer->IsFrozen())
			{
				freezeAll = true;
				break;
			}
		}
	}

	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pObjectLayer = it->second;
		if (layerHierarchy.find(pObjectLayer) == layerHierarchy.end() && !pObjectLayer->IsChildOf(pLayer)
			&& pObjectLayer->GetLayerType() != eObjectLayerType_Terrain)
		{
			pObjectLayer->SetFrozen(freezeAll);
		}
	}
}

void CObjectLayerManager::ToggleHideAllBut(CObjectLayer* pLayer)
{
	std::unordered_set<CObjectLayer*> layerHierarchy;
	layerHierarchy.insert(pLayer);
	pLayer->SetVisible(true);

	CObjectLayer* pParent = pLayer->GetParent();
	while (pParent)
	{
		layerHierarchy.insert(pParent);
		pParent->SetVisible(true);
		pParent = pParent->GetParent();
	}

	bool hideAll = false;
	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pObjectLayer = it->second;
		if (layerHierarchy.find(pObjectLayer) == layerHierarchy.end() && pObjectLayer->GetLayerType() != eObjectLayerType_Terrain)
		{
			if (pObjectLayer->IsVisible())
			{
				hideAll = true;
				break;
			}
		}
	}

	for (LayersMap::const_iterator it = m_layersMap.begin(); it != m_layersMap.end(); ++it)
	{
		CObjectLayer* pObjectLayer = it->second;
		if (layerHierarchy.find(pObjectLayer) == layerHierarchy.end() && !pObjectLayer->IsChildOf(pLayer)
			&& pObjectLayer->GetLayerType() != eObjectLayerType_Terrain)
		{
			pObjectLayer->SetVisible(!hideAll);
		}
	}
}

void CObjectLayerManager::SetAllFrozen(bool bFrozen)
{
	for (auto& it : m_layersMap)
	{
		it.second->SetFrozen(bFrozen);
	}
}

void CObjectLayerManager::SetAllVisible(bool bVisible)
{
	for (auto& it : m_layersMap)
	{
		it.second->SetVisible(bVisible);
	}
}

bool CObjectLayerManager::ReloadLayer(CObjectLayer* pLayer)
{
	if (pLayer)
	{
		string path = pLayer->GetLayerFilepath();
		CObjectLayer* pParentLayer = pLayer->GetParent();

		if (!CFileUtil::FileExists(path))
		{
			// Avoid deleting the layer if there is nothing to replace it with
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Layer file cannot be found - maybe layer was renamed?");
			return false;
		}

		DeleteLayer(pLayer);

		CObjectLayer* pNewLayer = ImportLayerFromFile(path);
		if (!pNewLayer)
			return false;

		if (pParentLayer)
			pParentLayer->AddChild(pNewLayer);

		SetCurrentLayer(pNewLayer);

		return true;
	}
	return false;
}

static bool FindDynTexSource(IMaterial* pMtl)
{
	bool foundDynTexSrc = false;
	if (pMtl)
	{
		const SShaderItem& shaderItem = pMtl->GetShaderItem();
		if (shaderItem.m_pShaderResources)
		{
			for (int texSlot = 0; texSlot < EFTT_MAX; ++texSlot)
			{
				SEfResTexture* pTex = shaderItem.m_pShaderResources->GetTexture(texSlot);
				if (pTex)
				{
					const char* pExt = PathUtil::GetExt(pTex->m_Name.c_str());
					foundDynTexSrc = pExt && (!stricmp(pExt, "swf") || !stricmp(pExt, "gfx") || !stricmp(pExt, "usm"));
					if (foundDynTexSrc)
						break;
				}
			}
		}
	}
	return foundDynTexSrc;
}

XmlNodeRef CObjectLayerManager::GenerateDynTexSrcLayerInfo() const
{
	struct PerMtlSharedObjList
	{
		PerMtlSharedObjList() : list(), crossLayerSharingDetected(false) {}

		std::vector<CBaseObject*> list;
		bool                      crossLayerSharingDetected;
	};
	typedef std::map<string, PerMtlSharedObjList> MtlSharingMap; // mtl name -> list of objs sharing said mtl

	MtlSharingMap mtlSharingMap;

	typedef std::set<string> IgnoredMtlSet;

	IgnoredMtlSet ignoredMtlSet;

	std::vector<CBaseObjectPtr> objects;
	m_pObjectManager->GetAllObjects(objects);

	for (size_t i = 0; i < objects.size(); ++i)
	{
		CBaseObject* pObj = objects[i];
		if (pObj)
		{
			CMaterial* pMat = (CMaterial*)pObj->GetRenderMaterial();
			if (pMat)
			{
				const char* pMtlName = pMat->GetName();
				assert(pMtlName);

				bool foundDynTexSrc = false;

				const int numSubMtls = pMat->GetSubMaterialCount();
				if (!numSubMtls)
				{
					if (pMat->LayerActivationAllowed())
					{
						IMaterial* pEngMat = pMat->GetMatInfo();
						foundDynTexSrc = FindDynTexSource(pEngMat);
					}
					else
					{
						ignoredMtlSet.insert(IgnoredMtlSet::value_type(pMtlName));
					}
				}
				else
				{
					for (int subMtlIdx = 0; subMtlIdx < numSubMtls; ++subMtlIdx)
					{
						CMaterial* pSubMat = pMat->GetSubMaterial(subMtlIdx);
						if (pSubMat)
						{
							if (pSubMat->LayerActivationAllowed())
							{
								if (!foundDynTexSrc)
								{
									IMaterial* pEngMat = pSubMat->GetMatInfo();
									foundDynTexSrc = FindDynTexSource(pEngMat);
								}
							}
							else
							{
								ignoredMtlSet.insert(IgnoredMtlSet::value_type(pMtlName));
								foundDynTexSrc = false;
								break;
							}
						}
					}
				}

				if (foundDynTexSrc)
				{
					CObjectLayer* pLayer = (CObjectLayer*)pObj->GetLayer();

					const char* pLayerName = pLayer ? pLayer->GetName() : 0;
					const char* pObjName = pObj->GetName();
					const char* pMtlName = pMat->GetName();

					assert(pLayerName);
					assert(pObjName);
					assert(pMtlName);

					MtlSharingMap::iterator itMtl = mtlSharingMap.find(pMtlName);
					if (itMtl != mtlSharingMap.end())
					{
						PerMtlSharedObjList& sharedObjList = (*itMtl).second;
						if (!sharedObjList.crossLayerSharingDetected)
						{
							assert(!sharedObjList.list.empty());
							assert(sharedObjList.list.back());

							CObjectLayer* pLayerCmp = (CObjectLayer*)sharedObjList.list.back()->GetLayer();
							const char* pLayerNameCmp = pLayerCmp ? pLayerCmp->GetName() : 0;

							assert(pLayerNameCmp);

							sharedObjList.crossLayerSharingDetected = 0 != strcmp(pLayerName, pLayerNameCmp);
						}

						sharedObjList.list.push_back(pObj);
					}
					else
					{
						PerMtlSharedObjList sharedObjList;
						sharedObjList.list.push_back(pObj);
						mtlSharingMap.insert(MtlSharingMap::value_type(pMtlName, sharedObjList));
					}
				}
			}
		}
	}

	typedef std::vector<string>               PerLayerMtlList;
	typedef std::map<string, PerLayerMtlList> LayerMap; // layer name -> list of mtls with dyn tex sources

	LayerMap layerMap;

	for (MtlSharingMap::const_iterator it = mtlSharingMap.begin(); it != mtlSharingMap.end(); ++it)
	{
		const char* pMaterialName = (*it).first;
		const PerMtlSharedObjList& sharedObjList = (*it).second;

		assert(pMaterialName);

		if (!sharedObjList.crossLayerSharingDetected)
		{
			assert(!sharedObjList.list.empty());
			assert(sharedObjList.list[0]);

			CObjectLayer* pLayer = (CObjectLayer*)sharedObjList.list[0]->GetLayer();
			const char* pLayerName = pLayer ? pLayer->GetName() : 0;

			assert(pLayerName);

			layerMap[pLayerName].push_back(pMaterialName);
		}
		else
		{
			const std::vector<CBaseObject*>& list = sharedObjList.list;
			for (size_t i = 0; i < list.size(); ++i)
			{
				CBaseObject* pObj = list[i];
				assert(pObj);

				const char* pObjName = pObj->GetName();
				assert(pObjName);

				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cross layer sharing of Flash material \"%s\" detected for \"%s\". %s"
				                                                       "Associated dynamic texture source(s) will not partake in layer activation!", pMaterialName, pObjName,
				           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", pObjName));
			}
		}
	}

	for (IgnoredMtlSet::const_iterator it = ignoredMtlSet.begin(); it != ignoredMtlSet.end(); ++it)
	{
		const char* pMtlName = (*it);
		assert(pMtlName);

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "Ignored Flash material \"%s\" for layer activation.", pMtlName);
	}

	XmlNodeRef root = XmlHelpers::CreateXmlNode("Layers");

	for (LayerMap::const_iterator it = layerMap.begin(); it != layerMap.end(); ++it)
	{
		const PerLayerMtlList& perLayerMtlList = (*it).second;
		if (!perLayerMtlList.empty())
		{
			const char* pLayerName = (*it).first;
			assert(pLayerName);

			XmlNodeRef layerNode = root->createNode("Layer");
			layerNode->setAttr("Name", pLayerName);

			for (size_t i = 0; i < perLayerMtlList.size(); ++i)
			{
				XmlNodeRef mtlNode = layerNode->createNode("Material");
				mtlNode->setAttr("Name", perLayerMtlList[i].c_str());
				layerNode->addChild(mtlNode);
			}

			root->addChild(layerNode);
		}
	}

	return root;
}

