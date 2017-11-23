// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ProxyModels/DeepFilterProxyModel.h>
#include <SystemTypes.h>

namespace ACE
{
class CSystemControl;
class CSystemLibrary;
class CSystemFolder;
class CSystemAsset;
class CSystemAssetsManager;

enum class EDataRole
{
	ItemType = Qt::UserRole + 1,
	Modified,
	InternalPointer,
	Id,
};

namespace AudioModelUtils
{
void          GetAssetsFromIndexesSeparated(QModelIndexList const& list, std::vector<CSystemLibrary*>& outLibraries, std::vector<CSystemFolder*>& outFolders, std::vector<CSystemControl*>& outControls);
void          GetAssetsFromIndexesCombined(QModelIndexList const& list, std::vector<CSystemAsset*>& outAssets);
CSystemAsset* GetAssetFromIndex(QModelIndex const& index);
} // namespace AudioModelUtils

class CAudioLibraryModel : public QAbstractItemModel
{
public:

	CAudioLibraryModel(CSystemAssetsManager* const pAssetsManager);
	virtual ~CAudioLibraryModel() override;

protected:

	// QAbstractItemModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// ~QAbstractItemModel

	void ConnectToSystem();
	void DisconnectFromSystem();
	CSystemAssetsManager* const m_pAssetsManager;
};

class CSystemControlsModel : public QAbstractItemModel
{
public:

	CSystemControlsModel(CSystemAssetsManager* const pAssetsManager, CSystemLibrary* const pLibrary);
	virtual ~CSystemControlsModel() override;

	void DisconnectFromSystem();

protected:

	QModelIndex IndexFromItem(CSystemAsset const* pItem) const;

	// QAbstractItemModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual QMimeData*      mimeData(QModelIndexList const& indexes) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// ~QAbstractItemModel

	void ConnectToSystem();

	CSystemAssetsManager* const m_pAssetsManager;
	CSystemLibrary* const       m_pLibrary;
};

class CSystemControlsFilterProxyModel final : public QDeepFilterProxyModel
{
public:

	CSystemControlsFilterProxyModel(QObject* parent);

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int source_row, QModelIndex const& source_parent) const override;
	// ~QDeepFilterProxyModel

	// QSortFilterProxyModel
	virtual bool lessThan(QModelIndex const& left, QModelIndex const& right) const override;
	// ~QSortFilterProxyModel

	void EnableControl(bool const isEnabled, ESystemItemType const type);

private:

	uint m_validControlsMask = std::numeric_limits<uint>::max();
};
} // namespace ACE
