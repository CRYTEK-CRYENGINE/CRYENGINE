// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>

#include <QAbstractItemModel>

#include <memory>

class CAsset;
class CAssetType;
class CItemModelAttribute;

//! Contains at most one item which represents a new asset. This item is editable, so that its name
//! can be set by a user. This item is only a placeholder and will be removed once a name is chosen.
//! This class is closely related to the "New asset" function of the asset browser.
class CNewAssetModel final : public QAbstractItemModel
{
private:
	struct SAssetDescription
	{
		string name;
		string path;
		const CAssetType* pAssetType;
		const void* pTypeSpecificParameter;
		bool bChanged = false;
	};

public:
	void BeginCreateAsset(const string& path, const string& name, const CAssetType& type, const void* pTypeSpecificParameter);
	void EndCreateAsset();
	bool IsEditing() { return m_pRow.get() != nullptr; }
	CAsset* GetNewAsset() const;

	static CNewAssetModel* GetInstance()
	{
		static CNewAssetModel theInstance;
		return &theInstance;
	}

	//////////////////////////////////////////////////////////
	// QAbstractItemModel implementation.
	virtual int rowCount(const QModelIndex& parent) const override;
	virtual int columnCount(const QModelIndex& parent) const override;
	virtual QVariant data(const QModelIndex& index, int role) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	//////////////////////////////////////////////////////////
private:
	CNewAssetModel();
private:
	std::unique_ptr<SAssetDescription> m_pRow;
	CAsset* m_pAsset;
	string m_defaultName;
};
