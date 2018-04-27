// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include "TextureManager.h"

class CTextureManager;

class CTextureModel : public QAbstractItemModel
{
public:
	enum EColumn
	{
		eColumn_Filename,
		eColumn_RcSettings,
		eColumn_COUNT
	};

	enum ERcSettings
	{
		eRcSettings_Diffuse,
		eRcSettings_Bumpmap,
		eRcSettings_Specular,
		eRcSettings_COUNT
	};
public:
	static const char* GetDisplayNameOfSettings(CTextureModel::ERcSettings settings);

	CTextureModel(CTextureManager* pTextureManager, QObject* pParent = nullptr);

	CTextureManager::TextureHandle GetTexture(const QModelIndex& index);

	CTextureManager*               GetTextureManager();

	// QAbstractItemModel implementation.
	QModelIndex   index(int row, int column, const QModelIndex& parent) const override;
	QModelIndex   parent(const QModelIndex& index) const override;
	int           rowCount(const QModelIndex& index) const override;
	int           columnCount(const QModelIndex& index) const override;
	QVariant      data(const QModelIndex& index, int role) const override;
	bool          setData(const QModelIndex& index, const QVariant& value, int role) override;
	QVariant      headerData(int column, Qt::Orientation orientation, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& modelIndex) const override;

	void          Reset();

	void   OnTexturesChanged();
private:
	string GetSettingsRcString(ERcSettings rcSettings) const;
	int    GetSettings(const string& rcString) const;

	CTextureManager* m_pTextureManager;
};

