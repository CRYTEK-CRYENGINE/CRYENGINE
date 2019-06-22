// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QAbstractItemModel>
#include "ProxyModels/ItemModelAttribute.h"
#include "ProxyModels/FavoritesHelper.h"
#include "AutoRegister.h"

class CAsset;
class CAssetType;
class CDragDropData;

enum EAssetColumns
{
	eAssetColumns_Favorite,
	eAssetColumns_Name,
	eAssetColumns_Type,
	eAssetColumns_Status,
	eAssetColumns_Folder,
	eAssetColumns_Uid,
	eAssetColumns_Thumbnail,
	eAssetColumns_FilterString, //optimization for smart search
	eAssetColumns_Details       // First column of asset type-specific details. NOTE: Must be the last enumerator.
};

enum EAssetModelRowType
{
	eAssetModelRow_Asset,
	eAssetModelRow_Folder
};

namespace AssetModelAttributes
{
extern CAttributeType<QString> s_dependenciesAttributeType;

extern CItemModelAttributeEnumFunc s_AssetTypeAttribute;
extern CItemModelAttributeEnumFunc s_AssetStatusAttribute;
extern CItemModelAttribute s_AssetFolderAttribute;
extern CItemModelAttribute s_AssetUidAttribute;
extern CItemModelAttribute s_FilterStringAttribute;
};

//! This model describes all the assets contained in the AssetManager
//! It uses attributes to enable smart filtering of assets in the asset browser
class EDITOR_COMMON_API CAssetModel : public QAbstractItemModel
{
	//CAssetModel instance is managed by the AssetManager
	friend class CAssetManager;
	CAssetModel(QObject* parent = nullptr);
	~CAssetModel();

	struct SDetailAttribute
	{
		const CItemModelAttribute* pAttribute;
		std::vector<CAssetType*>   assetTypes; //!< All asset types sharing attribute \p pAttribute.
		std::function<QVariant(const CAsset* pAsset, const CItemModelAttribute* pDetail, int role)> getValueFn;
	};

public:
	static CAssetModel* GetInstance();

	enum class Roles : int
	{
		InternalPointerRole = Qt::UserRole, //intptr_t (CAsset*)
		TypeCheckRole,                      //EAssetModelRowType
		Max,
	};

	//! A helper class to auto registration additional columns.
	//! \sa CAssetModel::AddColumn
	class CAutoRegisterColumn : public CAutoRegister<CAutoRegisterColumn>
	{
	public:
		CAutoRegisterColumn(const CItemModelAttribute* pAttribute, std::function<QVariant(const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)> getValueFn);
	};

	static bool                       IsAsset(const QModelIndex& index);
	static CAsset*                    ToAsset(const QModelIndex& index);

	QModelIndex                       ToIndex(const CAsset& asset, int col = 0) const;

	static std::vector<CAsset*>       GetAssets(const CDragDropData& data);
	static const CItemModelAttribute* GetColumnAttribute(int column);
	static QVariant                   GetHeaderData(int section, Qt::Orientation orientation, int role);
	static int                        GetColumnCount();
	bool                              AddColumn(const CItemModelAttribute* pAttribute, std::function<QVariant(const CAsset* pAsset, const CItemModelAttribute* pAttribute, int role)> getValueFn);

	//! Adds icons providers to the list.
	//! This icon providers are called in order to get additional icons that need to be displayed in thumbnail view
	void AddThumbnailIconProvider(const string& name, std::function<QIcon(const CAsset*)> iconProviderFunc);
	void RemoveThumbnailIconProvider(const string& name);

	//////////////////////////////////////////////////////////
	// QAbstractItemModel implementation
	virtual bool            hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	virtual int             rowCount(const QModelIndex& parent) const override;
	virtual int             columnCount(const QModelIndex& parent) const override;
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual bool            setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual Qt::DropActions supportedDragActions() const override;
	virtual QStringList     mimeTypes() const override;
	virtual QMimeData*      mimeData(const QModelIndexList& indexes) const override;
	//////////////////////////////////////////////////////////

	static QStringList GetAssetTypesStrList();
	static QStringList GetStatusesList();

	//! Finds a subset of the columns that are relevant for the specified asset types.
	//! \return Returns a vector of column indices.
	static std::vector<int> GetColumnsByAssetTypes(const std::vector<CAssetType*>& assetTypes);

private:
	//! \see CAssetManager::Init()
	void Init();

	//! Builds the vector of detail attributes. A detail attribute stores a CItemModelAttribute A and a name map M.
	//! For each asset type that returns a detail with attribute A, M contains a pair <N, T>, where T is the asset type and N is the detail name for A in T.
	//! Example: Assume MeshType returns a detail with name "vertexCount", and CItemModelAttribute vertexCountAttribute. Also, SkinType returns a detail with
	//! name "numVertices" but same attribute vertexCountAttribute.
	//! Then m_detailAttributes contains an element with A = vertexCountAttribute and M = { { MeshType, "vertexCount" }, { SkinType, "numVertices" } };
	void          BuildDetailAttributes();

	void          AddPredefinedComputedColumns();

	void          PreReset();
	void          PostReset();

	void          PreInsert(const std::vector<CAsset*>& assets);
	void          PostInsert(const std::vector<CAsset*>& assets);

	void          PreRemove(const std::vector<CAsset*>& assets);
	void          PostRemove();

	void          OnAssetChanged(CAsset& asset);
	void          OnAssetThumbnailLoaded(CAsset& asset);

	CAsset*       ToAssetInternal(const QModelIndex& index);
	const CAsset* ToAssetInternal(const QModelIndex& index) const;

	FavoritesHelper               m_favHelper;

	std::vector<SDetailAttribute> m_detailAttributes;
	std::vector<std::pair<string, std::function<QIcon(const CAsset*)>>> m_tumbnailIconProviders;
};