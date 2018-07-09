// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "LevelModelsManager.h"

#include "ProxyModels/MergingProxyModel.h"
#include "ProxyModels/MountingProxyModel.h"
#include "ProxyModels/ItemModelAttribute.h"

#include "Objects/ObjectLayerManager.h"

namespace CLevelModelsManager_Private
{
namespace
{

enum EFullLevelColumns
{
	eFullLevelColumns_LayerColor,
	eFullLevelColumns_Visible, //Must be kept index 0 to match
	eFullLevelColumns_Frozen,  //Must be kept index 1 to match
	eFullLevelColumns_Name,    //Must be kept index 2 to match
	eFullLevelColumns_Layer,
	eFullLevelColumns_Type,
	eFullLevelColumns_TypeDesc,
	eFullLevelColumns_DefaultMaterial,
	eFullLevelColumns_CustomMaterial,
	eFullLevelColumns_Breakability,
	eFullLevelColumns_SmartObject,
	eFullLevelColumns_FlowGraph,
	eFullLevelColumns_Geometry,
	eFullLevelColumns_InstanceCount,
	eFullLevelColumns_LODCount,
	eFullLevelColumns_MinSpec,
	eFullLevelColumns_AI_GroupId,
	eFullLevelColumns_Exportable,
	eFullLevelColumns_ExportablePak,
	eFullLevelColumns_LoadedByDefault,
	eFullLevelColumns_HasPhysics,
	eFullLevelColumns_Platform,
	eFullLevelColumns_Size
};

static CItemModelAttribute* FullLevel_GetColumnAttribute(int column)
{
	switch (column)
	{
	case eFullLevelColumns_LayerColor:
		return &LevelModelsAttributes::s_LayerColorAttribute;
	case eFullLevelColumns_Name:
		return &Attributes::s_nameAttribute;
	case eFullLevelColumns_Layer:
		return &LevelModelsAttributes::s_layerNameAttribute;
	case eFullLevelColumns_Type:
		return &LevelModelsAttributes::s_objectTypeAttribute;
	case eFullLevelColumns_TypeDesc:
		return &LevelModelsAttributes::s_objectTypeDescAttribute;
	case eFullLevelColumns_Visible:
		return &LevelModelsAttributes::s_visibleAttribute;
	case eFullLevelColumns_Frozen:
		return &LevelModelsAttributes::s_frozenAttribute;
	case eFullLevelColumns_DefaultMaterial:
		return &LevelModelsAttributes::s_defaultMaterialAttribute;
	case eFullLevelColumns_CustomMaterial:
		return &LevelModelsAttributes::s_customMaterialAttribute;
	case eFullLevelColumns_Breakability:
		return &LevelModelsAttributes::s_breakableAttribute;
	case eFullLevelColumns_SmartObject:
		return &LevelModelsAttributes::s_smartObjectAttribute;
	case eFullLevelColumns_FlowGraph:
		return &LevelModelsAttributes::s_flowGraphAttribute;
	case eFullLevelColumns_Geometry:
		return &LevelModelsAttributes::s_geometryAttribute;
	case eFullLevelColumns_InstanceCount:
		return &LevelModelsAttributes::s_geometryInstancesAttribute;
	case eFullLevelColumns_LODCount:
		return &LevelModelsAttributes::s_lodCountAttribute;
	case eFullLevelColumns_MinSpec:
		return &LevelModelsAttributes::s_specAttribute;
	case eFullLevelColumns_AI_GroupId:
		return &LevelModelsAttributes::s_aiGroupIdAttribute;
	case eFullLevelColumns_Exportable:
		return &LevelModelsAttributes::s_ExportableAttribute;
	case eFullLevelColumns_ExportablePak:
		return &LevelModelsAttributes::s_ExportablePakAttribute;
	case eFullLevelColumns_LoadedByDefault:
		return &LevelModelsAttributes::s_LoadedByDefaultAttribute;
	case eFullLevelColumns_HasPhysics:
		return &LevelModelsAttributes::s_HasPhysicsAttribute;
	case eFullLevelColumns_Platform:
		return &LevelModelsAttributes::s_PlatformAttribute;
	default:
		return nullptr;
	}
}

static QVariant FullLevel_GetHeaderData(int section, Qt::Orientation orientation, int role)
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	auto pAttribute = FullLevel_GetColumnAttribute(section);
	if (!pAttribute)
	{
		return QVariant();
	}
	if (role == Qt::DecorationRole)
	{
		if (section == eFullLevelColumns_Visible)
			return CryIcon("icons:General/Visibility_True.ico");
		if (section == eFullLevelColumns_Frozen)
			return CryIcon("icons:General/editable.ico");
	}
	if (role == Qt::DisplayRole)
	{
		//For Visible and Frozen we use Icons instead
		if (section == eFullLevelColumns_Visible || section == eFullLevelColumns_Frozen || section == eFullLevelColumns_LayerColor)
		{
			return QString("");
		}
		return pAttribute->GetName();
	}
	else if (role == Qt::ToolTipRole)
	{
			return pAttribute->GetName();
	}
	else if (role == Attributes::s_getAttributeRole)
	{
		return QVariant::fromValue(pAttribute);
	}
	else if (role == Attributes::s_attributeMenuPathRole)
	{
		switch (section)
		{
		case eFullLevelColumns_Visible:
		case eFullLevelColumns_Frozen:
		case eFullLevelColumns_Name:
		case eFullLevelColumns_LayerColor:
			return "";
		case eFullLevelColumns_Layer:
		case eFullLevelColumns_Type:
		case eFullLevelColumns_TypeDesc:
		case eFullLevelColumns_DefaultMaterial:
		case eFullLevelColumns_CustomMaterial:
		case eFullLevelColumns_Breakability:
		case eFullLevelColumns_SmartObject:
		case eFullLevelColumns_FlowGraph:
		case eFullLevelColumns_Geometry:
		case eFullLevelColumns_InstanceCount:
		case eFullLevelColumns_LODCount:
		case eFullLevelColumns_MinSpec:
		case eFullLevelColumns_AI_GroupId:
			return "Objects";
		default:
			return "Layers";
		}
	
	}
	return QVariant();
}

} // namespace
} // namespace CLevelModelsManager_Private

CLevelModelsManager& CLevelModelsManager::GetInstance()
{
	static CLevelModelsManager instance;
	return instance;
}

CLevelModel* CLevelModelsManager::GetLevelModel() const
{
	return m_levelModel;
}

CLevelLayerModel* CLevelModelsManager::GetLayerModel(CObjectLayer* layer) const
{
	//it is possible that the layer is requested before it is created, 
	//in this case attach to signalLayerModelsUpdated to request the model after creation.
	if (layer)
	{
		auto it = m_layerModels.find(layer);
		if (it != m_layerModels.end())
		{
			return m_layerModels.at(layer);
		}
	}

	return nullptr;
}

QAbstractItemModel* CLevelModelsManager::GetFullHierarchyModel() const
{
	return m_fullLevelModel;
}

QAbstractItemModel* CLevelModelsManager::GetAllObjectsListModel() const
{
	return m_allObjectsModel;
}

CLevelModelsManager::CLevelModelsManager()
	: QObject()
	, m_ignoreLayerNotify(false)
{
	GetIEditorImpl()->RegisterNotifyListener(this);

	m_levelModel = new CLevelModel(this);
	m_levelModel->signalBeforeLayerUpdateEvent.Connect(this, &CLevelModelsManager::OnLayerChange);

	m_allObjectsModel = new CMergingProxyModel(this);
	m_allObjectsModel->SetHeaderDataCallbacks(CLevelLayerModel::eObjectColumns_Size, &CLevelLayerModel::GetHeaderData, Attributes::s_getAttributeRole);
	m_allObjectsModel->SetDragCallback(&LevelModelsUtil::GetDragDropData);

	m_fullLevelModel = new CMountingProxyModel(WrapMemberFunction(this, &CLevelModelsManager::CreateLayerModelFromIndex));
	m_fullLevelModel->SetHeaderDataCallbacks(CLevelModelsManager_Private::eFullLevelColumns_Size, &CLevelModelsManager_Private::FullLevel_GetHeaderData, Attributes::s_getAttributeRole);
	m_fullLevelModel->SetSourceModel(m_levelModel);
	m_fullLevelModel->SetDragCallback(&LevelModelsUtil::GetDragDropData);

	GetIEditorImpl()->GetObjectManager()->signalBatchProcessStarted.Connect(this, &CLevelModelsManager::OnBatchProcessStarted);
	GetIEditorImpl()->GetObjectManager()->signalBatchProcessFinished.Connect(this, &CLevelModelsManager::OnBatchProcessFinished);
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->signalChangeEvent.Connect(this, &CLevelModelsManager::OnLayerUpdate);

	CreateLayerModels();
}

CLevelModelsManager::~CLevelModelsManager()
{
	GetIEditorImpl()->GetObjectManager()->signalBatchProcessStarted.DisconnectObject(this);
	GetIEditorImpl()->GetObjectManager()->signalBatchProcessFinished.DisconnectObject(this);
	GetIEditorImpl()->UnregisterNotifyListener(this);
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->signalChangeEvent.DisconnectObject(this);//TODO : this is not connected properly, why has this been made by a signal now ?
	DeleteLayerModels();
}

void CLevelModelsManager::DeleteLayerModels()
{
	auto it = m_layerModels.begin();
	for (; it != m_layerModels.end(); ++it)
	{
		// Since deleteLater will queue up this model's deletion which will be handled next frame, we need to 
		// make sure to disconnect the model so it doesn't handle events while in the process of dying. 
		// May it rest in peace.
		it->second->Disconnect();
		it->second->deleteLater();
	}
	m_layerModels.clear();
	m_allObjectsModel->UnmountAll();

	signalLayerModelsUpdated();
}

void CLevelModelsManager::CreateLayerModels()
{
	LOADING_TIME_PROFILE_SECTION;
	const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
	for (CObjectLayer* layer : layers)
	{
		if (layer->GetLayerType() != eObjectLayerType_Layer)
		{
			continue;
		}
		auto layerModel = new CLevelLayerModel(layer, this);
		m_layerModels[layer] = layerModel;
		m_allObjectsModel->MountAppend(layerModel);
	}

	signalLayerModelsUpdated();
}

QAbstractItemModel* CLevelModelsManager::CreateLayerModelFromIndex(const QModelIndex& sourceIndex)
{
	if (sourceIndex.model() != m_levelModel)
	{
		return nullptr;
	}
	auto layer = m_levelModel->LayerFromIndex(sourceIndex);
	if (layer->GetLayerType() != eObjectLayerType_Layer)
	{
		return nullptr;
	}
	else
	{
		return GetLayerModel(layer);
	}
}

void CLevelModelsManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	//eNotify_OnClearLevelContents : Not handled because it is always within other notifications
	//eNotify_OnBeginSceneOpen : Not handled because we are using BeginLoad/EndLoad events
	switch (event)
	{
	case eNotify_OnBeginLoad:
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneClose:
		m_ignoreLayerNotify = true;
		DeleteLayerModels();
		break;

	case eNotify_OnEndLoad:
	case eNotify_OnEndNewScene:
	case eNotify_OnEndSceneClose:
		m_ignoreLayerNotify = false;
		CreateLayerModels();
		break;

	default:
		break;
	}
}

void CLevelModelsManager::OnLayerChange(const CLayerChangeEvent& event)
{
	if (m_ignoreLayerNotify)
		return;

	switch (event.m_type)
	{
	case CLayerChangeEvent::LE_AFTER_ADD:
		{
			auto layerModel = new CLevelLayerModel(event.m_layer, this);
			m_layerModels[event.m_layer] = layerModel;
			m_allObjectsModel->MountAppend(layerModel);
		}
		break;
	case CLayerChangeEvent::LE_AFTER_REMOVE:
		{
			auto layerModel = m_layerModels[event.m_layer];
			m_allObjectsModel->Unmount(layerModel);
			layerModel->deleteLater();
			m_layerModels.erase(event.m_layer);
		}
		break;
	case CLayerChangeEvent::LE_UPDATE_ALL:
		DeleteLayerModels();
		CreateLayerModels();
		break;
	default:
		//do nothing
		break;
	}
}

void CLevelModelsManager::OnBatchProcessStarted(const std::vector<CBaseObject*>& objects)
{
	using namespace Private_ObjectLayerManager;
	++m_batchProcessStackIndex;
	if (m_batchProcessStackIndex != 1)
	{
		return;
	}

	std::vector<CObjectLayer*> layers = GetIEditorImpl()->GetObjectManager()->GetUniqueLayersRelatedToObjects(objects);
	for (auto pLayer : layers)
	{
		auto pLayerModel = GetLayerModel(pLayer);
		if (pLayerModel)
		{
			m_layerModelsInvolvedInBatching.push_back(pLayerModel);
			pLayerModel->StartBatchProcess();
		}
	}
}

void CLevelModelsManager::OnBatchProcessFinished()
{
	--m_batchProcessStackIndex;
	assert(m_batchProcessStackIndex >= 0);
	if (m_batchProcessStackIndex != 0)
	{
		return;
	}
	for (auto pLayerModel : m_layerModelsInvolvedInBatching)
	{
		pLayerModel->FinishBatchProcess();
	}
	m_layerModelsInvolvedInBatching.clear();
}

void CLevelModelsManager::OnLayerUpdate(const CLayerChangeEvent& event)
{
	if (event.m_type == CLayerChangeEvent::LE_BEFORE_REMOVE)
	{
		if (auto pModel = GetLayerModel(event.m_layer))
		{
			pModel->Disconnect();
		}
		DisconnectChildrenModels(event.m_layer);
	}
}

void CLevelModelsManager::DisconnectChildrenModels(CObjectLayer* pLayer)
{
	for (int i = 0; i < pLayer->GetChildCount(); ++i)
	{
		auto pChildLayer = pLayer->GetChild(i);
		if (auto pModel = GetLayerModel(pChildLayer))
		{
			pModel->Disconnect();
		}
		DisconnectChildrenModels(pChildLayer);
	}
}

