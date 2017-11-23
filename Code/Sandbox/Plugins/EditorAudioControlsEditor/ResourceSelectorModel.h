// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SystemControlsModel.h"

namespace ACE
{
class CSystemLibrary;
class CSystemAssetsManager;

class CResourceLibraryModel final : public CAudioLibraryModel
{
public:

	CResourceLibraryModel(CSystemAssetsManager* const pAssetsManager)
		: CAudioLibraryModel(pAssetsManager)
	{}

private:

	// CAudioAssetsExplorerModel
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	// ~CAudioAssetsExplorerModel
};

class CResourceControlsModel final : public CSystemControlsModel
{
public:

	CResourceControlsModel(CSystemAssetsManager* const pAssetsManager, CSystemLibrary* const pLibrary)
		:CSystemControlsModel(pAssetsManager, pLibrary)
	{}

private:

	// CAudioLibraryModel
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	// ~CAudioLibraryModel
};
} // namespace ACE
