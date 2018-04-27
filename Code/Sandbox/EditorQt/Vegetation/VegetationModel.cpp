// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "VegetationModel.h"

#include "VegetationMap.h"
#include "VegetationObject.h"
#include "VegetationDragDropData.h"

#include "Terrain/TerrainManager.h"

#include <QFileInfo>
#include <QSet>

namespace Private_VegetationModel
{
const QString kDefaultGroupNamePattern("Default%1");
const QString kDefaultGroupName("Default");
}

CVegetationModel::CVegetationModelItem::CVegetationModelItem(const QString& name, int row)
	: name(name)
	, row(row)
	, pVegetationObject(nullptr)
	, visibility(EVisibility::Hidden)
	, pParent(nullptr)
{}

CVegetationModel::CVegetationModelItem::CVegetationModelItem(int r, CVegetationObject* pVegetationObject, CVegetationModelItem* pParent)
	: name(pVegetationObject->GetFileName())
	, row(r)
	, pVegetationObject(pVegetationObject)
	, visibility(pVegetationObject->IsHidden() ? EVisibility::Hidden : EVisibility::Visible)
	, pParent(pParent)
{}

CVegetationModel::~CVegetationModel()
{
	DisconnectVegetationMap();
}

CVegetationModel::CVegetationModelItem* CVegetationModel::CVegetationModelItem::AddChild(CVegetationObject* pVegetationObject)
{
	VegetationModelItemPtr pChildPtr(new CVegetationModelItem(children.size(), pVegetationObject, this));
	children.push_back(std::move(pChildPtr));
	return children.back().get();
}

int CVegetationModel::CVegetationModelItem::rowCount() const
{
	return children.size();
}

bool CVegetationModel::CVegetationModelItem::IsGroup() const
{
	return pVegetationObject == nullptr;
}

CVegetationModel::CVegetationModelItem::EVisibility CVegetationModel::CVegetationModelItem::GetVisibilityFromCheckState(Qt::CheckState checkState)
{
	switch (checkState)
	{
	case Qt::Unchecked:
		return EVisibility::Hidden;

	case Qt::PartiallyChecked:
		return EVisibility::PartiallyVisible;

	case Qt::Checked:
		return EVisibility::Visible;

	// unknown value
	default:
		return EVisibility::Hidden;
	}
}

Qt::CheckState CVegetationModel::CVegetationModelItem::GetCheckStateFromVisibility(EVisibility visibility)
{
	switch (visibility)
	{
	case EVisibility::Hidden:
		return Qt::Unchecked;

	case EVisibility::PartiallyVisible:
		return Qt::PartiallyChecked;

	case EVisibility::Visible:
		return Qt::Checked;

	// unknown value
	default:
		return Qt::Unchecked;
	}
}

CVegetationModel::CVegetationModel(QObject* pParent)
	: QAbstractItemModel(pParent)
	, m_pVegetationMap(GetIEditorImpl()->GetVegetationMap())
{
	LoadObjects();
	connect(this, &CVegetationModel::dataChanged, this, &CVegetationModel::HandleModelInfoDataChange);
	connect(this, &CVegetationModel::rowsRemoved, this, &CVegetationModel::HandleModelInfoDataChange);
	connect(this, &CVegetationModel::rowsInserted, this, &CVegetationModel::HandleModelInfoDataChange);
	connect(this, &CVegetationModel::modelReset, this, &CVegetationModel::HandleModelInfoDataChange);
	ConnectVegetationMap();
}

QModelIndex CVegetationModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
	{
		return QModelIndex();
	}

	if (!parent.isValid())
	{
		return createIndex(row, column, m_groups[row].get());
	}

	auto pGroupItem = static_cast<CVegetationModelItem*>(parent.internalPointer());
	auto pChildItem = pGroupItem->children[row].get();
	return createIndex(row, column, pChildItem);
}

QModelIndex CVegetationModel::parent(const QModelIndex& child) const
{
	if (!child.isValid())
	{
		return QModelIndex();
	}

	auto pChildItem = static_cast<CVegetationModelItem*>(child.internalPointer());
	if (pChildItem->pParent)
	{
		auto pParentItem = pChildItem->pParent;
		return createIndex(pParentItem->row, 0, pParentItem);
	}
	return QModelIndex();
}

int CVegetationModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
	{
		return m_groups.size();
	}

	auto pParentTreeItem = static_cast<CVegetationModelItem*>(parent.internalPointer());
	return pParentTreeItem->rowCount();
}

int CVegetationModel::columnCount(const QModelIndex& parent) const
{
	return static_cast<int>(Column::Count);
}

QVariant CVegetationModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	auto pTreeItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	// Object Visiblity/Name columns have to be merged
	if (role == Qt::CheckStateRole && static_cast<Column>(index.column()) == Column::VisibleAndObject)
	{
		return CVegetationModelItem::GetCheckStateFromVisibility(pTreeItem->visibility);
	}

	if (pTreeItem->IsGroup())
	{
		return GetGroupData(pTreeItem, static_cast<Column>(index.column()), role);
	}
	return GetVegetationObjectData(pTreeItem, static_cast<Column>(index.column()), role);
}

QVariant CVegetationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	switch (static_cast<Column>(section))
	{
	case Column::VisibleAndObject:
		return tr("(Visible) Object");

	case Column::ObjectCount:
		return tr("Count");

	case Column::Textsize:
		return tr("TextMem Usage");

	case Column::Material:
		return tr("Material");

	case Column::ElevationMin:
		return tr("Elevation Min");

	case Column::ElevationMax:
		return tr("Elevation Max");

	case Column::SlopeMin:
		return tr("Slope Min");

	case Column::SlopeMax:
		return tr("Slope Max");

	default:
		return QVariant();
	}
}

Qt::ItemFlags CVegetationModel::flags(const QModelIndex& index) const
{
	// user may drop (nearly) everywhere
	auto itemFlags = QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;
	if (!index.isValid())
	{
		return itemFlags;
	}

	itemFlags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable;

	const auto pItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	const auto column = static_cast<Column>(index.column());
	if (pItem->IsGroup() && column == Column::VisibleAndObject)
	{
		itemFlags |= Qt::ItemIsEditable;
	}
	return itemFlags;
}

bool CVegetationModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid())
	{
		return false;
	}

	auto pItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	const auto column = static_cast<Column>(index.column());
	bool bDataChanged = false;
	if (pItem->IsGroup())
	{
		if (column == Column::VisibleAndObject)
		{
			switch (role)
			{
			case Qt::EditRole:
				RenameGroup(pItem, value.toString());
				bDataChanged = true;
				break;

			case Qt::CheckStateRole:
				{
					// direct value function call on variant does not work
					auto checkState = static_cast<Qt::CheckState>(value.toUInt());
					auto visibility = CVegetationModelItem::GetVisibilityFromCheckState(checkState);

					SetVisibilityRecursively(pItem, visibility);

					bDataChanged = true;
					break;
				}

			default:
				break;
			}
		}
	}
	else if (column == Column::VisibleAndObject && role == Qt::CheckStateRole)
	{
		// direct value function call on variant does not work
		auto checkState = static_cast<Qt::CheckState>(value.toUInt());
		auto visibility = CVegetationModelItem::GetVisibilityFromCheckState(checkState);
		SetVisibility(pItem, visibility);
		UpdateParentVisibilityRecursively(pItem);

		bDataChanged = true;
	}

	if (bDataChanged)
	{
		dataChanged(index, index);
	}

	return bDataChanged;
}

bool CVegetationModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	if (action != Qt::MoveAction)
	{
		return false;
	}

	auto pDragDropData = qobject_cast<const CVegetationDragDropData*>(pData);
	if (!pDragDropData)
	{
		return false;
	}

	// not allowed to drop objects between groups
	if (pDragDropData->HasObjectListData() && !parent.isValid())
	{
		return false;
	}

	// not allowed to drop groups "outside" all groups
	if (pDragDropData->HasGroupListData() && !parent.isValid() && row == -1)
	{
		return false;
	}

	return QAbstractItemModel::canDropMimeData(pData, action, row, column, parent);
}

bool CVegetationModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int, const QModelIndex& parent)
{
	if (action != Qt::MoveAction)
	{
		return true;
	}

	auto pDragDropData = qobject_cast<const CVegetationDragDropData*>(pData);
	if (!pDragDropData)
	{
		return true;
	}

	// need to use owning containers
	// remove old objects/groups
	auto movedVegetationObjectItems = DeserializeDropObjectData(pDragDropData);
	auto movedGroupItems = DeserializeDropGroupData(pDragDropData);

	DropVegetationObjects(movedVegetationObjectItems, row, parent);
	DropGroups(movedGroupItems, row, parent);

	return true;
}

QMimeData* CVegetationModel::mimeData(const QModelIndexList& indexes) const
{
	QSet<int> encodedObjectIds;
	QSet<int> encodedGroupIndexes;

	for (const auto& index : indexes)
	{
		auto pItem = static_cast<CVegetationModelItem*>(index.internalPointer());
		if (pItem->IsGroup())
		{
			const auto index = pItem->row;
			encodedGroupIndexes.insert(index);
		}
	}

	for (const auto& index : indexes)
	{
		auto pItem = static_cast<CVegetationModelItem*>(index.internalPointer());
		if (!pItem->IsGroup())
		{
			const auto pVegetationObject = pItem->pVegetationObject;
			auto pParentGroup = pItem->pParent;
			// do not encode vegetation objects when their groups are encoded already
			if (pVegetationObject && !encodedGroupIndexes.contains(pParentGroup->row))
			{
				const auto id = pVegetationObject->GetId();
				encodedObjectIds.insert(id);
			}
		}
	}

	auto pDragDropData = new CVegetationDragDropData();
	if (!encodedObjectIds.empty())
	{
		QVector<int> objectIdVector;
		for (auto id : encodedObjectIds)
		{
			objectIdVector.push_back(id);
		}
		QByteArray encodedObjectData;
		QDataStream objectStream(&encodedObjectData, QIODevice::WriteOnly);
		objectStream << objectIdVector;
		pDragDropData->SetObjectListData(encodedObjectData);
	}

	if (!encodedGroupIndexes.empty())
	{
		QVector<int> groupIdxVector;
		for (auto id : encodedGroupIndexes)
		{
			groupIdxVector.push_back(id);
		}
		QByteArray encodedGroupData;
		QDataStream groupStream(&encodedGroupData, QIODevice::WriteOnly);
		groupStream << groupIdxVector;
		pDragDropData->SetGroupListData(encodedGroupData);
	}

	return pDragDropData;
}

QStringList CVegetationModel::mimeTypes() const
{
	QStringList list;
	list << CVegetationDragDropData::GetObjectListMimeFormat() << CVegetationDragDropData::GetGroupListMimeFormat();
	return list;
}

Qt::DropActions CVegetationModel::supportedDragActions() const
{
	return Qt::CopyAction | Qt::MoveAction;
}

Qt::DropActions CVegetationModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

void CVegetationModel::ConnectVegetationMap()
{
	m_pVegetationMap->signalAllVegetationObjectsChanged.Connect(this, &CVegetationModel::OnVegetationMapObjectUpdate);
	m_pVegetationMap->signalVegetationObjectChanged.Connect(this, &CVegetationModel::UpdateVegetationObject);
	m_pVegetationMap->signalVegetationObjectsMerged.Connect(this, &CVegetationModel::Reset);
}

void CVegetationModel::DisconnectVegetationMap()
{
	m_pVegetationMap->signalAllVegetationObjectsChanged.DisconnectObject(this);
	m_pVegetationMap->signalVegetationObjectChanged.DisconnectObject(this);
	m_pVegetationMap->signalVegetationObjectsMerged.DisconnectObject(this);
}

void CVegetationModel::LoadObjects()
{
	for (int i = 0; i < m_pVegetationMap->GetObjectCount(); ++i)
	{
		auto pVegetationObject = m_pVegetationMap->GetObject(i);
		auto pGroup = FindOrCreateGroup(pVegetationObject->GetGroup());
		AddVegetationObjectToGroup(pGroup, pVegetationObject);
	}

	// set initial group visibility
	for (const auto& pGroup : m_groups)
	{
		const auto& children = pGroup->children;
		if (!children.empty())
		{
			UpdateParentVisibilityRecursively(children.front().get());
		}
	}
}

void CVegetationModel::ClearSelection()
{
	for (int i = 0; i < m_pVegetationMap->GetObjectCount(); ++i)
	{
		auto pVegetationObject = m_pVegetationMap->GetObject(i);
		pVegetationObject->SetSelected(false);
	}
}

void CVegetationModel::BeginResetOnLevelChange()
{
	beginResetModel();
	m_groups.clear();
	m_vegetationObjectToItem.clear();
	DisconnectVegetationMap();
	endResetModel();
}

void CVegetationModel::EndResetOnLevelChange()
{
	LOADING_TIME_PROFILE_SECTION;
	beginResetModel();
	m_pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	ConnectVegetationMap();
	LoadObjects();
	endResetModel();
}

CVegetationModel::CVegetationModelItem* CVegetationModel::FindGroup(const QString& name)
{
	for (const auto& pGroup : m_groups)
	{
		if (pGroup->name == name)
		{
			return pGroup.get();
		}
	}
	return nullptr;
}

CVegetationModel::CVegetationModelItem* CVegetationModel::CreateGroup(const QString& name)
{
	VegetationModelItemPtr pGroupPtr(new CVegetationModelItem(name, m_groups.size()));
	m_groups.push_back(std::move(pGroupPtr));
	return m_groups.back().get();
}

CVegetationModel::CVegetationModelItem* CVegetationModel::FindOrCreateGroup(const QString& name)
{
	auto pGroupItem = FindGroup(name);
	if (pGroupItem)
	{
		return pGroupItem;
	}
	return CreateGroup(name);
}

void CVegetationModel::RenameGroup(CVegetationModelItem* pGroupItem, const QString& name)
{
	pGroupItem->name = name;
	for (const auto& pVegetationObjectItem : pGroupItem->children)
	{
		pVegetationObjectItem->pVegetationObject->SetGroup(name.toStdString().c_str());
	}
}

CVegetationModel::CVegetationModelItem* CVegetationModel::AddGroupByName(const QString& name)
{
	const auto groupSize = m_groups.size();
	beginInsertRows(QModelIndex(), groupSize, groupSize);
	auto pGroupItem = CreateGroup(name);
	endInsertRows();
	return pGroupItem;
}

void CVegetationModel::AddGroup()
{
	// search for unused group name with pattern "Default%n". If all n
	// in the range of unsigned int are used the pattern is extended with an
	// underscore before the number "Default_..._%n" to ensure to build an
	// unused group name.
	unsigned suffixCount = 1;
	QString defaultName;
	QString namePattern = Private_VegetationModel::kDefaultGroupNamePattern;
	while (true)
	{
		if (suffixCount == 0)
		{
			namePattern = namePattern.arg("_%1");
			++suffixCount;
		}
		defaultName = namePattern.arg(suffixCount++);
		auto nameMatch = std::find_if(m_groups.cbegin(), m_groups.cend(), [&defaultName](const VegetationModelItemPtr& pGroup)
		{
			return pGroup->name == defaultName;
		});

		if (nameMatch == m_groups.cend())
		{
			break;
		}
	}
	AddGroupByName(defaultName);
}

void CVegetationModel::RenameGroup(const QModelIndex& selectedGroupIndex, const QString& name)
{
	if (!selectedGroupIndex.isValid())
	{
		return;
	}

	auto pGroupItem = static_cast<CVegetationModelItem*>(selectedGroupIndex.internalPointer());
	RenameGroup(pGroupItem, name);
	dataChanged(selectedGroupIndex, selectedGroupIndex);
}

bool CVegetationModel::IsGroup(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return false;
	}
	auto pTreeItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	return pTreeItem->IsGroup();
}

bool CVegetationModel::IsVegetationObject(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return false;
	}
	auto pTreeItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	return !pTreeItem->IsGroup();
}

CVegetationObject* CVegetationModel::GetVegetationObject(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return false;
	}
	auto pTreeItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	return pTreeItem->pVegetationObject;
}

const CVegetationModel::CVegetationModelItem* CVegetationModel::GetGroup(const QModelIndex& index) const
{
	if (!index.isValid())
	{
		return nullptr;
	}
	auto pTreeItem = static_cast<CVegetationModelItem*>(index.internalPointer());
	if (pTreeItem->IsGroup())
	{
		return pTreeItem;
	}
	return nullptr;
}

QModelIndex CVegetationModel::GetGroup(CVegetationObject* pVegetationObject)
{
	if (!pVegetationObject || !pVegetationObject->GetGroup())
		return QModelIndex();

	CVegetationModelItem* pGroupItem = FindGroup(pVegetationObject->GetGroup());
	if (!pGroupItem)
		return QModelIndex();

	return index(pGroupItem->row, 0);
}

void CVegetationModel::Select(const QModelIndexList& selectedIndexes)
{
	ClearSelection();
	for (const auto& selectedIndex : selectedIndexes)
	{
		if (IsGroup(selectedIndex))
		{
			auto pGroupItem = GetGroup(selectedIndex);
			for (const auto& pVegetationObjectItem : pGroupItem->children)
			{
				pVegetationObjectItem->pVegetationObject->SetSelected(true);
			}
		}
		else
		{
			auto pVegetationObject = GetVegetationObject(selectedIndex);
			pVegetationObject->SetSelected(true);
		}
	}
}

QModelIndexList CVegetationModel::FindVegetationObjects(const QVector<CVegetationInstance*> selectedInstances)
{
	QModelIndexList selectionIndices;
	const int groupCount = rowCount();

	for (int group = 0; group < groupCount; ++group)
	{
		const int objectCount = rowCount(index(group, 0));
		QModelIndex groupIdx = index(group, 0);

		for (int item = 0; item < objectCount; ++item)
		{
			QModelIndex itemIdx = index(item, 0, groupIdx);
			if (itemIdx.isValid())
			{
				auto pItem = static_cast<CVegetationModelItem*>(itemIdx.internalPointer());
				if (!pItem->IsGroup())
				{
					for (CVegetationInstance* pInstance : selectedInstances)
					{
						if (pInstance->object == pItem->pVegetationObject)
						{
							selectionIndices.append(itemIdx);
						}
					}
				}
			}
		}
	}

	return selectionIndices;
}

CVegetationObject* CVegetationModel::AddVegetationObject(const QModelIndex& currentIndex, const QString& filename)
{
	auto pVegetationObject = m_pVegetationMap->CreateObject();
	if (!pVegetationObject)
	{
		return nullptr;
	}

	auto pGroupItem = [this, &currentIndex]
	{
		using namespace Private_VegetationModel;

		if (currentIndex.isValid())
		{
			auto pItem = static_cast<CVegetationModelItem*>(currentIndex.internalPointer());
			if (pItem->IsGroup())
			{
				return pItem;
			}
			return pItem->pParent;
		}

		auto pGroupItem = FindGroup(kDefaultGroupName);
		// new group was created
		if (pGroupItem)
		{
			return pGroupItem;
		}
		return AddGroupByName(kDefaultGroupName);
	} ();
	pVegetationObject->SetGroup(pGroupItem->name.toStdString().c_str());
	pVegetationObject->SetFileName(filename.toStdString().c_str());

	const auto objectCount = pGroupItem->rowCount();

	beginInsertRows(index(pGroupItem->row, 0), objectCount, objectCount);
	AddVegetationObjectToGroup(pGroupItem, pVegetationObject);
	UpdateParentVisibilityRecursively(pGroupItem->children.back().get());
	endInsertRows();

	return pVegetationObject;
}

void CVegetationModel::RemoveItems(const QModelIndexList& selectedIndexes)
{
	if (selectedIndexes.empty())
	{
		return;
	}

	QHash<CVegetationModelItem*, QSet<CVegetationModelItem*>> removeObjects;
	QSet<CVegetationModelItem*> removeGroups;

	for (const auto& selectedIndex : selectedIndexes)
	{
		auto pItem = static_cast<CVegetationModelItem*>(selectedIndex.internalPointer());
		if (pItem->IsGroup())
		{
			removeGroups.insert(pItem);
		}
	}

	for (const auto& selectedIndex : selectedIndexes)
	{
		auto pItem = static_cast<CVegetationModelItem*>(selectedIndex.internalPointer());
		if (!pItem->IsGroup())
		{
			const auto pVegetationObject = pItem->pVegetationObject;
			auto pParentGroup = pItem->pParent;
			// do not remove vegetation objects when their groups are removed
			if (pVegetationObject && !removeGroups.contains(pParentGroup))
			{
				removeObjects[pParentGroup].insert(pItem);
			}
		}
	}

	for (auto removeObjectIt = removeObjects.begin(); removeObjectIt != removeObjects.end(); ++removeObjectIt)
	{
		const auto pParentGroup = removeObjectIt.key();
		const auto& objects = removeObjectIt.value();
		RemoveVegetationObjectsFromParent(pParentGroup, objects);
	}

	RemoveGroups(removeGroups);

	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();
	GetIEditorImpl()->UpdateViews(eUpdateStatObj);
}

void CVegetationModel::RemoveVegetationObjectsFromParent(CVegetationModelItem* pParentGroup, const QSet<CVegetationModelItem*>& removeObjects)
{
	const auto& groupObjects = pParentGroup->children;
	for (int i = groupObjects.size() - 1; i >= 0; --i)
	{
		for (; i >= 0 && !removeObjects.contains(groupObjects[i].get()); --i)
			;
		if (i < 0)
		{
			break;
		}
		auto last = i;
		for (; i >= 0 && removeObjects.contains(groupObjects[i].get()); --i)
			;
		auto first = i + 1;

		beginRemoveRows(index(pParentGroup->row, 0), first, last);
		for (int j = last; j >= first; --j)
		{
			RemoveVegetationObject(groupObjects[j].get());
		}
		endRemoveRows();
	}
}

void CVegetationModel::RemoveGroups(const QSet<CVegetationModelItem*>& removeGroups)
{
	for (int i = m_groups.size() - 1; i >= 0; --i)
	{
		for (; i >= 0 && !removeGroups.contains(m_groups[i].get()); --i)
			;
		if (i < 0)
		{
			break;
		}
		auto last = i;
		for (; i >= 0 && removeGroups.contains(m_groups[i].get()); --i)
			;
		auto first = i + 1;

		beginRemoveRows(QModelIndex(), first, last);
		for (int j = last; j >= first; --j)
		{
			RemoveGroup(m_groups[j].get());
		}
		endRemoveRows();
	}
}

void CVegetationModel::RemoveGroup(CVegetationModel::CVegetationModelItem* pGroupItem)
{
	const auto itemRow = pGroupItem->row;
	for (const auto& pVegetationItem : pGroupItem->children)
	{
		auto pVegetationObject = pVegetationItem->pVegetationObject;
		m_pVegetationMap->RemoveObject(pVegetationObject);
		m_vegetationObjectToItem.remove(pVegetationObject);
	}

	m_groups.erase(m_groups.begin() + itemRow);

	// make rows valid again
	ValidateRows(m_groups, itemRow);
}

void CVegetationModel::RemoveVegetationObject(CVegetationModel::CVegetationModelItem* pVegetationObjectItem)
{
	auto pGroupItem = pVegetationObjectItem->pParent;
	const auto itemRow = pVegetationObjectItem->row;

	auto pVegetationObject = pVegetationObjectItem->pVegetationObject;
	m_pVegetationMap->RemoveObject(pVegetationObject);
	m_vegetationObjectToItem.remove(pVegetationObject);
	pGroupItem->children.erase(pGroupItem->children.begin() + itemRow);

	// make rows valid
	ValidateChildRows(pGroupItem, itemRow);
}

void CVegetationModel::ReplaceVegetationObject(const QModelIndex& objectIndex, const QString& filename)
{
	if (!objectIndex.isValid())
	{
		return;
	}

	auto pTreeItem = static_cast<CVegetationModelItem*>(objectIndex.internalPointer());
	if (pTreeItem->IsGroup())
	{
		return;
	}

	pTreeItem->pVegetationObject->SetFileName(filename.toStdString().c_str());
	dataChanged(objectIndex, objectIndex);

	m_pVegetationMap->RepositionObject(pTreeItem->pVegetationObject);
}

void CVegetationModel::CloneObject(const QModelIndex& objectIndex)
{
	if (!objectIndex.isValid())
	{
		return;
	}

	auto pTreeItem = static_cast<CVegetationModelItem*>(objectIndex.internalPointer());
	if (pTreeItem->IsGroup())
	{
		return;
	}

	auto pGroupItem = pTreeItem->pParent;
	auto pGroupObjCount = pGroupItem->rowCount();

	beginInsertRows(objectIndex.parent(), pGroupObjCount, pGroupObjCount);
	auto pNewVegetationObject = m_pVegetationMap->CreateObject(pTreeItem->pVegetationObject);
	AddVegetationObjectToGroup(pGroupItem, pNewVegetationObject);
	endInsertRows();
}

void CVegetationModel::UpdateVegetationObject(CVegetationObject* pVegetationObject)
{
	assert(pVegetationObject);
	if (pVegetationObject)
	{
		auto pVegetationObjectItem = m_vegetationObjectToItem[pVegetationObject];
		auto topLeftIndex = createIndex(pVegetationObjectItem->row, 0, pVegetationObjectItem);
		dataChanged(topLeftIndex, topLeftIndex.sibling(topLeftIndex.row(), static_cast<int>(Column::Count)));
	}
}

void CVegetationModel::UpdateAllVegetationObjects()
{
	dataChanged(QModelIndex(), QModelIndex());
}

void CVegetationModel::MoveInstancesToGroup(const QString& groupName, const QVector<CVegetationInstance*> selectedInstances)
{
	if (groupName.isNull() || selectedInstances.empty())
	{
		return;
	}

	beginResetModel();
	auto numThings = selectedInstances.size();
	for (int i = 0; i < numThings; ++i)
	{
		auto pSelectedObject = selectedInstances[i]->object;
		if (groupName != pSelectedObject->GetGroup())
		{
			auto pNewVegetationObject = m_pVegetationMap->CreateObject(pSelectedObject);
			if (pNewVegetationObject)
			{
				auto pGroupItem = FindOrCreateGroup(groupName);
				pNewVegetationObject->SetGroup(groupName.toStdString().c_str());
				AddVegetationObjectToGroup(pGroupItem, pNewVegetationObject);
				for (int j = i; j < numThings; ++j)
				{
					if (selectedInstances[j]->object == pSelectedObject)
					{
						pSelectedObject->SetNumInstances(pSelectedObject->GetNumInstances() - 1);
						selectedInstances[j]->object = pNewVegetationObject;
						pNewVegetationObject->SetNumInstances(pNewVegetationObject->GetNumInstances() + 1);
					}
				}
			}
		}
	}
	endResetModel();
}

void CVegetationModel::AddVegetationObjectToGroup(CVegetationModelItem* pGroup, CVegetationObject* pVegetationObject)
{
	auto newObjectItem = pGroup->AddChild(pVegetationObject);
	m_vegetationObjectToItem[pVegetationObject] = newObjectItem;
}

QVariant CVegetationModel::GetGroupData(CVegetationModelItem* pGroup, Column column, int role) const
{
	if (!pGroup->IsGroup())
	{
		return QVariant();
	}

	if (column != Column::VisibleAndObject || (role != Qt::DisplayRole && role != Qt::EditRole))
	{
		return QVariant();
	}
	return pGroup->name;
}

QVariant CVegetationModel::GetVegetationObjectData(CVegetationModelItem* pVegetationObjectItem, Column column, int role) const
{
	if (pVegetationObjectItem->IsGroup())
	{
		return QVariant();
	}

	auto pVegetationObject = pVegetationObjectItem->pVegetationObject;
	CRY_ASSERT(pVegetationObject);
	if (!pVegetationObject)
	{
		return QVariant();
	}

	switch (role)
	{
	case Qt::DisplayRole:
		{
			switch (column)
			{
			case Column::VisibleAndObject:
				return QFileInfo(pVegetationObjectItem->name).baseName();

			case Column::ObjectCount:
				return pVegetationObject->GetNumInstances();

			case Column::Textsize:
				{
					int textSize = pVegetationObject->GetTextureMemUsage();
					return QString("%1 K").arg(textSize / 1024);
				}

			case Column::Material:
				{
					auto staticObj = pVegetationObject->GetObject();
					if (!staticObj)
					{
						return QVariant();
					}
					auto pMaterial = staticObj->GetMaterial();
					if (pMaterial)
					{
						return QFileInfo(pMaterial->GetName()).baseName();
					}
					return QVariant();
				}
			case Column::ElevationMin:
				return pVegetationObject->GetElevationMin();

			case Column::ElevationMax:
				return pVegetationObject->GetElevationMax();

			case Column::SlopeMin:
				return pVegetationObject->GetSlopeMin();

			case Column::SlopeMax:
				return pVegetationObject->GetSlopeMax();

			default:
				return QVariant();
			}
		}

	case Qt::ToolTipRole:
		{
			if (column != Column::VisibleAndObject)
			{
				return QVariant();
			}
			return pVegetationObjectItem->name;
		}

	default:
		return QVariant();
	}
}

void CVegetationModel::HandleModelInfoDataChange()
{
	auto objectCount = m_pVegetationMap->GetObjectCount();
	auto instanceCount = m_pVegetationMap->GetNumInstances();
	auto textureUsage = m_pVegetationMap->GetTexureMemoryUsage(false) / 1024;
	InfoDataChanged(objectCount, instanceCount, textureUsage);
}

void CVegetationModel::Reset()
{
	beginResetModel();
	m_groups.clear();
	m_vegetationObjectToItem.clear();
	LoadObjects();
	endResetModel();
}

std::vector<CVegetationModel::VegetationModelItemPtr> CVegetationModel::DeserializeDropGroupData(const CVegetationDragDropData* pDragDropData)
{
	std::vector<VegetationModelItemPtr> movedGroupItems;
	if (!pDragDropData->HasGroupListData())
	{
		return movedGroupItems;
	}

	const auto byteArray = pDragDropData->GetGroupListData();
	QDataStream stream(byteArray);
	QVector<int> groupIndexes;
	stream >> groupIndexes;
	std::sort(groupIndexes.begin(), groupIndexes.end(), std::greater<int>());

	for (const auto groupIdx : groupIndexes)
	{
		beginRemoveRows(QModelIndex(), groupIdx, groupIdx);

		movedGroupItems.push_back(std::move(m_groups[groupIdx]));
		m_groups.erase(m_groups.cbegin() + groupIdx);

		endRemoveRows();
	}
	ValidateRows(m_groups);
	return movedGroupItems;
}

std::vector<CVegetationModel::VegetationModelItemPtr> CVegetationModel::DeserializeDropObjectData(const CVegetationDragDropData* pDragDropData)
{
	std::vector<VegetationModelItemPtr> movedVegetationObjectItems;
	if (!pDragDropData->HasObjectListData())
	{
		return movedVegetationObjectItems;
	}

	const auto byteArray = pDragDropData->GetObjectListData();
	QDataStream stream(byteArray);
	QSet<int> objectIds;
	stream >> objectIds;

	for (const auto objectId : objectIds)
	{
		auto pVegetationObject = m_pVegetationMap->GetObjectById(objectId);
		if (pVegetationObject)
		{
			auto pObjectItem = m_vegetationObjectToItem[pVegetationObject];
			auto pGroupItem = pObjectItem->pParent;
			const auto itemRow = pObjectItem->row;

			beginRemoveRows(index(pGroupItem->row, 0), itemRow, itemRow);

			movedVegetationObjectItems.push_back(std::move(pGroupItem->children[itemRow]));
			pGroupItem->children.erase(pGroupItem->children.cbegin() + itemRow);
			// make rows valid
			ValidateChildRows(pGroupItem, itemRow);

			endRemoveRows();
		}
	}
	return movedVegetationObjectItems;
}

void CVegetationModel::DropVegetationObjects(std::vector<VegetationModelItemPtr>& movedVegetationObjectItems, int row, const QModelIndex& parent)
{
	if (movedVegetationObjectItems.empty())
	{
		return;
	}

	auto pDropTargetItem = parent.isValid() ? static_cast<CVegetationModelItem*>(parent.internalPointer())
	                       : m_groups[row].get(); // impossible to drop on invisible root -> row > 0

	auto parentIndex = parent;
	// in group between other objects
	auto insertRow = row;
	// directly on group
	if (insertRow == -1 && pDropTargetItem->IsGroup())
	{
		insertRow = pDropTargetItem->rowCount();
	}
	// target can be root only if drop was done directly on object
	else if (!pDropTargetItem->IsGroup())
	{
		// insert after drop target object
		insertRow = pDropTargetItem->row + 1;
		pDropTargetItem = pDropTargetItem->pParent;
		parentIndex = index(pDropTargetItem->row, 0);
	}
	// drop between objects
	else
	{
		// adapt insert row when objects are dragged from a lower position
		// to a higher position within the same parent group
		for (const auto& pMovedObjectItem : movedVegetationObjectItems)
		{
			if (pMovedObjectItem->pParent == pDropTargetItem && pMovedObjectItem->row < row)
			{
				--insertRow;
			}
		}
	}

	const auto bIsInsertOp = insertRow < pDropTargetItem->rowCount();
	const auto insertCount = movedVegetationObjectItems.size();

	beginInsertRows(parentIndex, insertRow, insertRow + insertCount - 1);

	if (bIsInsertOp)
	{
		for (auto& pMovedObjectItem : movedVegetationObjectItems)
		{
			pMovedObjectItem->pParent = pDropTargetItem;
			pMovedObjectItem->pVegetationObject->SetGroup(pDropTargetItem->name.toStdString().c_str());
			const auto insertPos = pDropTargetItem->children.cbegin() + insertRow;
			pDropTargetItem->children.insert(insertPos, std::move(pMovedObjectItem));
		}
	}
	else
	{
		for (auto& pMovedObjectItem : movedVegetationObjectItems)
		{
			pMovedObjectItem->pParent = pDropTargetItem;
			pMovedObjectItem->pVegetationObject->SetGroup(pDropTargetItem->name.toStdString().c_str());
			pDropTargetItem->children.push_back(std::move(pMovedObjectItem));
		}
	}
	ValidateChildRows(pDropTargetItem, insertRow);

	endInsertRows();
}

void CVegetationModel::DropGroups(std::vector<VegetationModelItemPtr>& movedGroupItems, int row, const QModelIndex& parent)
{
	if (movedGroupItems.empty())
	{
		return;
	}

	auto pDropTargetItem = parent.isValid() ? static_cast<CVegetationModelItem*>(parent.internalPointer())
	                       : nullptr;

	auto insertRow = row;
	// drop between groups
	if (!pDropTargetItem)
	{
		// calculate row when group is to be dropped between groups
		for (auto& pMovedGroupItem : movedGroupItems)
		{
			if (pMovedGroupItem->row < row)
			{
				--insertRow;
			}
		}
	}
	// drop directly on object
	else if (!pDropTargetItem->IsGroup())
	{
		insertRow = pDropTargetItem->pParent->row + 1;
	}
	// drop directly on group
	else
	{
		insertRow = pDropTargetItem->row + 1;
	}

	const auto bIsInsertOp = insertRow < m_groups.size();
	const auto insertCount = movedGroupItems.size();

	beginInsertRows(QModelIndex(), insertRow, insertRow + insertCount - 1);

	if (bIsInsertOp)
	{
		for (auto& pMovedGroupItem : movedGroupItems)
		{
			const auto insertPos = m_groups.cbegin() + insertRow;
			m_groups.insert(insertPos, std::move(pMovedGroupItem));
		}
	}
	else
	{
		for (auto& pMovedGroupItem : movedGroupItems)
		{
			m_groups.push_back(std::move(pMovedGroupItem));
		}
	}
	ValidateRows(m_groups, insertRow);

	endInsertRows();
}

void CVegetationModel::OnVegetationMapObjectUpdate(bool bNeedsReset)
{
	if (bNeedsReset)
	{
		Reset();
	}
	else
	{
		UpdateAllVegetationObjects();
	}
}

void CVegetationModel::ValidateChildRows(CVegetationModelItem* pGroup, int startIdx) const
{
	ValidateRows(pGroup->children, startIdx);
}

void CVegetationModel::ValidateRows(std::vector<VegetationModelItemPtr>& items, int startIdx) const
{
	for (int i = startIdx; i < items.size(); ++i)
	{
		items[i]->row = i;
	}
}

void CVegetationModel::SetVisibility(CVegetationModelItem* pItem, CVegetationModelItem::EVisibility visibility)
{
	pItem->visibility = visibility;

	auto pVegetationObject = pItem->pVegetationObject;
	if (pVegetationObject)
	{
		CRY_ASSERT(visibility != CVegetationModelItem::EVisibility::PartiallyVisible);
		m_pVegetationMap->HideObject(pVegetationObject, visibility == CVegetationModelItem::EVisibility::Hidden ? true : false);
	}
}

void CVegetationModel::SetVisibilityRecursively(CVegetationModelItem* pItem, CVegetationModelItem::EVisibility visibility)
{
	SetVisibility(pItem, visibility);

	const auto& children = pItem->children;
	for (const auto& pChild : children)
	{
		SetVisibilityRecursively(pChild.get(), visibility);
	}

	if (children.empty())
	{
		return;
	}

	auto column = static_cast<int>(Column::VisibleAndObject);
	dataChanged(createIndex(children.front()->row, column, children.front().get()), createIndex(children.back()->row, column, children.back().get()));
}

void CVegetationModel::UpdateParentVisibilityRecursively(CVegetationModelItem* pItem)
{
	auto pParent = pItem->pParent;
	if (!pParent)
	{
		return;
	}

	const auto& siblings = pParent->children;
	CRY_ASSERT(!siblings.empty());
	if (siblings.empty())
	{
		return;
	}

	auto visibility = siblings.front()->visibility;
	if (std::any_of(siblings.cbegin(), siblings.cend(), [visibility](const VegetationModelItemPtr& pSibling) { return pSibling->visibility != visibility; }))
	{
		visibility = CVegetationModelItem::EVisibility::PartiallyVisible;
	}

	pParent->visibility = visibility;
	auto column = static_cast<int>(Column::VisibleAndObject);
	auto updateIndex = createIndex(pParent->row, column, pParent);
	dataChanged(updateIndex, updateIndex);

	// bubble up
	UpdateParentVisibilityRecursively(pParent);
}

