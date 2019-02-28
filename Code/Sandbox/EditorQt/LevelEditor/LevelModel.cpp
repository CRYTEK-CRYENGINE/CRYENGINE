// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelModel.h"

#include <QtUtil.h>
#include <QMimeData>
#include <QApplication>
#include <QByteArray>

#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include <CryCore/ToolsHelpers/GuidUtil.h>

#include "CryIcon.h"
#include "QAdvancedItemDelegate.h"
#include "ProxyModels/ItemModelAttribute.h"
#include "LevelLayerModel.h"
#include "DragDrop.h"

//////////////////////////////////////////////////////////////////////////
class CBaseObject;
class CObjectLayer;
namespace LevelModelsUtil
{
void ProcessIndexList(const QModelIndexList& list, std::vector<CBaseObject*>& outObjects, std::vector<CObjectLayer*>& outLayers, std::vector<CObjectLayer*>& outLayerFolders)
{
	outObjects.reserve(list.count());
	outLayers.reserve(list.count());
	outLayerFolders.reserve(list.count());

	for (auto& index : list)
	{
		QVariant type = index.data((int)CLevelModel::Roles::TypeCheckRole);
		QVariant internalPtrVar = index.data((int)CLevelModel::Roles::InternalPointerRole);
		switch (type.toInt())
		{
		case ELevelElementType::eLevelElement_Layer:
			{
				CObjectLayer* pLayer = reinterpret_cast<CObjectLayer*>(internalPtrVar.value<intptr_t>());

				if (pLayer->GetLayerType() == eObjectLayerType_Layer)
				{
					outLayers.push_back(pLayer);
				}
				else if (pLayer->GetLayerType() == eObjectLayerType_Folder)
				{
					outLayerFolders.push_back(pLayer);
				}
			}
			break;
		case ELevelElementType::eLevelElement_Object:
			outObjects.push_back(reinterpret_cast<CBaseObject*>(internalPtrVar.value<intptr_t>()));
			break;
		default:
			break;
		}
	}
}

QModelIndexList FilterByColumn(const QModelIndexList& list, int column /*= 0*/)
{
	QModelIndexList filtered;
	filtered.reserve(list.size());
	for (auto& index : list)
	{
		if (index.column() == column)
		{
			filtered.append(index);
		}
	}

	return filtered;
}

QMimeData* GetDragDropData(const QModelIndexList& list)
{
	std::vector<CBaseObject*> objects;
	std::vector<CObjectLayer*> layers;
	std::vector<CObjectLayer*> folders;
	ProcessIndexList(FilterByColumn(list, list.first().column()), objects, layers, folders);
	layers.insert(layers.end(), folders.cbegin(), folders.cend());

	if (layers.empty() && objects.empty())
	{
		return nullptr;
	}
	CDragDropData* dragDropData = new CDragDropData();

	QByteArray byteArray;
	QDataStream stream(&byteArray, QIODevice::ReadWrite);

	if (auto size = layers.size())
	{
		stream << (int)eLevelElement_Layer;
		stream << (int)size;
		for (auto& layer : layers)
		{
			stream << reinterpret_cast<intptr_t>(layer);
		}
	}

	if (auto size = objects.size())
	{
		stream << (int)eLevelElement_Object;
		stream << (int)size;
		for (auto& obj : objects)
		{
			stream << reinterpret_cast<intptr_t>(obj);
		}
	}

	dragDropData->SetCustomData("LayersAndObjects", byteArray);
	return dragDropData;
}

bool ProcessDragDropData(const QMimeData* data, std::vector<CBaseObject*>& outObjects, std::vector<CObjectLayer*>& outLayers)
{
	const CDragDropData* dragDropData = CDragDropData::FromMimeData(data);
	if (dragDropData->HasCustomData("LayersAndObjects"))
	{
		//TODO: better encapsulation of drag data, too many internals are required right now
		QByteArray byteArray = dragDropData->GetCustomData("LayersAndObjects");
		QDataStream stream(byteArray);

		while (!stream.atEnd())
		{
			int type;
			stream >> type;

			if (type == eLevelElement_Layer)
			{
				int count;
				stream >> count;
				outLayers.clear();
				outLayers.reserve(count);
				for (int i = 0; i < count; i++)
				{
					intptr_t value;
					stream >> value;
					outLayers.push_back(reinterpret_cast<CObjectLayer*>(value));
				}
			}
			else if (type == eLevelElement_Object)
			{
				int count;
				stream >> count;
				outObjects.clear();
				outObjects.reserve(count);
				for (int i = 0; i < count; i++)
				{
					intptr_t value;
					stream >> value;
					outObjects.push_back(reinterpret_cast<CBaseObject*>(value));
				}
			}
			else
				assert(0);
		}
		return true;
	}
	return false;
}

}

//////////////////////////////////////////////////////////////////////////

namespace LevelModelsAttributes
{
CItemModelAttribute s_ExportableAttribute("Exportable", eAttributeType_Boolean, CItemModelAttribute::StartHidden);
CItemModelAttribute s_ExportablePakAttribute("Exportable to Pak", eAttributeType_Boolean, CItemModelAttribute::StartHidden);
CItemModelAttribute s_LoadedByDefaultAttribute("Loaded in Game", eAttributeType_Boolean, CItemModelAttribute::StartHidden);
CItemModelAttribute s_HasPhysicsAttribute("Has Physics", eAttributeType_Boolean, CItemModelAttribute::StartHidden);
CItemModelAttribute s_PlatformAttribute("Platform", eAttributeType_String, CItemModelAttribute::StartHidden);
}

//See IEntitySystem.h
QString CLevelModel::LayerSpecToString(int spec)
{
	QString str;

	if (spec & eSpecType_PC)
		str += "PC, ";

	if (spec & eSpecType_XBoxOne)
		str += "XBox One, ";

	if (spec & eSpecType_PS4)
		str += "PS4, ";

	if (str.endsWith(", "))
	{
		return str.left(str.size() - 2);
	}

	return str;
}

int CLevelModel::LayerSpecToInt(const QString& specStr)
{
	int spec;

	if (specStr.contains("PC"))
		spec = spec | eSpecType_PC;

	if (specStr.contains("XBox One"))
		spec = spec | eSpecType_XBoxOne;

	if (specStr.contains("PS4"))
		spec = spec | eSpecType_PS4;

	return spec;
}

//////////////////////////////////////////////////////////
CLevelModel::CLevelModel(QObject* parent /*= nullptr*/)
	: QAbstractItemModel(parent)
	, m_ignoreLayerUpdates(false)
{
	GetIEditorImpl()->RegisterNotifyListener(this);
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->signalChangeEvent.Connect(this, &CLevelModel::OnLayerUpdate);
}

//////////////////////////////////////////////////////////
CLevelModel::~CLevelModel()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->signalChangeEvent.DisconnectObject(this);
}

CItemModelAttribute* CLevelModel::GetColumnAttribute(int column) const
{
	switch (column)
	{
	case eLayerColumns_Name:
		return &Attributes::s_nameAttribute;
	case eLayerColumns_Visible:
		return &LevelModelsAttributes::s_visibleAttribute;
	case eLayerColumns_Frozen:
		return &LevelModelsAttributes::s_frozenAttribute;
	case eLayerColumns_Exportable:
		return &LevelModelsAttributes::s_ExportableAttribute;
	case eLayerColumns_ExportablePak:
		return &LevelModelsAttributes::s_ExportablePakAttribute;
	case eLayerColumns_LoadedByDefault:
		return &LevelModelsAttributes::s_LoadedByDefaultAttribute;
	case eLayerColumns_HasPhysics:
		return &LevelModelsAttributes::s_HasPhysicsAttribute;
	case eLayerColumns_Platform:
		return &LevelModelsAttributes::s_PlatformAttribute;
	case eLayerColumns_Color:
		return &LevelModelsAttributes::s_LayerColorAttribute;
	default:
		return nullptr;
	}
}

//////////////////////////////////////////////////////////
QVariant CLevelModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	if (role == (int)CLevelModel::Roles::TypeCheckRole)
	{
		return (int)eLevelElement_Layer;
	}

	CObjectLayer* pLayer = LayerFromIndex(index);
	if (pLayer)
	{
		if (role == (int)Roles::InternalPointerRole)
		{
			return reinterpret_cast<intptr_t>(pLayer);
		}

		switch (index.column())
		{
		case ELayerColumns::eLayerColumns_Name:
			switch (role)
			{
			case Qt::DisplayRole:
			case Qt::EditRole:
				return (const char*)pLayer->GetName();
			case Qt::ToolTipRole:
				{
					if (pLayer->GetLayerType() == eObjectLayerType_Layer)
					{
						if (GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetCurrentLayer() == pLayer)
							return QString("%1 (Active Layer)").arg((const char*)pLayer->GetName());
						else
							return (const char*)pLayer->GetName();
					}

				}
			case Qt::DecorationRole:
				if (pLayer->GetLayerType() == eObjectLayerType_Layer)
				{
					if (GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetCurrentLayer() == pLayer)
						return CryIcon("icons:General/layer_active.ico").pixmap(16, 16, QIcon::Normal, QIcon::On);
					else
						return CryIcon("icons:General/layer.ico").pixmap(16, 16);
				}
				else if (pLayer->GetLayerType() == eObjectLayerType_Folder)
				{
					//IsChildOf also checks for indirect ancestory (if it's a grandchild etc.)
					auto currentLayer = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetCurrentLayer();
					if(currentLayer && currentLayer->IsChildOf(pLayer))
						return CryIcon("icons:General/Folder_Tree_Active.ico").pixmap(16, 16);
					else
						return CryIcon("icons:General/Folder_Tree.ico").pixmap(16, 16);
				} 
				else if (pLayer->GetLayerType() == eObjectLayerType_Terrain)
				{
					return CryIcon("icons:Tools/tools_terrain-editor.ico").pixmap(16, 16);
				}
			}
			break;
		case ELayerColumns::eLayerColumns_Visible:
			switch (role)
			{
			case Qt::CheckStateRole:
				{
					CObjectLayer* pParent = pLayer->GetParent();
					if (pParent && !pParent->IsVisible())
					{
						return pLayer->IsVisible(false) ? Qt::PartiallyChecked : Qt::Unchecked;
					}
					return pLayer->IsVisible(false) ? Qt::Checked : Qt::Unchecked;
				}
			case QAdvancedItemDelegate::s_IconOverrideRole:
				if (pLayer->IsVisible(false))
					return CryIcon("icons:General/Visibility_True.ico");
				else
					return CryIcon("icons:General/Visibility_False.ico");
			default:
				break;
			}
		case ELayerColumns::eLayerColumns_Frozen:

			switch (role)
			{
			case Qt::CheckStateRole:
					return pLayer->IsFrozen(false) ? Qt::Checked : Qt::Unchecked;
			case QAdvancedItemDelegate::s_IconOverrideRole:
				if (pLayer->IsFrozen(false))
					return CryIcon("icons:General/editable_false.ico");
				else
					return CryIcon("icons:General/editable_true.ico");
			default:
				break;
			}
		case ELayerColumns::eLayerColumns_Exportable:
			switch (role)
			{
			case Qt::CheckStateRole:
				return pLayer->IsExportable() ? Qt::Checked : Qt::Unchecked;
			}
			break;
		case ELayerColumns::eLayerColumns_ExportablePak:
			switch (role)
			{
			case Qt::CheckStateRole:
				return pLayer->IsExporLayerPak() ? Qt::Checked : Qt::Unchecked;
			}
			break;
		case ELayerColumns::eLayerColumns_LoadedByDefault:
			switch (role)
			{
			case Qt::CheckStateRole:
				return pLayer->IsDefaultLoaded() ? Qt::Checked : Qt::Unchecked;
			}
			break;
		case ELayerColumns::eLayerColumns_HasPhysics:
			switch (role)
			{
			case Qt::CheckStateRole:
				return pLayer->HasPhysics() ? Qt::Checked : Qt::Unchecked;
			}
			break;

		case ELayerColumns::eLayerColumns_Platform:
			switch (role)
			{
			case Qt::DisplayRole:
			case Qt::EditRole:
				return LayerSpecToString(pLayer->GetSpecs());
			}
			break;
		case ELayerColumns::eLayerColumns_Color:
			switch (role)
			{
			case Qt::DecorationRole:
			{
				COLORREF colorRef = pLayer->GetColor();
				return QColor::fromRgb(GetRValue(colorRef), GetGValue(colorRef), GetBValue(colorRef));
			}
			case QAdvancedItemDelegate::s_DrawRectOverrideRole:
			{
				// Force the layer color to take the full height of the item in the tree
				QRect decorationRect;
				decorationRect.setX(-3);
				decorationRect.setY(-4);
				decorationRect.setWidth(4);
				decorationRect.setHeight(24);
				return decorationRect;
			}
			default:
				break;
			}
		}
	}
	return QVariant();
}

//////////////////////////////////////////////////////////
bool CLevelModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	CObjectLayer* pLayer = LayerFromIndex(index);
	if (pLayer)
	{
		switch (index.column())
		{
		case ELayerColumns::eLayerColumns_Name:
			if (role == Qt::EditRole)
			{
				string newName = QtUtil::ToString(value.toString());
				
				// Make sure the name actually changed before attempting to rename
				const char* szName = newName.c_str();
				if (szName == pLayer->GetName())
					return false;

				if (!IsNameValid(szName))
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Layer name '%s' is already in use", szName);
					return false;
				}

				GetIEditorImpl()->GetIUndoManager()->Begin();
				pLayer->SetName(szName);
				GetIEditorImpl()->GetIUndoManager()->Accept("Rename Layer");
			}
			break;
		case ELayerColumns::eLayerColumns_Visible:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetVisible(value == Qt::Checked, true); // recursive = true
			}
			break;
		case ELayerColumns::eLayerColumns_Frozen:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetFrozen(value == Qt::Checked, true); // recursive = true
			}
			break;
		case ELayerColumns::eLayerColumns_Exportable:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetExportable(value == Qt::Checked);
			}
			break;
		case ELayerColumns::eLayerColumns_ExportablePak:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetExportLayerPak(value == Qt::Checked);
			}
			break;
		case ELayerColumns::eLayerColumns_LoadedByDefault:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetDefaultLoaded(value == Qt::Checked);
			}
			break;
		case ELayerColumns::eLayerColumns_HasPhysics:
			if (role == Qt::CheckStateRole)
			{
				pLayer->SetHavePhysics(value == Qt::Checked);
			}
			break;
		case ELayerColumns::eLayerColumns_Platform:
			if (role == Qt::EditRole)
			{
				pLayer->SetSpecs(LayerSpecToInt(value.toString()));
			}
			break;
		}
		QVector<int> roleVector(1, role);
		dataChanged(index, index, roleVector);
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////
QVariant CLevelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	auto pAttribute = GetColumnAttribute(section);
	if (!pAttribute)
	{
		return QVariant();
	}

	if (role == Qt::DecorationRole)
	{
		if (section == eLayerColumns_Visible)
			return CryIcon("icons:General/Visibility_True.ico");
		if (section == eLayerColumns_Frozen)
			return CryIcon("icons:Navigation/Basics_Select_False.ico");
	}
	if (role == Qt::DisplayRole)
	{
		//For Visible, Frozen and Layer Color we don't return the name because we use Icons instead
		if (section != eLayerColumns_Visible && section != eLayerColumns_Frozen && section != eLayerColumns_Color)
		{
			return pAttribute->GetName();
		}
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
		case eLayerColumns_Name:
		case eLayerColumns_Visible:
		case eLayerColumns_Frozen:
			return "";
		default:
			return "Layers";
		}
	}
	return QVariant();
}

//////////////////////////////////////////////////////////
Qt::ItemFlags CLevelModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags f = QAbstractItemModel::flags(index);
	if (index.isValid())
	{
		switch (index.column())
		{
		case eLayerColumns_Name:
		case eLayerColumns_Platform:
			f |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
			break;
		case eLayerColumns_Visible:
		case eLayerColumns_Frozen:
			f |= Qt::ItemIsUserCheckable;
			break;
		case eLayerColumns_Exportable:
		case eLayerColumns_ExportablePak:
		case eLayerColumns_LoadedByDefault:
		case eLayerColumns_HasPhysics:
			f |= Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled;
			break;
		case eLayerColumns_Color:
		default:
			f |= Qt::ItemIsDragEnabled;
			break;
		}
	}
	return f | Qt::ItemIsDropEnabled;
}

//////////////////////////////////////////////////////////
QModelIndex CLevelModel::index(int row, int column, const QModelIndex& parent) const
{
	if ((row >= 0) && (column >= 0))
	{
		const CObjectLayer* pParentLayer = LayerFromIndex(parent);
		if (pParentLayer && pParentLayer->GetChildCount() > row)
		{
			CObjectLayer* pChild = pParentLayer->GetChild(row);
			if (pChild)
			{
				return createIndex(row, column, reinterpret_cast<quintptr>(pChild));
			}
		}
		else
		{
			// index for a top level layer
			int index = 0;
			const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
			for (CObjectLayer* pLayer : layers)
			{
				// since all the layers are stored together we need look for the ones
				// that have no parents as these are the top level layers
				if (pLayer->GetParent() == nullptr)
				{
					if (index == row)
					{
						return createIndex(row, column, reinterpret_cast<quintptr>(pLayer));
					}
					++index;
				}
			}
		}
	}
	return QModelIndex();
}

//////////////////////////////////////////////////////////
QModelIndex CLevelModel::parent(const QModelIndex& index) const
{
	if (index.isValid())
	{
		CObjectLayer* pLayer = LayerFromIndex(index);
		if (pLayer)
		{
			return IndexFromLayer(pLayer->GetParent());
		}
	}
	return QModelIndex();
}

Qt::DropActions CLevelModel::supportedDragActions() const
{
	return Qt::MoveAction;
}

//////////////////////////////////////////////////////////
Qt::DropActions CLevelModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList CLevelModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("LayersAndObjects");
	return result;
}

QMimeData* CLevelModel::mimeData(const QModelIndexList& indexes) const
{
	return LevelModelsUtil::GetDragDropData(indexes);
}

//////////////////////////////////////////////////////////
bool CLevelModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int, const QModelIndex& parent)
{
	if (action != Qt::MoveAction)
	{
		return false;
	}
	std::vector<CBaseObject*> objects;
	std::vector<CObjectLayer*> layers;
	if (!LevelModelsUtil::ProcessDragDropData(pData, objects, layers))
	{
		return false;
	}
	bool hasOnlyLayers = layers.size() && !objects.size();
	bool hasOnlyObjects = !layers.size() && objects.size();
	if (!parent.isValid())
	{
		if (!hasOnlyLayers)
		{
			return false;
		}
		CUndo undo("Move Layers");
		for (auto& layer : layers)
		{
			layer->SetAsRootLayer();
		}
		return true;
	}
	auto targetLayer = LayerFromIndex(parent);
	if (!targetLayer)
	{
		return false;
	}
	switch (targetLayer->GetLayerType())
	{
	case eObjectLayerType_Folder:
	{
		CUndo undo("Move Layers");
		if (!hasOnlyLayers)
		{
			return false;
		}
		for (auto& layer : layers)
		{
			if (!targetLayer->IsChildOf(layer) && layer->GetParent() != targetLayer)
			{
				targetLayer->AddChild(layer);
			}
		}
		return true;
	}
	case eObjectLayerType_Layer:
		{
			CUndo undo("Move Objects & Layers");
			// Optimization: Clear selection and reselect all objects at the end so we don't fire individual
			// selection changes. Also this makes sure objects are selected at the end of the operation
			IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
			pObjectManager->UnselectObjects(objects);

			for (auto& object : objects)
			{
				if (object->GetLayer() != targetLayer)
				{
					object->SetLayer(targetLayer);
				}
			}
			auto targetParentLayer = targetLayer->GetParent();
			if (!targetParentLayer)
			{
				for (auto& layer : layers)
				{
					layer->SetAsRootLayer();
				}
			}
			else
			{
				for (auto& layer : layers)
				{
					if (!targetParentLayer->IsChildOf(layer) && layer->GetParent() != targetParentLayer)
					{
						targetParentLayer->AddChild(layer);
					}
				}
			}
			// Re-select all objects
			pObjectManager->SelectObjects(objects);
			return true;
		}

	default:
		return false;
	}
}

//////////////////////////////////////////////////////////
bool CLevelModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (action != Qt::MoveAction)
	{
		return false;
	}
	std::vector<CBaseObject*> objects;
	std::vector<CObjectLayer*> layers;
	if (!LevelModelsUtil::ProcessDragDropData(pData, objects, layers))
	{
		return false;
	}

	auto targetLayer = LayerFromIndex(parent);
	bool hasOnlyLayers = layers.size() && !objects.size();

	if (!parent.isValid())
	{
		bool result = hasOnlyLayers && QAbstractItemModel::canDropMimeData(pData, action, row, column, parent); //Can only drop layers at root
		if (result && targetLayer)
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Move to %1").arg(QtUtil::ToQString(targetLayer->GetName())));
		else
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Invalid operation"));

		return result;
	}

	if (!targetLayer)
	{
		return false;
	}
	switch (targetLayer->GetLayerType())
	{
	case eObjectLayerType_Folder:
	{
		if (std::any_of(layers.begin(), layers.end(), [&](CObjectLayer* layer) { return targetLayer->IsChildOf(layer) || layer->GetParent() == targetLayer; }))
		{
			return false;
		}

		bool result = hasOnlyLayers && QAbstractItemModel::canDropMimeData(pData, action, row, column, parent);
		if (result)
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Move to %1").arg(QtUtil::ToQString(targetLayer->GetName())));
		else
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Invalid operation"));

		return result;
	}
	case eObjectLayerType_Layer:
		{
			auto targetParentLayer = targetLayer->GetParent();
			// do not drop an layer on a layer if it is already a child of it
			if (std::any_of(layers.begin(), layers.end(), [&](CObjectLayer* layer) { return (targetParentLayer ? targetParentLayer->IsChildOf(layer) : false) || layer->GetParent() == targetParentLayer; }))
			{
				return false;
			}
			// do not drop an object on a layer if it is already a child of it
			if (std::any_of(objects.begin(), objects.end(), [&](CBaseObject* object) { return object->GetLayer() == targetLayer; }))
			{
				return false;
			}

			bool result = QAbstractItemModel::canDropMimeData(pData, action, row, column, parent); //Can drop objects/layers in a layer

			if (result)
				CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Move to %1").arg(QtUtil::ToQString(targetLayer->GetName())));
			else
				CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Invalid operation"));

			return result;
		}

	default:
		return false;
	}
}

//////////////////////////////////////////////////////////
int CLevelModel::rowCount(const QModelIndex& parent) const
{
	if (m_ignoreLayerUpdates)
	{
		return 0;
	}

	CObjectLayer* pLayer = LayerFromIndex(parent);
	if (pLayer)
	{
		return pLayer->GetChildCount();
	}
	else
	{
		// rowCount for the top level layers
		int count = 0;
		const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
		for (CObjectLayer* pOtherLayer : layers)
		{
			if (pOtherLayer->GetParent() == nullptr)
			{
				// since all the layers are stored together we need to count only
				// the top level ones to get the right index
				++count;
			}
		}
		return count;
	}
}

//////////////////////////////////////////////////////////
CObjectLayer* CLevelModel::LayerFromIndex(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return nullptr;
	}
	return static_cast<CObjectLayer*>(index.internalPointer());
}

//////////////////////////////////////////////////////////
QModelIndex CLevelModel::IndexFromLayer(const CObjectLayer* pLayer) const
{
	if (pLayer)
	{
		CObjectLayer* pParent = pLayer->GetParent();
		if (pParent)
		{
			const int size = pParent->GetChildCount();
			for (int i = 0; i < size; ++i)
			{
				if (pParent->GetChild(i) == pLayer)
				{
					return createIndex(i, 0, reinterpret_cast<quintptr>(pLayer));
				}
			}
		}
		else
		{
			// pLayer is a top level item
			int index = 0;
			const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
			for (CObjectLayer* pOtherLayer : layers)
			{
				if (pOtherLayer == pLayer)
				{
					return createIndex(index, 0, reinterpret_cast<quintptr>(pLayer));
				}
				if (pOtherLayer->GetParent() == nullptr)
				{
					// since all the layers are stored together we need to count only
					// the top level ones to get the right index
					++index;
				}
			}
		}
	}
	return QModelIndex();
}

int CLevelModel::RootRowIfInserted(const CObjectLayer* pLayer) const
{
	//Calculate insertion position
	int index = 0;

	const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();

	for (CObjectLayer* pOtherLayer : layers)
	{
		if (pOtherLayer->GetParent() == nullptr)//Only count top level layers
		{
			if (pLayer->GetGUID() < pOtherLayer->GetGUID())
			{
				break;
			}
			else
			{
				++index;
			}
		}
	}

	return index;
}

//////////////////////////////////////////////////////////
bool CLevelModel::IsNameValid(const char* szName)
{
	CObjectLayerManager* layerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();

	// NOTE: Currently layer names NEED to be unique per level. THIS CANNOT BE CHANGED TO SUPPORT UNIQUE SIBLING NAMING
	// BECAUSE LAYERS ARE CURRENTLY REFERENCED BY NAME RATHER THAN FULL PATH IN MANY PLACES.
	if (layerManager->FindAnyLayerByName(szName))
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////
void CLevelModel::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	//eNotify_OnClearLevelContents : Not handled because it is always within other notifications
	//eNotify_OnBeginSceneOpen : Not handled because we are using BeginLoad/EndLoad events
	switch (event)
	{
	case eNotify_OnBeginLoad:
	case eNotify_OnBeginNewScene:
	case eNotify_OnBeginSceneClose:
		// set ignore to reset. This will basically make all row counts zero and ignore layer events for the duration of the loading
		beginResetModel();
		m_ignoreLayerUpdates = true;
		endResetModel();
		break;

	case eNotify_OnEndLoad:
	case eNotify_OnEndNewScene:
	case eNotify_OnEndSceneClose:
		// level has finished loading, remove event ignore and allow level to get the real layer counts again
		beginResetModel();
		m_ignoreLayerUpdates = false;
		endResetModel();
		break;

	default:
		break;
	}
}

//////////////////////////////////////////////////////////
void CLevelModel::OnLayerUpdate(const CLayerChangeEvent& event)
{
	signalBeforeLayerUpdateEvent(event);

	if (m_ignoreLayerUpdates)
		return;

	QModelIndex layerIdx = event.m_layer ? IndexFromLayer(event.m_layer) : QModelIndex();
	QModelIndex parentIdx = event.m_layer && event.m_layer->GetParent() ? IndexFromLayer(event.m_layer->GetParent()) : QModelIndex();

	switch (event.m_type)
	{
	case CLayerChangeEvent::LE_BEFORE_ADD:
		{
			if (!parentIdx.isValid()) //Layer will be inserted in the layers map according to its GUID
			{
				int row = RootRowIfInserted(event.m_layer);
				beginInsertRows(parentIdx, row, row);
			}
			else //Layer is appended at the end of parent's layer list
			{
				beginInsertRows(parentIdx, rowCount(parentIdx), rowCount(parentIdx));
			}
		}
		break;
	case CLayerChangeEvent::LE_AFTER_ADD:
		endInsertRows();
		break;
	case CLayerChangeEvent::LE_AFTER_REMOVE:
		// we don't handle LE_BEFORE_REMOVE event and reset the whole model here because we can't have 2 layer models 
		// changing at the same time. Let say we have 2 layers and an object from one layer is linked to an object form
		// another layer. If you delete one of the layers, then during the deletion another layer has to be updated.
		// That's why we can't use beginRemoveRows() on LE_BEFORE_REMOVE and then endRemoveRows() on LE_AFTER_REMOVE.
		// This probably could be optimize so that it's done only once if you delete sibling layers.
		// For folders is good because only top-most layer notifies about removal.
		beginResetModel();
		endResetModel();
		break;
	case CLayerChangeEvent::LE_BEFORE_PARENT_CHANGE:
		{
			QModelIndex futureParentIdx = event.m_savedParent ? IndexFromLayer(event.m_savedParent) : QModelIndex();
			if (futureParentIdx.isValid())
			{
				beginMoveRows(parentIdx, layerIdx.row(), layerIdx.row(), futureParentIdx, rowCount(futureParentIdx));
			}
			else
			{
				beginMoveRows(parentIdx, layerIdx.row(), layerIdx.row(), futureParentIdx, RootRowIfInserted(event.m_layer));
			}
			break;
		}
	case CLayerChangeEvent::LE_AFTER_PARENT_CHANGE:
		endMoveRows();
		break;
	case CLayerChangeEvent::LE_MODIFY:
		dataChanged(index(layerIdx.row(), 0, parentIdx), index(layerIdx.row(), columnCount(parentIdx) - 1, parentIdx));
		break;
	case CLayerChangeEvent::LE_UPDATE_ALL:
		beginResetModel();
		endResetModel();
		break;
	default:
		break;
	}
}

