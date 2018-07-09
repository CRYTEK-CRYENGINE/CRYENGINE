// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameEngine.h"
#include "HyperGraph/FlowGraphManager.h"
#include "ObjectLayerManager.h"
#include "ObjectLayer.h"
#include "Terrain/TerrainManager.h"
#include "CryString/CryPath.h"

//////////////////////////////////////////////////////////////////////////
// class CUndoLayerStates
class CUndoLayerStates : public IUndoObject
{
public:
	CUndoLayerStates(CObjectLayer* pLayer)
	{
		m_undoIsVisible = pLayer->IsVisible();
		m_undoIsFrozen = pLayer->IsFrozen();
		m_layerGUID = pLayer->GetGUID();
	}
protected:
	virtual const char* GetDescription() { return "Set Layer State"; }

	virtual void        Undo(bool bUndo)
	{
		CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayer(m_layerGUID);
		if (pLayer)
		{
			if (bUndo)
			{
				m_redoIsVisible = pLayer->IsVisible();
				m_redoIsFrozen = pLayer->IsFrozen();
			}

			pLayer->SetVisible(m_undoIsVisible);
			pLayer->SetFrozen(m_undoIsFrozen);
		}
	}

	virtual void Redo()
	{
		CObjectLayer* pLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->FindLayer(m_layerGUID);
		if (pLayer)
		{
			pLayer->SetVisible(m_redoIsVisible);
			pLayer->SetFrozen(m_redoIsFrozen);
		}
	}

private:
	bool   m_undoIsVisible;
	bool   m_redoIsVisible;
	bool   m_undoIsFrozen;
	bool   m_redoIsFrozen;
	CryGUID m_layerGUID;
};

//////////////////////////////////////////////////////////////////////////
// class CUndoLayerName
class CUndoLayerName : public IUndoObject
{
public:
	CUndoLayerName(CObjectLayer* pLayer)
	{
		m_undoName = pLayer->GetName();
		m_layerGUID = pLayer->GetGUID();
	}
protected:
	virtual const char* GetDescription() { return "Change Layer Name"; }

	virtual void        Undo(bool bUndo)
	{
		CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		CObjectLayer* pLayer = pLayerManager->FindLayer(m_layerGUID);
		if (pLayer)
		{
			if (bUndo)
				m_redoName = pLayer->GetName();

			pLayer->SetName(m_undoName);
			CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, pLayer).Send();
		}
	}

	virtual void Redo()
	{
		CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		CObjectLayer* pLayer = pLayerManager->FindLayer(m_layerGUID);
		if (pLayer)
		{
			pLayer->SetName(m_redoName);
			CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, pLayer).Send();
		}
	}

private:
	string m_undoName;
	string m_redoName;
	CryGUID m_layerGUID;
};

class CUndoLayerReparent : public IUndoObject
{
public:
	CUndoLayerReparent(CObjectLayer* pChild, CObjectLayer* pParent)
	{
		if (pParent)
		{
			m_parentGUID = pParent->GetGUID();
		}

		CRY_ASSERT(pChild);
		m_childGUID = pChild->GetGUID();
		CObjectLayer* pPrevParent = pChild->GetParent();
		if (pPrevParent)
		{
			m_prevParentGUID = pPrevParent->GetGUID();
		}

	}
protected:
	virtual const char* GetDescription() { return "Change Layer Name"; }

	virtual void        Undo(bool bUndo)
	{
		Reparent(m_childGUID, m_prevParentGUID);
	}

	virtual void Redo()
	{
		Reparent(m_childGUID, m_parentGUID);
	}

private:
	void Reparent(CryGUID childGUID, CryGUID newParentGUID)
	{
		CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
		CObjectLayer* pNewParent = pLayerManager->FindLayer(newParentGUID);
		CObjectLayer* pChild = pLayerManager->FindLayer(childGUID);
		CRY_ASSERT(pChild);

		if (pNewParent)
		{
			pNewParent->AddChild(pChild);
		}
		else
		{
			pChild->SetAsRootLayer();
		}
	}

private:
	CryGUID m_childGUID;
	CryGUID m_parentGUID;
	CryGUID m_prevParentGUID;
};

namespace Private_ObjectLayer
{

class CTerrain : public CObjectLayer
{
public:
	CTerrain::CTerrain(const char* szName)
		: CObjectLayer(szName, eObjectLayerType_Terrain)
	{
		const CTerrainManager* const pTerrainManager = GetIEditorImpl()->GetTerrainManager();
		const string dataFolder = GetIEditorImpl()->GetLevelDataFolder();
		const size_t n = pTerrainManager->GetDataFilesCount();
		m_files.reserve(n);
		for (size_t i = 0; i < n; ++i)
		{
			m_files.push_back(PathUtil::Make(dataFolder, pTerrainManager->GetDataFilename(i)));
		}
	}

	virtual void SetVisible(bool isVisible, bool isRecursive) override
	{
		static ICVar* pTerrain = GetISystem()->GetIConsole()->GetCVar("e_Terrain");
		if (!pTerrain)
		{
			return;
		}

		if (m_hidden == isVisible || (pTerrain->GetIVal() > 0) != isVisible)
		{
			if (CUndo::IsRecording())
			{
				CUndo::Record(new CUndoLayerStates(this));
			}

			m_hidden = !isVisible;
			pTerrain->Set(isVisible ? 1 : 0);

			CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, this).Send();

			// If turning visibility on in a child layer make sure the parent
			// is also made visible, or else this layer will remain hidden
			if (isVisible && m_parent && !m_parent->IsVisible())
			{
				m_parent->SetVisible(isVisible, false);
			}
		}
	}

	virtual void SetFrozen(bool isFrozen, bool isRecursive = false) override
	{
		// This is an empty implementation, because:
		// - the frozen state is not applicable to the terrain at the moment.
		// - the terrain has no child objects.

		// TODO:  Consider introducing the frozen state.
	}
};

}

//////////////////////////////////////////////////////////////////////////
// class CObjectLayer
CObjectLayer::CObjectLayer()
	: m_guid(CryGUID::Create())
	, m_hidden(false)
	, m_frozen(false)
	, m_parent(nullptr)
	, m_exportable(true)
	, m_exportLayerPak(true)
	, m_defaultLoaded(false)
	, m_expanded(false)
	, m_havePhysics(true)
	, m_isModified(true)
	, m_nLayerId(0)
	, m_isDefaultColor(true)
	, m_specs(eSpecType_All)
	, m_layerType(eObjectLayerType_Layer)
{
}

CObjectLayer::CObjectLayer(const char* szName, EObjectLayerType type)
	: m_guid(CryGUID::Create())
	, m_name(szName)
	, m_hidden(false)
	, m_frozen(false)
	, m_parent(nullptr)
	, m_exportable(true)
	, m_exportLayerPak(true)
	, m_defaultLoaded(false)
	, m_expanded(false)
	, m_havePhysics(true)
	, m_isModified(true)
	, m_nLayerId(0)
	, m_isDefaultColor(true)
	, m_specs(eSpecType_All)
	, m_layerType(type)
{
	m_color = GetSysColor(COLOR_BTNFACE);
	ZeroStruct(m_parentGUID);
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SerializeBase(XmlNodeRef& node, bool isLoading)
{
	if (isLoading)
	{
		int type;
		node->getAttr("Name", m_name);
		node->getAttr("GUID", m_guid);
		node->getAttr("Hidden", m_hidden);
		node->getAttr("Frozen", m_frozen);
		node->getAttr("Expanded", m_expanded);
		if (node->getAttr("Type", type)) // Check if it loaded for backwards compatibility
		{
			m_layerType = static_cast<EObjectLayerType>(type);
		}
	}
	else // save
	{
		node->setAttr("Name", m_name);
		node->setAttr("FullName", GetFullName());
		node->setAttr("GUID", m_guid);
		node->setAttr("Hidden", m_hidden);
		node->setAttr("Frozen", m_frozen);
		node->setAttr("Expanded", m_expanded);
		node->setAttr("Type", static_cast<int>(m_layerType));
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::Serialize(XmlNodeRef& node, bool isLoading)
{
	if (isLoading)
	{
		// Loading.
		m_specs = eSpecType_All;
		node->getAttr("Name", m_name);
		node->getAttr("GUID", m_guid);
		node->getAttr("Exportable", m_exportable);
		node->getAttr("ExportLayerPak", m_exportLayerPak);
		node->getAttr("DefaultLoaded", m_defaultLoaded);
		node->getAttr("HavePhysics", m_havePhysics);
		node->getAttr("Specs", m_specs);

		m_isDefaultColor = false;
		node->getAttr("IsDefaultColor", m_isDefaultColor);

		m_color = GetSysColor(COLOR_BTNFACE);
		if (!m_isDefaultColor)
			node->getAttr("Color", m_color);
		SetColor(m_color); // Update default state

		ZeroStruct(m_parentGUID);
		node->getAttr("ParentGUID", m_parentGUID);

		GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();
	}
	else
	{
		// Saving.
		node->setAttr("Name", m_name);
		node->setAttr("GUID", m_guid);
		node->setAttr("Exportable", m_exportable);
		node->setAttr("ExportLayerPak", m_exportLayerPak);
		node->setAttr("DefaultLoaded", m_defaultLoaded);
		node->setAttr("HavePhysics", m_havePhysics);
		node->setAttr("IsDefaultColor", m_isDefaultColor);
		if (m_specs != eSpecType_All)
			node->setAttr("Specs", m_specs);
		if (!m_isDefaultColor)
			node->setAttr("Color", m_color);

		CryGUID parentGUID = m_parentGUID;
		if (parentGUID != CryGUID::Null())
			node->setAttr("ParentGUID", parentGUID);
	}
}

//////////////////////////////////////////////////////////////////////////
uint CObjectLayer::GetObjectCount() const
{
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetObjectManager()->GetObjects(objects, this);
	return int(objects.size());
}

CObjectLayer* CObjectLayer::Create(const char* szName, EObjectLayerType type)
{
	if (eObjectLayerType_Terrain == type)
	{
		return new Private_ObjectLayer::CTerrain(szName);
	}

	return new CObjectLayer(szName, type);
}

void CObjectLayer::ForEachLayerOf(const std::vector<CBaseObject*>& objects, std::function<void(CObjectLayer*, const std::vector<CBaseObject*>&)> func)
{
	if (objects.empty())
	{
		return;
	}

	auto pFirstLayer = static_cast<CObjectLayer*>(objects[0]->GetLayer());
	if (std::all_of(objects.cbegin(), objects.cend(), [pFirstLayer](const CBaseObject* pObject)
	{
		return pFirstLayer == pObject->GetLayer();
	}))
	{
		func(pFirstLayer, objects);
		return;
	}

	std::unordered_map<CObjectLayer*, std::vector<CBaseObject*>> layerToObjectsMap;
	for (auto pObj : objects)
	{
		auto pLayer = static_cast<CObjectLayer*>(pObj->GetLayer());
		assert(pLayer);
		layerToObjectsMap[pLayer].push_back(pObj);
	}

	for (auto& layerToObjectsPair : layerToObjectsMap)
	{
		func(layerToObjectsPair.first, layerToObjectsPair.second);
	}
}

void CObjectLayer::SetName(const string& name, bool IsUpdateDepends)
{
	// Forward call to layer manager because we need to track layer renames. When saving the level
	// layers will be created/removed from disk
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->SetLayerName(this, name, IsUpdateDepends);
	SetModified(true);
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SetNameImpl(const string& name, bool IsUpdateDepends)
{
	if (m_name == name)
		return;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoLayerName(this));

	string oldName = m_name;
	m_name = name;

	CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, this).Send();
	GetIEditorImpl()->GetFlowGraphManager()->UpdateLayerName(oldName.GetString(), name.GetString());
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::AddChild(CObjectLayer* pLayer, bool isNotify /*= true*/)
{
	assert(pLayer);
	if (IsChildOf(pLayer) || pLayer->IsChildOf(this))
		return;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoLayerReparent(pLayer, this));

	if (isNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_BEFORE_PARENT_CHANGE, pLayer, this /*new parent is sent*/).Send();
	}

	auto pOldParent = pLayer->GetParent();
	if (pOldParent)
	{
		pOldParent->RemoveChild(pLayer, false);
	}

	stl::push_back_unique(m_childLayers, pLayer);
	pLayer->m_parent = this;
	pLayer->m_parentGUID = GetGUID();
	GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

	if (isNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_AFTER_PARENT_CHANGE, pLayer, pOldParent /*old parent is sent*/).Send();
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::RemoveChild(CObjectLayer* pLayer, bool isNotify /*= true*/)
{
	if (isNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_BEFORE_PARENT_CHANGE, pLayer, nullptr /*new parent is sent*/).Send();
	}

	assert(pLayer);
	pLayer->m_parent = nullptr;
	ZeroStruct(pLayer->m_parentGUID);
	stl::find_and_erase(m_childLayers, pLayer);
	GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

	if (isNotify)
	{
		CLayerChangeEvent(CLayerChangeEvent::LE_AFTER_PARENT_CHANGE, pLayer, this /*old parent is sent*/).Send();
	}
}

void CObjectLayer::SetAsRootLayer()
{
	if (m_parent)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoLayerReparent(this, nullptr));

		m_parent->RemoveChild(this);
	}
}

//////////////////////////////////////////////////////////////////////////
CObjectLayer* CObjectLayer::GetChild(int index) const
{
	assert(index >= 0 && index < m_childLayers.size());
	return m_childLayers[index];
}

//////////////////////////////////////////////////////////////////////////
bool CObjectLayer::IsChildOf(const CObjectLayer* pParent) const
{
	if (m_parent == pParent)
		return true;

	if (m_parent)
		return m_parent->IsChildOf(pParent);

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SetVisible(bool isVisible, bool isRecursive)
{
	if (m_hidden != !isVisible)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoLayerStates(this));

		m_hidden = !isVisible;

		// Notify layer manager on layer modification.
		CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, this).Send();
		GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

		// If turning visibility on in a child layer make sure the parent
		// is also made visible, or else this layer will remain hidden
		if (isVisible && m_parent && !m_parent->IsVisible())
			m_parent->SetVisible(isVisible, false);
	}

	if (isRecursive)
	{
		for (int i = 0; i < GetChildCount(); i++)
		{
			GetChild(i)->SetVisible(isVisible, isRecursive);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SetFrozen(bool isFrozen, bool isRecursive)
{
	if (m_frozen != isFrozen)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoLayerStates(this));

		m_frozen = isFrozen;

		CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, this).Send();
		GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();

		// If unfreezing a child layer make sure the parent is also unfrozen
		if (!isFrozen && m_parent && m_parent->IsFrozen())
			m_parent->SetFrozen(isFrozen, false);
	}
	if (isRecursive)
	{
		for (int i = 0; i < GetChildCount(); i++)
		{
			GetChild(i)->SetFrozen(isFrozen, isRecursive);
		}
	}
	GetIEditorImpl()->GetFlowGraphManager()->SendNotifyEvent(EHG_GRAPH_UPDATE_FROZEN);
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::Expand(bool isExpand)
{
	m_expanded = isExpand;
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SetModified(bool isModified)
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();

	if (pLayerManager->CanModifyLayers() == false)
		return;

	if (m_isModified == isModified)
		return;
	m_isModified = isModified;

	CLayerChangeEvent(CLayerChangeEvent::LE_MODIFY, this).Send();
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
}

//////////////////////////////////////////////////////////////////////////
string CObjectLayer::GetLayerFilepath()
{
	CObjectLayerManager* pLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	string filePath = PathUtil::Make(GetIEditorImpl()->GetGameEngine()->GetLevelPath(), pLayerManager->GetLayersPath() + GetFullName()).c_str();
	if (m_layerType != eObjectLayerType_Folder)
		filePath += LAYER_FILE_EXTENSION;

	return filePath;
}

//////////////////////////////////////////////////////////////////////////
COLORREF CObjectLayer::GetColor() const
{
	if (m_isDefaultColor)
		return GetSysColor(COLOR_BTNFACE);
	return m_color;
}

//////////////////////////////////////////////////////////////////////////
void CObjectLayer::SetColor(COLORREF color)
{
	m_color = color;
	m_isDefaultColor = (color == GetSysColor(COLOR_BTNFACE));
}

//////////////////////////////////////////////////////////////////////////
string CObjectLayer::GetFullName() const
{
	string fullName(m_name);
	for (CObjectLayer* pParent = m_parent; pParent; pParent = pParent->m_parent)
		fullName = pParent->m_name + "/" + fullName;

	return fullName;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectLayer::IsVisible(bool isRecursive) const
{
	if (isRecursive && m_parent)
	{
		return !m_hidden& m_parent->IsVisible(isRecursive);
	}
	return !m_hidden;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectLayer::IsFrozen(bool isRecursive) const
{
	if (isRecursive && m_parent)
	{
		return m_frozen || m_parent->IsFrozen(isRecursive);
	}
	return m_frozen;
}

