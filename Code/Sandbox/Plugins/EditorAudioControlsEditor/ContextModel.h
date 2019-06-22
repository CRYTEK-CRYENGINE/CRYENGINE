// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAudio/IAudioInterfacesCommonData.h>
#include <QAbstractItemModel>

namespace ACE
{
class CContext;

class CContextModel final : public QAbstractItemModel
{
	Q_OBJECT

public:

	CContextModel() = delete;
	CContextModel(CContextModel const&) = delete;
	CContextModel(CContextModel&&) = delete;
	CContextModel& operator=(CContextModel const&) = delete;
	CContextModel& operator=(CContextModel&&) = delete;

	enum class EColumns : CryAudio::EnumFlagsType
	{
		Notification,
		Name,
		Count, };

	explicit CContextModel(QObject* const pParent);
	virtual ~CContextModel() override;

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
	// ~QAbstractItemModel

	void             Reset();
	void             DisconnectSignals();

	static CContext* GetContextFromIndex(QModelIndex const& index);

private:

	void ConnectSignals();
};
} // namespace ACE
