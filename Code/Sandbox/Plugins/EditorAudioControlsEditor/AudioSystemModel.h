// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include <ProxyModels/DeepFilterProxyModel.h>

namespace ACE
{
class IAudioSystemEditor;
class IAudioSystemItem;

namespace AudioModelUtils
{
void DecodeImplMimeData(const QMimeData* pData, std::vector<IAudioSystemItem*>& outItems);
} // namespace AudioModelUtils

class QAudioSystemModel final : public QAbstractItemModel
{

public:

	enum EAudioSystemColumns
	{
		eAudioSystemColumns_Name,
		eAudioSystemColumns_Count,
	};

	// TODO: Should be replaced with the new attribute system
	enum EAudioSystemAttributes
	{
		eAudioSystemAttributes_Type = Qt::UserRole + 1,
		eAudioSystemAttributes_Connected,
		eAudioSystemAttributes_Placeholder,
		eAudioSystemAttributes_Localized,
	};

	QAudioSystemModel();

	// QAbstractItemModel
	virtual int             rowCount(const QModelIndex& parent) const override;
	virtual int             columnCount(const QModelIndex& parent) const override;
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual Qt::DropActions supportedDragActions() const override;
	virtual QStringList     mimeTypes() const override;
	virtual QMimeData*      mimeData(const QModelIndexList& indexes) const override;
	// ~QAbstractItemModel

	IAudioSystemItem* ItemFromIndex(const QModelIndex& index) const;
	QModelIndex       IndexFromItem(IAudioSystemItem* pItem) const;
	void              Reset();

	static char const* const ms_szMimeType;

private:
	IAudioSystemEditor* m_pAudioSystem;
};

class QAudioSystemModelProxyFilter final : public QDeepFilterProxyModel
{
public:

	QAudioSystemModelProxyFilter(QObject* parent);

	// QSortFilterProxyModel
	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
	// ~QSortFilterProxyModel

	// QDeepFilterProxyModel
	virtual bool rowMatchesFilter(int source_row, const QModelIndex& source_parent) const override;
	// ~QDeepFilterProxyModel

	void SetAllowedControlsMask(uint allowedControlsMask);
	void SetHideConnected(bool bHideConnected);
	
private:

	uint m_allowedControlsMask;
	uint m_bHideConnected;
};
} // namespace ACE
