// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QAbstractItemModel>
#include <ProxyModels/DeepFilterProxyModel.h>

enum EDataRole
{
	eDataRole_ItemType = Qt::UserRole + 1,
	eDataRole_Modified,
	eDataRole_InternalPointer
};

namespace ACE
{
class CAudioControl;
class IAudioAsset;
class CAudioAssetsManager;

namespace AudioModelUtils
{
void         GetAssetsFromIndices(const QModelIndexList& list, std::vector<CAudioLibrary*>& outLibraries, std::vector<CAudioFolder*>& outFolders, std::vector<CAudioControl*>& outControls);
IAudioAsset* GetAssetFromIndex(const QModelIndex& index);
} // namespace AudioModelUtils

class CAudioAssetsExplorerModel : public QAbstractItemModel
{
public:
	CAudioAssetsExplorerModel(CAudioAssetsManager* pAssetsManager);
	~CAudioAssetsExplorerModel();

protected:
	// ------------- QAbstractItemModel ----------------
	virtual int             rowCount(const QModelIndex& parent) const override;
	virtual int             columnCount(const QModelIndex& parent) const override;
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual bool            setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual bool            canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual bool            dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// -------------------------------------------------------

private:
	void ConnectToSystem();
	void DisconnectFromSystem();
	CAudioAssetsManager* m_pAssetsManager = nullptr;
};

class QControlsProxyFilter : public QDeepFilterProxyModel
{
public:
	QControlsProxyFilter(QObject* parent);
	virtual bool     rowMatchesFilter(int source_row, const QModelIndex& source_parent) const override;
	virtual bool     lessThan(const QModelIndex& left, const QModelIndex& right) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;

	void         EnableControl(const bool bEnabled, const EItemType type);

private:
	uint m_validControlsMask = std::numeric_limits<uint>::max();
};

class CAudioLibraryModel : public QAbstractItemModel
{
public:
	CAudioLibraryModel(CAudioAssetsManager* pAssetsManager, CAudioLibrary* pLibrary);
	~CAudioLibraryModel();
	void DisconnectFromSystem();

protected:
	QModelIndex IndexFromItem(const IAudioAsset* pItem) const;

	// ------------- QAbstractItemModel ----------------
	virtual int             rowCount(const QModelIndex& parent) const override;
	virtual int             columnCount(const QModelIndex& parent) const override;
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual bool            setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual bool            canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual bool            dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
	virtual QMimeData*      mimeData(const QModelIndexList& indexes) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// -------------------------------------------------------

private:
	void ConnectToSystem();

	CAudioAssetsManager* m_pAssetsManager = nullptr;
	CAudioLibrary*       m_pLibrary = nullptr;
};
} // namespace ACE
