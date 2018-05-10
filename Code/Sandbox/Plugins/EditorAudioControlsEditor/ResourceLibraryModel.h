// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemLibraryModel.h"

namespace ACE
{
class CLibrary;

class CResourceLibraryModel final : public CSystemLibraryModel
{
public:

	explicit CResourceLibraryModel(CLibrary* const pLibrary, QObject* const pParent)
		: CSystemLibraryModel(pLibrary, pParent)
	{}

	CResourceLibraryModel() = delete;

protected:

	// CSystemLibraryModel
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	// ~CSystemLibraryModel
};
} // namespace ACE
