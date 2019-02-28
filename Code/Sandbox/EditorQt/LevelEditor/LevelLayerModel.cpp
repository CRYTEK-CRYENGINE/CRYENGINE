// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "LevelLayerModel.h"

#include "IObjectManager.h"
#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/BrushObject.h"
#include "Objects/EntityObject.h"
#include "Objects/GeomEntity.h"
#include "Objects/Group.h"
#include "Material/Material.h"
#include "HyperGraph/FlowGraphHelpers.h"

#include <CrySystem/ISystem.h>
#include <ProxyModels/ItemModelAttribute.h>
#include <CrySerialization/Enum.h>
#include "QAdvancedItemDelegate.h"
#include "CryIcon.h"
#include "QtUtil.h"
#include "DragDrop.h"

namespace LevelModelsAttributes
{
CItemModelAttribute s_visibleAttribute("Visible", eAttributeType_Boolean, CItemModelAttribute::Visible, true, Qt::Checked);
CItemModelAttribute s_frozenAttribute("Frozen", eAttributeType_Boolean);
CItemModelAttribute s_layerNameAttribute("Layer", eAttributeType_String);
CItemModelAttribute s_objectTypeDescAttribute("Type", eAttributeType_String);
CItemModelAttribute s_defaultMaterialAttribute("Default Material", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_customMaterialAttribute("Custom Material", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_breakableAttribute("Breakable", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_smartObjectAttribute("Smart Object", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_flowGraphAttribute("Flow Graph", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_geometryAttribute("Geometry", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_geometryInstancesAttribute("Instances", eAttributeType_Int, CItemModelAttribute::StartHidden);
CItemModelAttribute s_lodCountAttribute("LOD Count", eAttributeType_Int, CItemModelAttribute::StartHidden);
CItemModelAttribute s_specAttribute("Spec", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttribute s_aiGroupIdAttribute("AI GroupID", eAttributeType_String, CItemModelAttribute::StartHidden);
CItemModelAttributeEnumT<ObjectType> s_objectTypeAttribute("Object Type", CItemModelAttribute::StartHidden);
CItemModelAttribute s_LayerColorAttribute("Layer Color", eAttributeType_String, CItemModelAttribute::Visible, false);
}
///////////////////////////////////////////////////////////////////////////////////////////////
// Temporary hot-fix to get ObjectType enum in Sandbox, since the enum registration works
// only in the module where the enum was registered
///////////////////////////////////////////////////////////////////////////////////////////////
SERIALIZATION_ENUM_BEGIN(ObjectType, "Object Type")
SERIALIZATION_ENUM(OBJTYPE_GROUP, "group", "Group")
SERIALIZATION_ENUM(OBJTYPE_TAGPOINT, "tagpoint", "Tag Point");
SERIALIZATION_ENUM(OBJTYPE_AIPOINT, "aipoint", "AI Point");
SERIALIZATION_ENUM(OBJTYPE_ENTITY, "entity", "Entity");
SERIALIZATION_ENUM(OBJTYPE_SHAPE, "shape", "Shape");
SERIALIZATION_ENUM(OBJTYPE_VOLUME, "volume", "Volume");
SERIALIZATION_ENUM(OBJTYPE_BRUSH, "brush", "Brush");
SERIALIZATION_ENUM(OBJTYPE_PREFAB, "prefab", "Prefab");
SERIALIZATION_ENUM(OBJTYPE_SOLID, "solid", "Solid");
SERIALIZATION_ENUM(OBJTYPE_CLOUD, "cloud", "Cloud");
SERIALIZATION_ENUM(OBJTYPE_VOXEL, "voxel", "Voxel");
SERIALIZATION_ENUM(OBJTYPE_ROAD, "road", "Road");
SERIALIZATION_ENUM(OBJTYPE_OTHER, "other", "Other");
SERIALIZATION_ENUM(OBJTYPE_DECAL, "decal", "Decal");
SERIALIZATION_ENUM(OBJTYPE_DISTANCECLOUD, "distancecloud", "Distance Cloud");
SERIALIZATION_ENUM(OBJTYPE_TELEMETRY, "telemetry", "Telemetry");
SERIALIZATION_ENUM(OBJTYPE_REFPICTURE, "refpicture", "Reference Picture");
SERIALIZATION_ENUM(OBJTYPE_GEOMCACHE, "geomcache", "Geometry Cache");
SERIALIZATION_ENUM(OBJTYPE_VOLUMESOLID, "volumesolid", "Volume Solid");
SERIALIZATION_ENUM_END()

CItemModelAttribute* CLevelLayerModel::GetAttributeForColumn(EObjectColumns column)
{
	switch (column)
	{
	case eObjectColumns_Name:
		return &Attributes::s_nameAttribute;
	case eObjectColumns_Layer:
		return &LevelModelsAttributes::s_layerNameAttribute;
	case eObjectColumns_Type:
		return &LevelModelsAttributes::s_objectTypeAttribute;
	case eObjectColumns_TypeDesc:
		return &LevelModelsAttributes::s_objectTypeDescAttribute;
	case eObjectColumns_Visible:
		return &LevelModelsAttributes::s_visibleAttribute;
	case eObjectColumns_Frozen:
		return &LevelModelsAttributes::s_frozenAttribute;
	case eObjectColumns_DefaultMaterial:
		return &LevelModelsAttributes::s_defaultMaterialAttribute;
	case eObjectColumns_CustomMaterial:
		return &LevelModelsAttributes::s_customMaterialAttribute;
	case eObjectColumns_Breakability:
		return &LevelModelsAttributes::s_breakableAttribute;
	case eObjectColumns_SmartObject:
		return &LevelModelsAttributes::s_smartObjectAttribute;
	case eObjectColumns_FlowGraph:
		return &LevelModelsAttributes::s_flowGraphAttribute;
	case eObjectColumns_Geometry:
		return &LevelModelsAttributes::s_geometryAttribute;
	case eObjectColumns_InstanceCount:
		return &LevelModelsAttributes::s_geometryInstancesAttribute;
	case eObjectColumns_LODCount:
		return &LevelModelsAttributes::s_lodCountAttribute;
	case eObjectColumns_MinSpec:
		return &LevelModelsAttributes::s_specAttribute;
	case eObjectColumns_AI_GroupId:
		return &LevelModelsAttributes::s_aiGroupIdAttribute;
	case eObjectColumns_LayerColor:
		return &LevelModelsAttributes::s_LayerColorAttribute;
	default:
		return nullptr;
	}
}

QVariant CLevelLayerModel::GetHeaderData(int section, Qt::Orientation orientation, int role)
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	auto pAttribute = GetAttributeForColumn((EObjectColumns)section);
	if (!pAttribute)
	{
		return QVariant();
	}

	if (role == Qt::DecorationRole)
	{
		if (section == eObjectColumns_Visible)
			return CryIcon("icons:General/Visibility_True.ico");
		if (section == eObjectColumns_Frozen)
			return CryIcon("icons:Navigation/Basics_Select_False.ico");
	}
	if (role == Qt::DisplayRole)
	{
		//For Visible and Frozen we use Icons instead
		if (section == eObjectColumns_Visible || section == eObjectColumns_Frozen || section == eObjectColumns_LayerColor)
		{
			return "";
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
		case eObjectColumns_Name:
		case eObjectColumns_Visible:
		case eObjectColumns_Frozen:
		case eObjectColumns_LayerColor:
			return "";
			break;
		default:
			return "Objects";
		}
	}
	return QVariant();
}

CLevelLayerModel::CLevelLayerModel(CObjectLayer* pLayer, QObject* parent)
	: QAbstractItemModel(parent)
	, m_pLayer(pLayer)
	, m_pTopLevelNotificationObj(nullptr)
{
	Connect();
	Rebuild();
}

CLevelLayerModel::~CLevelLayerModel()
{
	Disconnect();
}

void CLevelLayerModel::Connect()
{
	CObjectManager* pObjManager = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager());

	pObjManager->signalObjectChanged.Connect(this, &CLevelLayerModel::OnObjectEvent);
	pObjManager->signalSelectionChanged.Connect(this, &CLevelLayerModel::OnSelectionChanged);
	pObjManager->signalBeforeObjectsAttached.Connect(this, &CLevelLayerModel::OnBeforeObjectsAttached);
	pObjManager->signalObjectsAttached.Connect(this, &CLevelLayerModel::OnObjectsAttached);
	pObjManager->signalBeforeObjectsDetached.Connect(this, &CLevelLayerModel::OnBeforeObjectsDetached);
	pObjManager->signalObjectsDetached.Connect(this, &CLevelLayerModel::OnObjectsDetached);
	pObjManager->signalBeforeObjectsDeleted.Connect(this, &CLevelLayerModel::OnBeforeObjectsDeleted);
	pObjManager->signalObjectsDeleted.Connect(this, &CLevelLayerModel::OnObjectsDeleted);
}

void CLevelLayerModel::Disconnect()
{
	CObjectManager* pObjManager = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager());

	pObjManager->signalObjectChanged.DisconnectObject(this);
	pObjManager->signalSelectionChanged.DisconnectObject(this);
	pObjManager->signalBeforeObjectsAttached.DisconnectObject(this);
	pObjManager->signalObjectsAttached.DisconnectObject(this);
	pObjManager->signalBeforeObjectsDetached.DisconnectObject(this);
	pObjManager->signalObjectsDetached.DisconnectObject(this);
	pObjManager->signalBeforeObjectsDeleted.DisconnectObject(this);
	pObjManager->signalObjectsDeleted.DisconnectObject(this);
}

int CLevelLayerModel::rowCount(const QModelIndex& parent) const
{
	CBaseObject* pObject = ObjectFromIndex(parent);
	if (pObject)
	{
		return pObject->GetChildCount() + pObject->GetLinkedObjectCount();
	}
	if (m_pLayer)
	{
		return m_rootObjects.size();
	}
	return 0;
}

QVariant CLevelLayerModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	switch (role)
	{
	case (int)CLevelModel::Roles::TypeCheckRole:
		return (int)eLevelElement_Object;
	case (int)Roles::InternalPointerRole:
		return reinterpret_cast<intptr_t>(ObjectFromIndex(index));
	case Qt::DisplayRole:
	case Qt::ToolTipRole:
	case Qt::EditRole:
		{
			CBaseObject* pObject = ObjectFromIndex(index);
			if (pObject)
			{
				switch (index.column())
				{
				case eObjectColumns_Name:
					return (const char*)pObject->GetName();
				case eObjectColumns_Layer:
				{
					IObjectLayer* pLayer = pObject->GetLayer();
					// There are cases where layers might not be set for certain objects yet the model will try and display it
					// an example of this is when grouping. When grouping, the root object is not attached to any layer to avoid
					// spamming with meaningless events. When the full group hierarchy is constructed the layer is changed
					// and children layers are slowly set. At this point the object may have children than are on a null layer
					return pLayer ? pLayer->GetName().c_str() : "";
				}
				case eObjectColumns_Type:
					return Serialization::getEnumDescription<ObjectType>().label(pObject->GetType());
				case eObjectColumns_TypeDesc:
					return (const char*)pObject->GetTypeDescription();
				case eObjectColumns_DefaultMaterial:
					return GetMaterialName(pObject, false);
				case eObjectColumns_CustomMaterial:
					return GetMaterialName(pObject, true);
				case eObjectColumns_Breakability:
					return GetObjectBreakability(pObject);
				case eObjectColumns_SmartObject:
					return GetSmartObject(pObject);
				case eObjectColumns_FlowGraph:
					return GetFlowGraphNames(pObject);
				case eObjectColumns_Geometry:
					return GetGeometryFile(pObject);
				case eObjectColumns_InstanceCount:
					{
						int count = GetInstancesCount(pObject);
						return (count > 0) ? count : QVariant();
					}
				case eObjectColumns_LODCount:
					{
						int lods = GETLODNumber(pObject);
						return (lods > 0) ? lods : QVariant();
					}
				case eObjectColumns_MinSpec:
					switch (pObject->GetMinSpec())
					{
					case CONFIG_CUSTOM:
						return tr("Custom");
					case CONFIG_LOW_SPEC:
						return tr("Low");
					case CONFIG_MEDIUM_SPEC:
						return tr("Medium");
					case CONFIG_HIGH_SPEC:
						return tr("High");
					case CONFIG_VERYHIGH_SPEC:
						return tr("Very High");
					case CONFIG_DURANGO:
						return tr("Xbox One");
					case CONFIG_ORBIS:
						return tr("PS4");
					case CONFIG_DETAIL_SPEC:
						return tr("Detail");
					}
					return "Unkown";
				case eObjectColumns_AI_GroupId:
					{
						int group = GetAIGroupID(pObject);
						return (group > 0) ? group : QVariant();
					}
				}
			}
		}
		break;
	case Qt::DecorationRole:
		if (index.column() == eObjectColumns_Name)
		{
			CBaseObject* pObject = ObjectFromIndex(index);
			CBaseObject* pLinkedObject = pObject->GetLinkedTo();
			if (pLinkedObject)
				return CryIcon("icons:ObjectTypes/Link.ico").pixmap(16, 16, QIcon::Active, pObject->IsSelected() ? QIcon::On : QIcon::Off);

			CObjectClassDesc* const classDesc = pObject->GetClassDesc();
			if(classDesc == nullptr)
				return CryIcon("icons:General/Placeholder.ico").pixmap(16, 16, QIcon::Active, pObject->IsSelected() ? QIcon::On : QIcon::Off);

			QString category = classDesc->Category();

			//Special Case 1: Display Misc as Brush and Static Mesh as legacy
			//Alternative: Change Icon Mapping?
			if (category == "Misc")
				category = "Brush";
			else if (category == "Static Mesh Entity")
				category = "Legacy_Entities";
			
			//Special Case 2: Use specific Group Icon
			ObjectType objType = classDesc->GetObjectType();
			if (classDesc->GetObjectType() == ObjectType::OBJTYPE_GROUP)
			{
				category = "Group";
			}

			QString icon;
			if (category.isEmpty())
			{
				icon = "icons:General/Placeholder.ico";
			}
			else
			{
				icon = QString("icons:CreateEditor/Add_To_Scene_%1.ico").arg(category);
				icon.replace(" ", "_");
			}
			return CryIcon(icon);
		}
		if (index.column() == eObjectColumns_LayerColor)
		{
			COLORREF colorRef = m_pLayer->GetColor();
			return QColor::fromRgb(GetRValue(colorRef), GetGValue(colorRef), GetBValue(colorRef));
		}
	case Qt::CheckStateRole:
		{
			CBaseObject* pObject = ObjectFromIndex(index);
			switch (index.column())
			{
			case eObjectColumns_Visible:
				return pObject->IsVisible() ? Qt::Checked : Qt::Unchecked;
			case eObjectColumns_Frozen:
				return pObject->IsFrozen() ? Qt::Checked : Qt::Unchecked;
			}
			break;
		}
	case QAdvancedItemDelegate::s_IconOverrideRole:
		{
			CBaseObject* pObject = ObjectFromIndex(index);
			switch (index.column())
			{
			case eObjectColumns_Visible:
				if (pObject->IsVisible())
				{
					return CryIcon("icons:General/Visibility_True.ico");
				}
				else
					return CryIcon("icons:General/Visibility_False.ico");
			case eObjectColumns_Frozen:
				if (pObject->IsFrozen())
					return CryIcon("icons:General/editable_false.ico");
				else
					return CryIcon("icons:General/editable_true.ico");
			default:
				break;
			}
			break;
		}
	case QAdvancedItemDelegate::s_DrawRectOverrideRole:
		{
			switch (index.column())
			{
			case eObjectColumns_LayerColor:
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

QVariant CLevelLayerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return GetHeaderData(section, orientation, role);
}

Qt::ItemFlags CLevelLayerModel::flags(const QModelIndex& index) const
{
	//TODO : Make more fields editable (materials could be picked for instance)
	Qt::ItemFlags f = QAbstractItemModel::flags(index);
	if (index.isValid())
	{
		switch (index.column())
		{
		case eLayerColumns_Name:
			f |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
			break;
		case eLayerColumns_Visible:
		case eLayerColumns_Frozen:
			f |= Qt::ItemIsUserCheckable;
			break;
		default:
			f |= Qt::ItemIsDragEnabled;
			break;
		}
	}
	return f | Qt::ItemIsDropEnabled;
}

QModelIndex CLevelLayerModel::index(int row, int column, const QModelIndex& parent) const
{
	if (row < 0 || column < 0 || !m_pLayer)
	{
		return QModelIndex(); // invalid index
	}

	if (!parent.isValid() && row < m_rootObjects.size())
	{
		auto pObject = m_rootObjects[row];
		return createIndex(row, column, reinterpret_cast<quintptr>(pObject));
	}

	if (CBaseObject* pParent = ObjectFromIndex(parent))
	{
		// If the index is valid then it means we have a parent (or we are linked to an object)
		if (pParent->GetChildCount() && pParent->GetChildCount() > row)
		{
			return createIndex(row, column, reinterpret_cast<quintptr>(pParent->GetChild(row)));
		}
		else if (pParent->GetLinkedObjectCount())
		{
			// Must subtract row from child count to get linked object index
			int idx = row - pParent->GetChildCount();

			if (idx >= 0 && pParent->GetLinkedObjectCount() > idx)
			{
				return createIndex(row, column, reinterpret_cast<quintptr>(pParent->GetLinkedObject(idx)));
			}
		}
	}

	return QModelIndex(); // row not found
}

QModelIndex CLevelLayerModel::parent(const QModelIndex& index) const
{
	CBaseObject* pObject = ObjectFromIndex(index);
	if (pObject)
	{
		if (CBaseObject* pParent = pObject->GetParent())
		{
			return IndexFromObject(pParent);
		}
		else if (CBaseObject* pLinkedTo = pObject->GetLinkedTo())
		{
			// If the parent or any of the ancestors is also in the current layer, then we have a parent to display in the layer hierarchy.
			if (pLinkedTo->IsAnyLinkedAncestorInLayer(m_pLayer))
				return IndexFromObject(pLinkedTo);
		}
	}

	return QModelIndex(); // all objects are children of root
}

bool CLevelLayerModel::canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const
{
	//Objects can be dropped on layers, folders or other objects.
	//Then it is added to the parent of the drop target
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

	auto parentLayer = m_pLayer;
	if (!parentLayer)
	{
		return false;
	}

	CBaseObject* pTargetObject = ObjectFromIndex(parent);

	if (pTargetObject)
	{
		// Check if our target is invalid
		bool isInvalidTarget = std::any_of(objects.begin(), objects.end(), [&](CBaseObject* object)
		{
			// Cannot drag onto self
			if (object == pTargetObject)
				return true;
			
			// If not a group check if it's a valid link target
			if (!object->CanLinkTo(pTargetObject))
				return true;

			return false;
		});

		if (isInvalidTarget)
		{
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Invalid operation"));
			return false;
		}
	}
	else
	{
		// do not drop an object on a layer if it is already a child of it
		if (std::any_of(objects.begin(), objects.end(), [&](CBaseObject* object) { return object->GetLayer() == m_pLayer; }))
		{
			CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Invalid operation"));
			return false;
		}
	}

	CGroup* pAttachTo = nullptr;

	if (pTargetObject)
	{
		if (GetIEditorImpl()->IsCGroup(pTargetObject))
			pAttachTo = static_cast<CGroup*>(pTargetObject);
		if (CBaseObject* pGroup = pTargetObject->GetGroup())
			pAttachTo = static_cast<CGroup*>(pGroup);
	}

	if (pAttachTo && std::any_of(objects.begin(), objects.end(), [&](CBaseObject* object) { return object->GetGroup() != pAttachTo; }))
		CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Attach to %1").arg(QtUtil::ToQString(pAttachTo->GetName())));
	else if (pTargetObject && pTargetObject == pAttachTo)
	{
		CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Already contained by %1").arg(QtUtil::ToQString(pAttachTo->GetName())));
		return false;
	}
	else if (pTargetObject)
		CDragDropData::ShowDragText(qApp->widgetAt(QCursor::pos()), QString("Link to %1").arg(QtUtil::ToQString(pTargetObject->GetName())));

	switch (parentLayer->GetLayerType())
	{
	case eObjectLayerType_Terrain:
		// intentional fall through
	case eObjectLayerType_Layer:
		// layers cannot be dropped on other layers. So the parent has to be
		// determined and the layers need to be added there
		parentLayer = parentLayer->GetParent();
	// intentional fall through

	case eObjectLayerType_Folder:
	{
		// do not drop an layer on a layer if it is already a child of it
		if (std::any_of(layers.begin(), layers.end(), [&](CObjectLayer* layer) { return (parentLayer ? parentLayer->IsChildOf(layer) : false) || layer->GetParent() == parentLayer; }))
		{
			return false;
		}

		return QAbstractItemModel::canDropMimeData(pData, action, row, column, parent);
	}
	default:
		return false;
	}
}

bool CLevelLayerModel::dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent)
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

	auto parentLayer = m_pLayer;
	if (!parentLayer)
	{
		return false;
	}

	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	CBaseObject* pTargetObject = ObjectFromIndex(parent);

	if (pTargetObject)
	{
		CGroup* pAttachTo = nullptr;

		if (GetIEditorImpl()->IsCGroup(pTargetObject))
			pAttachTo = static_cast<CGroup*>(pTargetObject);
		else if (CBaseObject* pGroup = pTargetObject->GetGroup())
			pAttachTo = static_cast<CGroup*>(pGroup);

		// Keep only top-most parents when doing linking or grouping, children will follow
		std::vector<CBaseObject*> filtered;
		pObjectManager->FilterParents(objects, filtered);
		std::vector<CBaseObject*> toBeAdded;
		std::vector<CBaseObject*> toBeLinked;
		for (CBaseObject* pObject : filtered)
		{
			if (pAttachTo && pObject->GetGroup() != pAttachTo)
				toBeAdded.push_back(pObject);
			else
				toBeLinked.push_back(pObject);
		}

		GetIEditorImpl()->GetIUndoManager()->Begin();

		for (auto pObject : toBeAdded)
			pAttachTo->AddMember(pObject);
		
		if (toBeLinked.size())
			pObjectManager->Link(toBeLinked, pTargetObject);

		if (pAttachTo) // if dropping into a group, select the group
			pObjectManager->SelectObject(pTargetObject);

		GetIEditorImpl()->GetIUndoManager()->Accept("Level Explorer Move");

		return true;
	}

	switch (parentLayer->GetLayerType())
	{
	case eObjectLayerType_Folder:
	{
		CUndo undo("Move Layers");
		for (auto layer : layers)
		{
			if (!parentLayer->IsChildOf(layer) && layer->GetParent() != parentLayer)
			{
				parentLayer->AddChild(layer);
			}
		}

		return true;
	}
	case eObjectLayerType_Terrain:
		// intentional fall through
	case eObjectLayerType_Layer:
	{
		CUndo undo("Move Layers");
		// layers cannot be dropped on other layers. So the parent has to be
		// determined and the layers need to be added there
		parentLayer = parentLayer->GetParent();

		if (!parentLayer)
		{
			for (auto layer : layers)
			{
				layer->SetAsRootLayer();
			}
		}
		else
		{
			for (auto layer : layers)
			{
				if (!parentLayer->IsChildOf(layer) && layer->GetParent() != parentLayer)
				{
					parentLayer->AddChild(layer);
				}
			}
		}
		return true;
	}

	default:
		return false;
	}
}

QMimeData* CLevelLayerModel::mimeData(const QModelIndexList& indexes) const
{
	return LevelModelsUtil::GetDragDropData(indexes);
}

bool CLevelLayerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	CBaseObject* pObject = ObjectFromIndex(index);
	if (pObject)
	{
		switch (index.column())
		{
		case eObjectColumns_Name:
			if (role == Qt::EditRole)
			{
				string newName = QtUtil::ToString(value.toString());
				GetIEditorImpl()->GetIUndoManager()->Begin();
				// if change is not successful the user is thrown a message and the change fails
				if (!GetIEditorImpl()->GetObjectManager()->ChangeObjectName(pObject, newName.c_str()))
				{
					GetIEditorImpl()->GetIUndoManager()->Cancel();
					return false;
				}
				GetIEditorImpl()->GetIUndoManager()->Accept("Rename Object");
			}
			break;
		case eObjectColumns_Visible:
			if (role == Qt::CheckStateRole)
			{
				pObject->SetVisible(value == Qt::Checked);
			}
			break;
		case eObjectColumns_Frozen:
			if (role == Qt::CheckStateRole)
			{
				pObject->SetFrozen(value == Qt::Checked);
			}
			break;
		default:
			break;
		}
		QVector<int> roleVector(1, role);
		dataChanged(index, index, roleVector);
		return true;
	}
	return false;
}

Qt::DropActions CLevelLayerModel::supportedDragActions() const
{
	return Qt::MoveAction;
}

Qt::DropActions CLevelLayerModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList CLevelLayerModel::mimeTypes() const
{
	QStringList result;
	result << CDragDropData::GetMimeFormatForType("LayersAndObjects");
	return result;
}

CBaseObject* CLevelLayerModel::ObjectFromIndex(const QModelIndex& index) const
{
	if ((index.row() < 0) || (index.column() < 0))
	{
		return nullptr;
	}
	return static_cast<CBaseObject*>(index.internalPointer());
}

QModelIndex CLevelLayerModel::IndexFromObject(const CBaseObject* pObject) const
{
	if (pObject)
	{
		int row = RowFromObject(pObject);
		if (row > -1)
		{
			return createIndex(row, 0, reinterpret_cast<quintptr>(pObject));
		}
		else if (CBaseObject* pLinkedTo = pObject->GetLinkedTo())
		{
			for (auto i = 0; i < pLinkedTo->GetLinkedObjectCount(); ++i)
			{
				if (pLinkedTo->GetLinkedObject(i) == pObject)
					return createIndex(pLinkedTo->GetChildCount() + i, 0, reinterpret_cast<quintptr>(pObject));
			}
		}
		else if (CBaseObject* pParent = pObject->GetParent())
		{
			for (auto i = 0; i < pParent->GetChildCount(); ++i)
			{
				if (pParent->GetChild(i) == pObject)
					return createIndex(i, 0, reinterpret_cast<quintptr>(pObject));
			}
		}
	}
	return QModelIndex();
}

int CLevelLayerModel::RowFromObject(const CBaseObject* pObject) const
{
	if (!pObject)
	{
		return -1; // no object given
	}

	CBaseObjectsArray::const_iterator it = std::find(m_rootObjects.cbegin(), m_rootObjects.cend(), pObject);
	if (it != m_rootObjects.cend())
	{
		return std::distance(m_rootObjects.cbegin(), it);
	}
	return -1; // object not found
}

bool CLevelLayerModel::Filter(CBaseObject const& obj, void* pLayer)
{
	if (obj.CheckFlags(OBJFLAG_DELETED))
		return false;

	CBaseObject* pParent = obj.GetParent();
	if (pParent) // We only keep track of root objects internally
		return false;

	if (obj.GetLinkedTo() && obj.GetLinkedTo()->IsAnyLinkedAncestorInLayer(static_cast<IObjectLayer*>(pLayer)))
		return false;

	if (pLayer == 0 || obj.GetLayer() == pLayer)
		return true;

	return false;
}

CBaseObjectsArray CLevelLayerModel::GetLayerObjects() const
{
	CBaseObjectsArray result;
	if (m_pLayer)
	{
		GetIEditorImpl()->GetObjectManager()->GetObjects(result, BaseObjectFilterFunctor(&CLevelLayerModel::Filter, m_pLayer));
	}
	return result;
}

void CLevelLayerModel::Rebuild()
{
	if (!m_rootObjects.empty())
		Clear();

	auto objects = GetLayerObjects();

	if (!objects.empty())
	{
		beginInsertRows(QModelIndex(), 0, objects.size() - 1);

		m_rootObjects.reserve(objects.size());
		m_rootObjects = objects;

		for (auto& object : objects)
			UpdateCachedDataForObject(object);

		endInsertRows();
	}
}

void CLevelLayerModel::Clear()
{
	beginResetModel();

	m_rootObjects.clear();
	m_geometryCountMap.clear();
	m_flowGraphMap.clear();

	endResetModel();
}

void CLevelLayerModel::AddObject(CBaseObject* pObject)
{
	// always assume new object is a root object.
	m_rootObjects.push_back(pObject);
	UpdateCachedDataForObject(pObject);
}

void CLevelLayerModel::AddObjects(const std::vector<CBaseObject*>& objects)
{
	m_rootObjects.insert(m_rootObjects.end(), objects.cbegin(), objects.cend());
	if (!m_isRuningBatchProcess)
	{
		for (auto pObj : objects)
		{
			UpdateCachedDataForObject(pObj);
		}
	}
}

void CLevelLayerModel::RemoveObject(CBaseObject* pObject)
{
	int i = RowFromObject(pObject);
	if (i >= 0)
	{
		RemoveObjects(i);
	}
}

void CLevelLayerModel::RemoveObjects(int row, int count /* = 1 */)
{
	for (int i = 0; i < count; ++i)
	{
		auto pObject = m_rootObjects[i + row];
		m_flowGraphMap.erase(pObject);
		string geometryFile = GetGeometryFile(pObject);
		if (!geometryFile.empty())
		{
			--m_geometryCountMap[geometryFile];
		}
	}
	auto it = m_rootObjects.begin() + row;
	m_rootObjects.erase(it, it + count);
}

void CLevelLayerModel::RemoveObjects(const std::vector<CBaseObject*>& objects)
{
	for (auto pObj : objects)
	{
		m_flowGraphMap.erase(pObj);
		string geometryFile = GetGeometryFile(pObj);
		if (!geometryFile.empty())
		{
			--m_geometryCountMap[geometryFile];
		}
	}
	m_rootObjects.erase(std::remove_if(m_rootObjects.begin(), m_rootObjects.end(), [this, &objects](CBaseObject* pRootObj)
	{
		return std::any_of(objects.cbegin(), objects.cend(), [pRootObj](CBaseObject* pObj)
		{
			return pObj == pRootObj;
		});
	}), m_rootObjects.end());
}

void CLevelLayerModel::UpdateCachedDataForObject(CBaseObject* pObject)
{
	string flowGraphName = "";
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		std::vector<CHyperFlowGraph*> flowgraphs;
		CHyperFlowGraph* pEntityFG = nullptr;
		FlowGraphHelpers::FindGraphsForEntity(pEntity, flowgraphs, pEntityFG);

		for (CHyperFlowGraph* pFlowGraph : flowgraphs)
		{
			if (!flowGraphName.empty())
			{
				flowGraphName += ",";
			}
			string name;
			FlowGraphHelpers::GetHumanName(pFlowGraph, name);
			flowGraphName += name;
			if (pFlowGraph == pEntityFG)
			{
				flowGraphName += "*";  // A special mark for an entity flow graph
			}
		}
	}

	if (!flowGraphName.empty())
	{
		m_flowGraphMap[pObject] = flowGraphName;
	}

	string geometryFile = GetGeometryFile(pObject);
	if (!geometryFile.empty())
	{
		++m_geometryCountMap[geometryFile];
	}
}

void CLevelLayerModel::OnObjectEvent(CObjectEvent& eventObj)
{
	CBaseObject* pObject = eventObj.m_pObj;

	if (!pObject)
	{
		return;
	}
	else if (pObject->GetLayer() != m_pLayer && !(eventObj.m_type == OBJECT_ON_LAYERCHANGE || eventObj.m_type == OBJECT_ON_PRELINKED || 
		eventObj.m_type == OBJECT_ON_LINKED || eventObj.m_type == OBJECT_ON_PREUNLINKED || eventObj.m_type == OBJECT_ON_UNLINKED))
	{
		return;
	}

	switch (eventObj.m_type)
	{
		case OBJECT_ON_ADD:
		{
			// we need to be slightly careful here because objects get named before being added, so we might try to access an invalid index.
			int irow = RowFromObject(pObject);

			// guard against setLayer/add combo at object creation
			if (irow == -1 && Filter(*pObject, m_pLayer))
			{
				irow = m_rootObjects.size();

				if (m_isRuningBatchProcess)
				{
					AddObject(pObject);
					return;
				}
				beginInsertRows(QModelIndex(), irow, irow);
				AddObject(pObject);
				endInsertRows();
			}
		}
		break;
		case OBJECT_ON_PRELINKED:
		{
			CObjectPreLinkEvent& evt = static_cast<CObjectPreLinkEvent&>(eventObj);
			CBaseObject* pLinkedTo = evt.m_pLinkedTo;

			// If the object we're linking to and all it's ancestors are not contained in this layer, then we'll reset the model later on, when handling
			// OBJECT_ON_LINKED events. We perform a reset in those cases because linking between layers is the only event that will modify several layer 
			// models ***at the same time*** which means we would need to *begin* modification of several models before calling *end*, this is unsupported
			// by qt and our mounting/merging proxy models. So what we do is we defer the processing of the events until the object is linked.
			if (m_isRuningBatchProcess || pObject->GetLayer() != m_pLayer || !pLinkedTo->IsLinkedAncestryInLayer(m_pLayer))
				return;

			QModelIndex objectIdx = IndexFromObject(pObject);
			QModelIndex linkedToIdx = IndexFromObject(pLinkedTo);

			beginMoveRows(objectIdx.parent(), objectIdx.row(), objectIdx.row(), linkedToIdx, rowCount(linkedToIdx));
		}
		break;
		case OBJECT_ON_LINKED:
		{
			CBaseObject* pLinkedTo = pObject->GetLinkedTo();

			if (pObject->GetLayer() != m_pLayer || !pLinkedTo->IsLinkedAncestryInLayer(m_pLayer))
			{
				// Make sure that there is at least a single point in the new hierarchy that belongs to this layer before resetting
				if (!pObject->IsAnyLinkedAncestorInLayer(m_pLayer))
					return;

				// We perform a reset in those cases because linking between layers is the only event that will modify several layer models ***at the same time*** 
				// which means we would need to *begin* modification of several models before calling *end*, this is unsupported by qt and our mounting/merging 
				// proxy models. So what we do is we defer the processing of the events until the object is linked.
				if (m_isRuningBatchProcess)
				{
					OnLink(pObject);
					return;
				}

				beginResetModel();
				OnLink(pObject);
				endResetModel();

				return;
			}

			RemoveObject(pObject);
			if (!m_isRuningBatchProcess)
			{
				endMoveRows();
			}
		}
		break;
		case OBJECT_ON_LAYERCHANGE:
		{
			CObjectLayerChangeEvent& evt = static_cast<CObjectLayerChangeEvent&>(eventObj);

			// We need to remove from old layer and add to new layer
			if (pObject->GetLayer() == m_pLayer && Filter(*pObject, m_pLayer))
			{
				int irow = m_rootObjects.size();

				beginInsertRows(QModelIndex(), irow, irow);
				AddObject(pObject);
				endInsertRows();
				break;
			}
			else if (evt.m_poldLayer == m_pLayer)
			{
				int irow = RowFromObject(pObject);
				if (irow == -1)
				{
					return; //Object must be in another layer
				}

				beginRemoveRows(QModelIndex(), irow, irow);
				RemoveObject(pObject);
				endRemoveRows();
			}
			break;
		}
		case OBJECT_ON_PREUNLINKED:
		{
			CBaseObject* pLinkedTo = pObject->GetLinkedTo();
			// If the object we're unlinking and all it's ancestors are not contained in this layer, then we'll reset the model later on, when handling
			// OBJECT_ON_UNLINKED events. We perform a reset in those cases because unlinking between layers is the only event that will modify several layer 
			// models ***at the same time*** which means we would need to *begin* modification of several models before calling *end*, this is unsupported
			// by qt and our mounting/merging proxy models. So what we do is we defer the processing of the events until the object is linked.
			if (m_isRuningBatchProcess || pObject->GetLayer() != m_pLayer || !pLinkedTo->IsLinkedAncestryInLayer(m_pLayer))
				return;

			QModelIndex objectIdx = IndexFromObject(pObject);
			beginMoveRows(objectIdx.parent(), objectIdx.row(), objectIdx.row(), QModelIndex(), m_rootObjects.size());
		}
		break;
		case OBJECT_ON_UNLINKED:
		{
			CBaseObject* pLinkedTo = static_cast<CObjectUnLinkEvent&>(eventObj).m_pLinkedTo;

			if (pObject->GetLayer() != m_pLayer || !pLinkedTo->IsLinkedAncestryInLayer(m_pLayer))
			{
				// Make sure that there is at least a single point in the new hierarchy that belongs to this layer before resetting
				if (pObject->GetLayer() != m_pLayer && !pLinkedTo->IsAnyLinkedAncestorInLayer(m_pLayer))
					return;

				// We perform a reset in this case because unlinking between layers is the only event that will modify several layer models ***at the same time*** 
				// which means we would need to *begin* modification of several models before calling *end*, this is unsupported by qt and our mounting/merging 
				// proxy models. So what we do is we defer the processing of the events until the object is unlinked.
				if (m_isRuningBatchProcess)
				{
					OnUnLink(pObject);
					return;
				}

				beginResetModel();
				OnUnLink(pObject);
				endResetModel();
				return;
			}

			AddObject(pObject);
			if (!m_isRuningBatchProcess)
			{
				endMoveRows();
			}
		}
		break;
		case OBJECT_ON_RENAME:
		{
			// we need to be slightly careful here because objects get named before being added, so we might try to access an invalid index.
			QVector<int> updateRoles(1, Qt::DisplayRole);
			NotifyUpdateObject(pObject, updateRoles);
		}
		break;

		case OBJECT_ON_SELECT:
		case OBJECT_ON_UNSELECT:
		{
			// If we're currently deleting this object (or an ancestor of this object) then ignore
			if (m_pTopLevelNotificationObj == pObject || pObject->IsChildOf(m_pTopLevelNotificationObj))
				break;

			QVector<int> updateRoles(1, Qt::DecorationRole);
			NotifyUpdateObject(pObject, updateRoles);
		}
		break;
		default:
			break;
	}
}

void CLevelLayerModel::OnBeforeObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform)
{
	if (pParent->GetLayer() != m_pLayer || m_isRuningBatchProcess)
	{
		return;
	}

	if (objects.size() > 1)
	{
		beginResetModel();
		return;
	}

	int row = RowFromObject(objects[0]);
	if (row == -1)
	{
		return;
	}

	IObjectLayer* pFutureLayer = pParent->GetLayer();
	CRY_ASSERT_MESSAGE(pFutureLayer == m_pLayer, "Layer Model: Attaching children between layers is unsupported");

	QModelIndex parentIdx = IndexFromObject(pParent);
	int parentRow = pParent->GetChildCount();

	beginMoveRows(QModelIndex(), row, row, parentIdx, parentRow);
}

void CLevelLayerModel::OnObjectsAttached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)
{
	if (pParent->GetLayer() != m_pLayer)
	{
		return;
	}

	RemoveObjects(objects);

	if (m_isRuningBatchProcess)
	{
		return;
	}
	else if (objects.size() > 1)
	{
		endResetModel();
		return;
	}
	
	CRY_ASSERT_MESSAGE(objects[0]->GetParent()->GetLayer() == m_pLayer, "Layer Model: Attaching children between layers is unsupported");

	endMoveRows();
}

void CLevelLayerModel::OnBeforeObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform)
{
	if (pParent->GetLayer() != m_pLayer || m_isRuningBatchProcess)
	{
		return;
	}

	if (objects.size() > 1)
	{
		beginResetModel();
		return;
	}

	const QModelIndex index = IndexFromObject(objects[0]);
	if (index.row() == -1)
	{
		return;
	}

	QModelIndex parentIdx = IndexFromObject(pParent);
	beginMoveRows(parentIdx, index.row(), index.row(), QModelIndex(), m_rootObjects.size());
}

void CLevelLayerModel::OnObjectsDetached(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)
{
	if (pParent->GetLayer() != m_pLayer)
	{
		return;
	}

	AddObjects(objects);

	if (m_isRuningBatchProcess)
	{
		return;
	}
	else if (objects.size() > 1)
	{
		endResetModel();
		return;
	}

	endMoveRows();
}

void CLevelLayerModel::OnBeforeObjectsDeleted(const CObjectLayer& layer, const std::vector<CBaseObject*>& objects)
{
	if (m_isRuningBatchProcess || &layer != m_pLayer)
	{
		return;
	}

	if (objects.size() > 1)
	{
		beginResetModel();
		return;
	}

	int row = RowFromObject(objects[0]);
	if (row == -1)
	{
		return; // Object must be in another layer or must be an untracked child object
	}

	m_pTopLevelNotificationObj = objects[0];
	beginRemoveRows(QModelIndex(), row, row);
}

void CLevelLayerModel::OnObjectsDeleted(const CObjectLayer& layer, const std::vector<CBaseObject*>& objects)
{
	if (&layer != m_pLayer)
	{
		return;
	}

	if (m_isRuningBatchProcess || objects.size() > 1)
	{
		RemoveObjects(objects);
		if (!m_isRuningBatchProcess)
		{
			endResetModel();
		}
		return;
	}

	int row = RowFromObject(objects[0]);
	if (row == -1)
	{
		return; // Object must be in another layer or must be an untracked child object
	}

	RemoveObject(objects[0]);
	endRemoveRows();
	m_pTopLevelNotificationObj = nullptr;
}

void CLevelLayerModel::OnSelectionChanged()
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();

	for(int i = 0, count = pSelection->GetCount(); i < count; ++i)
	{
		CBaseObject* pObject = pSelection->GetObject(i);
		if (pObject->GetLayer() != m_pLayer)
			continue;

		// If we're currently deleting this object (or an ancestor of this object) then ignore
		if (m_pTopLevelNotificationObj == pObject || pObject->IsChildOf(m_pTopLevelNotificationObj))
			continue;

		QVector<int> updateRoles(1, Qt::DecorationRole);
		NotifyUpdateObject(pObject, updateRoles);
	}
}

void CLevelLayerModel::OnLink(CBaseObject* pObject)
{
	// Make sure to remove any objects that are currently in the model that will eventually end up somewhere in our linked object's hierarchy
	int row = RowFromObject(pObject);
	if (row > -1)
	{
		CBaseObject* pLinkedObject = pObject->GetLinkedTo();
		while (pLinkedObject)
		{
			if (pLinkedObject && pLinkedObject->GetLayer() == m_pLayer)
			{
				RemoveObject(pObject);
				return;
			}
			pLinkedObject = pLinkedObject->GetLinkedTo();
		}
	}

	if (pObject->GetLayer() != m_pLayer && pObject->GetLinkedTo()->GetLayer() != m_pLayer)
		return;
	
	for (auto i = 0; i < pObject->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObject->GetLinkedObject(i);
		if (pLinkedObject->GetLayer() == m_pLayer)
			RemoveObject(pLinkedObject);
		else
			OnLink(pLinkedObject);
	}
}

void CLevelLayerModel::OnUnLink(CBaseObject* pObject)
{
	// Insert children of this object that belong in the layer that were removed when removing it's parent
	if (pObject->GetLayer() == m_pLayer)
	{
		const int row = RowFromObject(pObject);
		if (row < 0)
		{
			AddObject(pObject);
		}
		return;
	}

	for (auto i = 0; i < pObject->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObject->GetLinkedObject(i);

		if (pLinkedObject->GetLayer() == m_pLayer)
		{
			const int row = RowFromObject(pLinkedObject);
			if (row < 0)
			{
				AddObject(pLinkedObject);
			}
		}
		else
		{
			OnUnLink(pLinkedObject);
		}
	}
}

const char* CLevelLayerModel::GetMaterialName(CBaseObject* pObject, bool bUseCustomMaterial) const
{
	if (bUseCustomMaterial)
	{
		// Get material assigned by the user
		CMaterial* pMtl = (CMaterial*)pObject->GetMaterial();
		if (pMtl)
		{
			return pMtl->GetName();
		}
	}
	else
	{
		// Get default material stored in CGF file
		if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
		{
			CBrushObject* pBrush = (CBrushObject*)pObject;

			if (pBrush)
			{
				IStatObj* pStatObj = pBrush->GetIStatObj();
				if (pStatObj)
				{
					IMaterial* pMaterial = pStatObj->GetMaterial();
					if (pMaterial)
					{
						return pMaterial->GetName();
					}
				}
			}
		}
		else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
		{
			CEntityObject* pEntity = (CEntityObject*)pObject;
			if (pEntity->GetProperties())
			{
				IVariable* pVar = pEntity->GetProperties()->FindVariable("ParticleEffect");
				if (pVar)
				{
					string effect;
					pVar->Get(effect);
					return effect;
				}
			}
		}
	}
	return "";
}

const char* CLevelLayerModel::GetObjectBreakability(CBaseObject* pObject) const
{
	CMaterial* pMaterial = (CMaterial*)pObject->GetMaterial();
	if (!pMaterial)
	{
		return "";
	}
	std::set<string> breakabilityTypes;
	GetMaterialBreakability(&breakabilityTypes, pMaterial);

	string result;
	{
		std::set<string>::iterator it;
		for (it = breakabilityTypes.begin(); it != breakabilityTypes.end(); ++it)
		{
			if (!result.IsEmpty())
			{
				result += ", ";
			}
			result += *it;
		}
	}
	return result;
}

void CLevelLayerModel::GetMaterialBreakability(std::set<string>* breakTypes, CMaterial* pMaterial) const
{
	if (pMaterial)
	{
		const string& surfaceTypeName = pMaterial->GetSurfaceTypeName();
		ISurfaceTypeManager* pSurfaceManager = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager();
		if (pSurfaceManager)
		{
			ISurfaceType* pSurfaceType = pSurfaceManager->GetSurfaceTypeByName(surfaceTypeName);
			if (pSurfaceType && pSurfaceType->GetBreakability() != 0)
			{
				breakTypes->insert(pSurfaceType->GetType());
			}

			int subMaterialCount = pMaterial->GetSubMaterialCount();
			for (int i = 0; i < subMaterialCount; ++i)
			{
				CMaterial* pChild = pMaterial->GetSubMaterial(i);
				if (pChild)
				{
					GetMaterialBreakability(breakTypes, pChild);
				}
			}
		}
	}
}

const char* CLevelLayerModel::GetSmartObject(CBaseObject* pObject) const
{
	if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = (CEntityObject*)pObject;
		return (const char*)pEntity->GetSmartObjectClass();
	}
	return "";
}

const char* CLevelLayerModel::GetFlowGraphNames(CBaseObject* pObject) const
{
	const auto itr = m_flowGraphMap.find(pObject);
	if (itr != m_flowGraphMap.end())
	{
		return itr->second;
	}
	return "";
}

const char* CLevelLayerModel::GetGeometryFile(CBaseObject* pObject) const
{
	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pObj = (CBrushObject*)pObject;
		return pObj->GetGeometryFile().c_str();
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		CGeomEntity* pObj = (CGeomEntity*)pObject;
		return pObj->GetGeometryFile().c_str();

	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		IRenderNode* pEngineNode = pEntity->GetEngineNode();

		if (pEngineNode)
		{
			IStatObj* pEntityStatObj = pEngineNode->GetEntityStatObj();
			ICharacterInstance* pEntityCharacter = pEngineNode->GetEntityCharacter(0);
			if (pEntityStatObj)
			{
				return pEntityStatObj->GetFilePath();
			}
			if (pEntityCharacter)
			{
				return pEntityCharacter->GetFilePath();
			}
			if (pEntity->GetProperties())
			{
				IVariable* pVar = pEntity->GetProperties()->FindVariable("Model");
				if (pVar)
				{
					string result;
					pVar->Get(result);
					return result.c_str();
				}
			}
		}
	}
	return "";
}

uint CLevelLayerModel::GetInstancesCount(CBaseObject* pObject) const
{
	if (pObject)
	{
		string geometryFile = GetGeometryFile(pObject);
		if (!geometryFile.empty())
		{
			return stl::find_in_map(m_geometryCountMap, geometryFile, 0);
		}
	}
	return 0;
}

uint CLevelLayerModel::GETLODNumber(CBaseObject* pObject) const
{
	IStatObj::SStatistics stats;

	if (pObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		CBrushObject* pBrush = (CBrushObject*)pObject;

		if (pBrush)
		{
			IStatObj* pStatObj = pBrush->GetIStatObj();
			if (pStatObj)
			{
				pStatObj->GetStatistics(stats);
				return stats.nLods;
			}
		}
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CGeomEntity)))
	{
		CGeomEntity* pGeomEntity = (CGeomEntity*)pObject;
		IRenderNode* pEngineNode = pGeomEntity->GetEngineNode();
		if (pEngineNode)
		{
			IStatObj* pEntityStatObj = pEngineNode->GetEntityStatObj();
			if (pEntityStatObj)
			{
				pEntityStatObj->GetStatistics(stats);
				return stats.nLods;
			}
		}
	}
	else if (pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = static_cast<CEntityObject*>(pObject);
		IRenderNode* pEngineNode = pEntity->GetEngineNode();
		if (pEngineNode)
		{
			IStatObj* pEntityStatObj = pEngineNode->GetEntityStatObj();
			if (pEntityStatObj)
			{
				pEntityStatObj->GetStatistics(stats);
				return stats.nLods;
			}

			ICharacterInstance* pEntityCharacter = pEngineNode->GetEntityCharacter(0);
			if (pEntityCharacter)
			{
				IDefaultSkeleton& defaultSkeleton = pEntityCharacter->GetIDefaultSkeleton();
				{
					uint32 numInstances = gEnv->pCharacterManager->GetNumInstancesPerModel(defaultSkeleton);
					if (numInstances > 0)
					{
						ICharacterInstance* pCharInstance = gEnv->pCharacterManager->GetICharInstanceFromModel(defaultSkeleton, 0);
						if (pCharInstance)
						{
							if (ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose())
							{
								IDefaultSkeleton& defaultSkeleton = pCharInstance->GetIDefaultSkeleton();

								// check StatObj attachments
								uint32 numJoints = defaultSkeleton.GetJointCount();
								for (uint32 i = 0; i < numJoints; ++i)
								{
									IStatObj* pStatObj = (IStatObj*)pSkeletonPose->GetStatObjOnJoint(i);
									if (pStatObj)
									{
										pStatObj->GetStatistics(stats);
										return stats.nLods;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return 0;
}

int CLevelLayerModel::GetAIGroupID(CBaseObject* pObject) const
{
	if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		CEntityObject* pEntity = (CEntityObject*)pObject;
		CVarBlock* pProperties = pEntity->GetProperties2();
		if (pProperties)
		{
			IVariable* pVariable = pProperties->FindVariable("groupid");
			if (pVariable)
			{
				int groupid;
				pVariable->Get(groupid);
				return groupid;
			}
		}
	}
	return -1;
}

void CLevelLayerModel::NotifyUpdateObject(CBaseObject* pObject, const QVector<int>& updateRoles)
{
	const QModelIndex leftIndex = IndexFromObject(pObject);
	if (!leftIndex.isValid())
	{
		return;
	}

	const QModelIndex rightIndex = leftIndex.sibling(leftIndex.row(), columnCount() - 1);
	dataChanged(leftIndex, rightIndex, updateRoles);
}

bool CLevelLayerModel::IsRelatedToTopLevelObject(CBaseObject* pObject) const 
{
	return (m_pTopLevelNotificationObj && (pObject == m_pTopLevelNotificationObj || pObject->IsChildOf(m_pTopLevelNotificationObj)));
}

void CLevelLayerModel::StartBatchProcess()
{
	assert(!m_isRuningBatchProcess);
	m_isRuningBatchProcess = true;
	beginResetModel();
}

void CLevelLayerModel::FinishBatchProcess()
{
	endResetModel();
	assert(m_isRuningBatchProcess);
	m_isRuningBatchProcess = false;
}

