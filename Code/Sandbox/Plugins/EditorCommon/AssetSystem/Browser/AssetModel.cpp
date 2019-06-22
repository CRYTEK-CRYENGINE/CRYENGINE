// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetModel.h"

#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetType.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/Browser/AssetThumbnailsLoader.h"

#include "DragDrop.h"
#include "PathUtils.h"
#include "QAdvancedItemDelegate.h"
#include "QThumbnailView.h"
#include "QtUtil.h"

QStringList CAssetModel::GetAssetTypesStrList()
{
	QStringList list;

	for (auto& type : CAssetManager::GetInstance()->GetAssetTypes())
	{
		list.append(QString(type->GetUiTypeName()));
	}
	
	return list;
}

QStringList CAssetModel::GetStatusesList()
{
	//Note: this should add the version control statuses too.
	QStringList list;
	list.append(tr("Open"));
	list.append(tr("Modified"));
	return list;
}

std::vector<int> CAssetModel::GetColumnsByAssetTypes(const std::vector<CAssetType*>& assetTypes)
{
	std::vector<int> columns;

	std::set<const CItemModelAttribute*> attributes;
	for (const CAssetType* pType : assetTypes)
	{
		const std::vector<CItemModelAttribute*> details = pType->GetDetails();
		attributes.insert(details.cbegin(), details.cend());
	}

	const int columnCount = CAssetModel::GetColumnCount();
	columns.reserve(columnCount);
	for (int i = 0; i < columnCount; ++i)
	{
		const CItemModelAttribute* const pAttribute = CAssetModel::GetColumnAttribute(i);
		if (i < eAssetColumns_Details || attributes.find(pAttribute) != attributes.end())
		{
			columns.push_back(i);
		}
	}
	return columns;
}

namespace AssetModelAttributes
{

CItemModelAttributeEnumFunc s_AssetTypeAttribute("Type", &CAssetModel::GetAssetTypesStrList);
CItemModelAttributeEnumFunc s_AssetStatusAttribute("Status", &CAssetModel::GetStatusesList);
CItemModelAttribute s_AssetFolderAttribute("Folder", &Attributes::s_stringAttributeType, CItemModelAttribute::StartHidden, false, "");
CItemModelAttribute s_AssetUidAttribute("Unique Id", &Attributes::s_stringAttributeType, CItemModelAttribute::StartHidden);
CItemModelAttribute s_FilterStringAttribute("_filter_string_", &Attributes::s_stringAttributeType, CItemModelAttribute::AlwaysHidden, false, "");

}

//////////////////////////////////////////////////////////////////////////

void CAssetModel::AddPredefinedComputedColumns()
{
	static CItemModelAttribute fileExtensionAttribute("File extension", &Attributes::s_stringAttributeType, CItemModelAttribute::StartHidden);
	AddColumn(&fileExtensionAttribute, [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)
	{
		CRY_ASSERT(&fileExtensionAttribute == pAttribute);
		return role != Qt::DisplayRole ? QVariant() : QVariant(QtUtil::ToQString(pAsset->GetType()->GetFileExtension()));
	});

	static CItemModelAttribute filesCountAttribute("Files count", &Attributes::s_intAttributeType, CItemModelAttribute::StartHidden);
	AddColumn(&filesCountAttribute, [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)
	{
		CRY_ASSERT(&filesCountAttribute == pAttribute);
		return role != Qt::DisplayRole ? QVariant() : QVariant(pAsset->GetFilesCount());
	});
}


void CAssetModel::BuildDetailAttributes()
{
	AddPredefinedComputedColumns();

	for (CAssetType* pAssetType : CAssetManager::GetInstance()->GetAssetTypes())
	{
		static const auto getDetailValue = [](const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)
		{
			if (role != Qt::DisplayRole)
			{
				return QVariant();
			}
			const CAssetType* const pAssetType = pAsset->GetType();
			return  pAssetType->GetDetailValue(pAsset, pAttribute);
		};

		for (CItemModelAttribute* pAttribute : pAssetType->GetDetails())
		{
			if (!pAttribute)
			{
				CryWarning(VALIDATOR_MODULE_ASSETS, VALIDATOR_ERROR, "Asset type '%s' has null attribute", pAssetType->GetTypeName());
				continue;
			}

			auto otherIt = std::find_if(m_detailAttributes.begin(), m_detailAttributes.end(), [pAttribute](const SDetailAttribute& attrib)
			{
				return attrib.pAttribute == pAttribute;
			});
			if (otherIt != m_detailAttributes.end())
			{
				otherIt->assetTypes.emplace_back(pAssetType);
			}
			else
			{
				SDetailAttribute attrib;
				attrib.pAttribute = pAttribute;
				attrib.getValueFn = getDetailValue;
				attrib.assetTypes.emplace_back(pAssetType);

				m_detailAttributes.push_back(attrib);
			}
		}
	}
}

CAssetModel* CAssetModel::GetInstance()
{
	return CAssetManager::GetInstance()->GetAssetModel();
}

CAssetModel::CAssetModel(QObject* parent /*= nullptr*/)
	: QAbstractItemModel(parent)
	, m_favHelper("Asset Browser", eAssetColumns_Favorite)
{
}

void CAssetModel::Init()
{
	CAutoRegisterColumn::RegisterAll();
	BuildDetailAttributes();

	CAssetManager::GetInstance()->signalBeforeAssetsReset.Connect(this, &CAssetModel::PreReset);
	CAssetManager::GetInstance()->signalAfterAssetsReset.Connect(this, &CAssetModel::PostReset);

	CAssetManager::GetInstance()->signalBeforeAssetsInserted.Connect(this, &CAssetModel::PreInsert);
	CAssetManager::GetInstance()->signalAfterAssetsInserted.Connect(this, &CAssetModel::PostInsert);

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.Connect(this, &CAssetModel::PreRemove);
	CAssetManager::GetInstance()->signalAfterAssetsRemoved.Connect(this, &CAssetModel::PostRemove);

	CAssetManager::GetInstance()->signalAssetChanged.Connect(this, &CAssetModel::OnAssetChanged);

	CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded.Connect(this, &CAssetModel::OnAssetThumbnailLoaded);
}

CAssetModel::~CAssetModel()
{
	CAssetManager::GetInstance()->signalBeforeAssetsReset.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsReset.DisconnectObject(this);

	CAssetManager::GetInstance()->signalBeforeAssetsInserted.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsInserted.DisconnectObject(this);

	CAssetManager::GetInstance()->signalBeforeAssetsRemoved.DisconnectObject(this);
	CAssetManager::GetInstance()->signalAfterAssetsRemoved.DisconnectObject(this);

	CAssetManager::GetInstance()->signalAssetChanged.DisconnectObject(this);

	CAssetThumbnailsLoader::GetInstance().signalAssetThumbnailLoaded.DisconnectObject(this);
}

const CItemModelAttribute* CAssetModel::GetColumnAttribute(int column)
{
	switch (column)
	{
	case eAssetColumns_Name:
		return &Attributes::s_nameAttribute;
	case eAssetColumns_Type:
		return &AssetModelAttributes::s_AssetTypeAttribute;
	case eAssetColumns_Status:
		return &AssetModelAttributes::s_AssetStatusAttribute;
	case eAssetColumns_Folder:
		return &AssetModelAttributes::s_AssetFolderAttribute;
	case eAssetColumns_Uid:
		return &AssetModelAttributes::s_AssetUidAttribute;
	case eAssetColumns_Favorite:
		return &Attributes::s_favoriteAttribute;
	case eAssetColumns_Thumbnail:
		return &Attributes::s_thumbnailAttribute;
	case eAssetColumns_FilterString:
		return &AssetModelAttributes::s_FilterStringAttribute;
	default:
		break;
	}

	const int detailColumn = column - (int)eAssetColumns_Details;
	if (detailColumn >= 0 && detailColumn < (int)GetInstance()->m_detailAttributes.size())
	{
		return GetInstance()->m_detailAttributes[detailColumn].pAttribute;
	}

	return nullptr;
}

bool CAssetModel::hasChildren(const QModelIndex& parent /*= QModelIndex()*/) const
{
	return parent == QModelIndex();
}

int CAssetModel::rowCount(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return CAssetManager::GetInstance()->GetAssetsCount();
	else
		return 0;
}

int CAssetModel::GetColumnCount()
{
	return eAssetColumns_Details + GetInstance()->m_detailAttributes.size();
}

bool CAssetModel::AddColumn(const CItemModelAttribute* pAttribute, std::function<QVariant(const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)> getValueFn)
{
	if (std::any_of(m_detailAttributes.begin(), m_detailAttributes.end(), [pAttribute](const SDetailAttribute& attrib)
	{
		return attrib.getValueFn == nullptr;
	}))
	{
		CRY_ASSERT_MESSAGE(false, "Adding computed columns after CAssetModel::Init() is not allowed.");
		return false;
	}

	if (!getValueFn)
	{
		return false;
	}

	auto otherIt = std::find_if(m_detailAttributes.begin(), m_detailAttributes.end(), [pAttribute](const SDetailAttribute& attrib)
	{
		return attrib.pAttribute == pAttribute;
	});
	if (otherIt != m_detailAttributes.end())
	{
		CRY_ASSERT_MESSAGE(false, "The column is already registered: %s", pAttribute->GetName());
		return false;
	}

	SDetailAttribute attrib;
	attrib.pAttribute = pAttribute;
	attrib.getValueFn = std::move(getValueFn);

	m_detailAttributes.push_back(attrib);
	return true;
}

void CAssetModel::AddThumbnailIconProvider(const string& name, std::function<QIcon(const CAsset*)> iconProviderFunc)
{
	RemoveThumbnailIconProvider(name);
	m_tumbnailIconProviders.push_back(std::make_pair(name, std::move(iconProviderFunc)));
}

void CAssetModel::RemoveThumbnailIconProvider(const string& name)
{
	auto it = std::find_if(m_tumbnailIconProviders.cbegin(), m_tumbnailIconProviders.cend(), [&name](const auto& pair)
	{
		return pair.first == name;
	});
	if (it != m_tumbnailIconProviders.cend())
	{
		m_tumbnailIconProviders.erase(it);
	}
}

int CAssetModel::columnCount(const QModelIndex& parent) const
{
	return GetColumnCount();
}

bool CAssetModel::IsAsset(const QModelIndex& index)
{
	bool ok = false;
	return index.data((int)CAssetModel::Roles::TypeCheckRole).toUInt(&ok) == eAssetModelRow_Asset && ok;
}

CAsset* CAssetModel::ToAsset(const QModelIndex& index)
{
	return reinterpret_cast<CAsset*>(index.data((int)CAssetModel::Roles::InternalPointerRole).value<intptr_t>());
}

CAsset* CAssetModel::ToAssetInternal(const QModelIndex& index)
{
	return static_cast<CAsset*>(index.internalPointer());
}

const CAsset* CAssetModel::ToAssetInternal(const QModelIndex& index) const
{
	return static_cast<const CAsset*>(index.internalPointer());
}

QModelIndex CAssetModel::ToIndex(const CAsset& asset, int col) const
{
	const std::vector<CAssetPtr>& assets = CAssetManager::GetInstance()->m_assets;

	// TODO: Get rid of search.
	auto it = std::find_if(assets.begin(), assets.end(), [&asset](const CAssetPtr& other) { return other.get() == &asset; });
	const int row = (int)std::distance(assets.begin(), it);

	return createIndex(row, col, (quintptr)&asset);
}

QVariant CAssetModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	const CAsset* pAsset = ToAssetInternal(index);
	
	switch (role)
	{
	case (int)Roles::InternalPointerRole:
		return reinterpret_cast<intptr_t>(pAsset);
	case (int)Roles::TypeCheckRole:
		return (int)eAssetModelRow_Asset;
	case FavoritesHelper::s_FavoritesIdRole:
	{
		return QString(pAsset->GetGUID().ToString());
	}
	default:
		break;
	}

	switch (index.column())
	{
	case eAssetColumns_Type:
		if(role == Qt::DisplayRole)
			return QString(pAsset->GetType()->GetUiTypeName());
		break;
	case eAssetColumns_Name:
	case eAssetColumns_Thumbnail:
		{
			switch (role)
			{
			case Qt::DisplayRole:
				if (pAsset->IsModified())
				{
					//TODO : make the text bold/italics if possible
					QString str(pAsset->GetName());
					str += " *";
					return str;
				}
			case Qt::EditRole:
				return QString(pAsset->GetName());
			case Qt::DecorationRole:
				return pAsset->GetType()->GetIcon();
			case QThumbnailsView::s_ThumbnailRole:
				//start threaded loading
				if (!pAsset->IsThumbnailLoaded())
				{
					CAssetThumbnailsLoader::GetInstance().PostAsset(const_cast<CAsset*>(pAsset));
				}
				return pAsset->GetThumbnail();
			case QThumbnailsView::s_ThumbnailColorRole:
				return pAsset->GetType()->GetThumbnailColor();
			case QThumbnailsView::s_ThumbnailBackgroundColorRole:
				// Use a tinted background only for the default type icons.
				if (!pAsset->GetType()->HasThumbnail() || !pAsset->IsThumbnailLoaded())
				{
					const float lightnessFactor = 0.75f;
					const float saturationFactor = 0.125f;

					const QColor hslColor = pAsset->GetType()->GetThumbnailColor().toHsl();
					const QColor backgroundColor = QColor::fromHslF(
						hslColor.hslHueF(),
						hslColor.hslSaturationF() * saturationFactor,
						hslColor.lightnessF() * lightnessFactor,
						hslColor.alphaF());
					return backgroundColor;
				}
				else
				{
					return QVariant();
				}
			case QThumbnailsView::s_ThumbnailIconsRole:
			{
				QVariantList icons;
				if (!m_tumbnailIconProviders.empty())
				{
					for (const auto& it : m_tumbnailIconProviders)
					{
						QIcon icon = it.second(pAsset);
						if (!icon.isNull())
						{
							icons.push_back(std::move(icon));
						}
					}
				}

				if (m_favHelper.IsFavorite(index))
				{
					const QThumbnailsView::SSubIcon subIcon{ m_favHelper.GetFavoriteIcon(true), QThumbnailsView::SSubIcon::EPosition::TopLeft };
					QVariant v;
					v.setValue(subIcon);
					icons.push_back(v);
				}

				if (pAsset->IsModified())
				{
					const static CryIcon modified("icons:common/general_state_modified.ico");
					const QThumbnailsView::SSubIcon subIcon{ modified, QThumbnailsView::SSubIcon::EPosition::BottomRight };
					QVariant v;
					v.setValue(subIcon);
					icons.push_back(v);
				}

				return icons;
			}
			default:
				break;
			}
		}
	case eAssetColumns_FilterString:
	{
		if (role == Qt::DisplayRole)
		{
			return pAsset->GetFilterString();
		}
		break;
	}
	case eAssetColumns_Folder:
	{
		if (role == Qt::DisplayRole)
		{
			return pAsset->GetFolder().c_str();
		}
		break;
	}
	case eAssetColumns_Uid:
	{
		if (role == Qt::DisplayRole)
		{
			return QString(pAsset->GetGUID().ToString());
		}
		break;
	}
	case eAssetColumns_Status:
	{
		if (role == Qt::DisplayRole)
		{
			QStringList statuses;
			if (pAsset->IsBeingEdited())
				statuses.append(tr("Open"));

			if(pAsset->IsModified())
				statuses.append(tr("Modified"));

			return statuses.join(", ");
		}
		break;
	}
	default:
		break;
	}

	const int detailColumn = index.column() - (int)eAssetColumns_Details;
	if (detailColumn >= 0 && detailColumn < m_detailAttributes.size())
	{
		QVariant value = m_detailAttributes[detailColumn].getValueFn(pAsset, m_detailAttributes[detailColumn].pAttribute, role);

		// By returning a string, we ensure proper sorting of assets where this detail does not apply.
		// Note that invalid variants (QVariant()) have unnatural sorting characteristics.
		// For example, an invalid variant is greater than any variant storing an int.
		return value.isValid() ? value : QVariant(QString());  
	}

	return m_favHelper.data(index, role);
}

bool CAssetModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (index.isValid() && (index.column() == eAssetColumns_Name || index.column() == eAssetColumns_Thumbnail) && role == Qt::EditRole)
	{
		CAsset* pAsset = ToAssetInternal(index);

		QString stringValue = value.toString();

		if (stringValue.isEmpty())
		{
			return false;
		}

		const CryPathString newName = PathUtil::AdjustCasing(QtUtil::ToString(stringValue).c_str());
		const CryPathString newAssetFilepath = PathUtil::Make(pAsset->GetFolder(), pAsset->GetType()->MakeMetadataFilename(newName));

		string reasonToReject;
		if (!CAssetType::IsValidAssetPath(newAssetFilepath, reasonToReject))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Invalid asset path: %s. %s", newAssetFilepath.c_str(), reasonToReject.c_str());
			return false;
		}

		// Check if the new name doesn't collide with existing name
		const CAsset* const pExistingAsset = CAssetManager::GetInstance()->FindAssetForMetadata(newAssetFilepath);
		if (pExistingAsset && pExistingAsset != pAsset)
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Asset already exists: %s", newAssetFilepath.c_str());
			return false;
		}

		if (!CAssetManager::GetInstance()->RenameAsset(pAsset, newName))
		{
			return false;
		}
	}

	return m_favHelper.setData(index, value, role);
}

QVariant CAssetModel::GetHeaderData(int section, Qt::Orientation orientation, int role)
{
	if (orientation != Qt::Horizontal)
	{
		return QVariant();
	}

	const CItemModelAttribute* pAttribute = GetColumnAttribute(section);
	if (!pAttribute)
	{
		return QVariant();
	}

	if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
	{
		return pAttribute->GetName();
	}
	else if (role == Attributes::s_getAttributeRole)
	{
		return QVariant::fromValue(const_cast<CItemModelAttribute*>(pAttribute));
	}
	else if (role == Attributes::s_attributeMenuPathRole)
	{
		const int detailColumn = section - static_cast<int>(eAssetColumns_Details);
		if (detailColumn >= 0 && detailColumn < static_cast<int>(GetInstance()->m_detailAttributes.size()))
		{
			const SDetailAttribute& attrib = GetInstance()->m_detailAttributes[detailColumn];
			if (attrib.assetTypes.size() == 1)
			{
				return QtUtil::ToQString(attrib.assetTypes[0]->GetTypeName());
			}
		}
		// Shared details are in top-level category.
		return "";  
	}
	return QVariant();
}

QVariant CAssetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	return GetHeaderData(section, orientation, role);
}

Qt::ItemFlags CAssetModel::flags(const QModelIndex& index) const
{
	Qt::ItemFlags flags = m_favHelper.flags(index) | Qt::ItemIsDragEnabled;
	if (index.column() == eAssetColumns_Name || index.column() == eAssetColumns_Thumbnail)
	{
		flags |= Qt::ItemIsEditable;
	}
	return flags;
}

QModelIndex CAssetModel::index(int row, int column, const QModelIndex& parent /*= QModelIndex()*/) const
{
	if (!CAssetManager::GetInstance()->GetAssetsCount())
	{
		// QTreeView's layout code might call this function even though the model is empty.
		return QModelIndex();
	}

	if (row >= 0 && column >= 0)
	{
		CAsset* assetPtr = CAssetManager::GetInstance()->m_assets[row];
		return createIndex(row, column, reinterpret_cast<quintptr>(assetPtr));
	}
	return QModelIndex();
}

QModelIndex CAssetModel::parent(const QModelIndex& index) const
{
	//Hierarchy not supported
	return QModelIndex();
}

void CAssetModel::PreReset()
{
	beginResetModel();
}

void CAssetModel::PostReset()
{
	endResetModel();
}

void CAssetModel::PreInsert(const std::vector<CAsset*>& assets)
{
	CRY_ASSERT(assets.size());
	const int assetCount = CAssetManager::GetInstance()->GetAssetsCount();
	beginInsertRows(QModelIndex(), assetCount, assetCount + assets.size() - 1);
}

void CAssetModel::PostInsert(const std::vector<CAsset*>& assets)
{
	// New assets are supposed to be appended to m_assets.
	CRY_ASSERT(assets.size() <= CAssetManager::GetInstance()->GetAssetsCount());
	CRY_ASSERT(std::equal(assets.rbegin(), assets.rend(), CAssetManager::GetInstance()->m_assets.rbegin()));

	endInsertRows();
}

void CAssetModel::PreRemove(const std::vector<CAsset*>& assets)
{
	beginResetModel();
}

void CAssetModel::PostRemove()
{
	endResetModel();
}

void CAssetModel::OnAssetChanged(CAsset& asset)
{
	//Note: this will become very expensive with a lot of assets and a lot of simultaneous changes. Optimize by caching the index row in the CAsset itself so ToAsset is constant time
	QModelIndex topLeft = ToIndex(asset);
	dataChanged(topLeft, topLeft.sibling(topLeft.row(), GetColumnCount() - 1));
}

void CAssetModel::OnAssetThumbnailLoaded(CAsset& asset)
{
	const QModelIndex topLeft = ToIndex(asset);
	const QModelIndex changed = topLeft.sibling(topLeft.row(), eAssetColumns_Thumbnail);
	dataChanged(changed, changed);
}

Qt::DropActions CAssetModel::supportedDragActions() const
{
	return Qt::CopyAction | Qt::MoveAction;
}

QStringList CAssetModel::mimeTypes() const
{
	auto typeList = QAbstractItemModel::mimeTypes();
	// TODO: refactor hard coded mime type which must be known by each
	// widget using this model with drag and drop
	typeList << CDragDropData::GetMimeFormatForType("EngineFilePaths");
	typeList << CDragDropData::GetMimeFormatForType("Assets");
	return typeList;
}

QMimeData* CAssetModel::mimeData(const QModelIndexList& indexes) const
{
	if (indexes.size() <= 0)
	{
		return nullptr; // Documented behavior of base implementation.
	}

	QVector<quintptr> assets;
	QStringList enginePaths;
	QSet<qint64> idsUsed;

	// when rows with multiple columns are selected the same
	// path would be added for every column index of a selected
	// row leading to multiple paths. Therefore a check is required
	for (const auto& index : indexes)
	{
		if (!index.isValid() || idsUsed.contains(index.internalId()))
		{
			continue;
		}

		const CAsset* const pAsset = ToAssetInternal(index);
		CRY_ASSERT(pAsset);

		if (!pAsset->GetFilesCount())
		{
			continue;
		}

		assets << (quintptr)pAsset;

		auto filePath = QtUtil::ToQString(pAsset->GetType()->GetObjectFilePath(pAsset));
		// remove leading "/" if present
		if (!filePath.isEmpty() && filePath[0] == QChar('/'))
		{
			filePath.remove(0, 1);
		}
		enginePaths << filePath;

		idsUsed.insert(index.internalId());
	}

	if (assets.empty())
	{
		return nullptr;
	}

	auto pDragDropData = new CDragDropData();

	// Write EngineFilePaths mime data.
	{
		QByteArray byteArray;
		QDataStream stream(&byteArray, QIODevice::ReadWrite);
		stream << enginePaths;
		// TODO: refactor hard coded mime type
		pDragDropData->SetCustomData("EngineFilePaths", byteArray);
	}

	// Write Assets mime data.
	{
		QByteArray byteArray;
		QDataStream stream(&byteArray, QIODevice::ReadWrite);
		stream << assets;
		pDragDropData->SetCustomData("Assets", byteArray);
	}

	return pDragDropData;
}

std::vector<CAsset*> CAssetModel::GetAssets(const CDragDropData& data)
{
	QVector<quintptr> tmp;
	QByteArray byteArray = data.GetCustomData("Assets");
	QDataStream stream(byteArray);
	stream >> tmp;

	std::vector<CAsset*> assets;
	assets.reserve(tmp.size());
	std::transform(tmp.begin(), tmp.end(), std::back_inserter(assets), [](quintptr p)
	{
		return reinterpret_cast<CAsset*>(p);
	});
	return assets;
}

CAssetModel::CAutoRegisterColumn::CAutoRegisterColumn(const CItemModelAttribute* pAttribute, std::function<QVariant(const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)> computeFn)
	: CAutoRegister([pAttribute, computeFn = std::move(computeFn)]()
{
	CAssetManager::GetInstance()->GetAssetModel()->AddColumn(pAttribute, computeFn);
})
{
}
