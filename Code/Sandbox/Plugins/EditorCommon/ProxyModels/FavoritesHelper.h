// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

#include <QSet>
#include <QVariant>
#include <QString>

class QAbstractItemView;
class QAdvancedItemDelegate;
class QTreeView;

class QModelIndex;

//! Designed to help you with implementation details of a favorite system in an ItemModel/View system
//! Favorites will be saved in the project personalization as they often refer to project specific assets and data
//! How to use:
//!  - (View) Requires usage of QAdvancedItemDelegate in the view to display icons properly
//!	 - (View) Call SetupView
//!  - (Model) Implement Qt::FavoritesIdRole (or overriden role) to extract a unique QVariant to identify the row. This value must be consistent through several instances of the program (for instance do not use a pointer value).
//!  - (Model) Call FavoritesHelper::data() in your data()
//!  - (Model) Call FavoritesHelper::setData() in your setData()
//!  - (Model) Call FavoritesHelper::favoriteFlags() in your flags() for the appropriate column
//!  - (Model) It is also recommended to reuse the attribute Attribute::s_favoriteAttribute for convenience
class EDITOR_COMMON_API FavoritesHelper
{
public:

	static constexpr int s_FavoritesIdRole = Qt::UserRole + 1436;

	FavoritesHelper(const QString& uniqueFavKey, int favoritesColumn);
	~FavoritesHelper();

	//add overloads of SetupView for other view types as needed
	static void SetupView(QAbstractItemView* view, QAdvancedItemDelegate* delegate, int favoritesColumn);
	static void SetupView(QTreeView* treeView, QAdvancedItemDelegate* delegate, int favoritesColumn);

	bool IsFavorite(const QModelIndex& index) const;
	void SetFavorite(const QModelIndex& index, bool favorite = true);

	//! default role value is s_FavoritesIdRole
	void SetFavoritesIdRole(int role);

	//Use these as a fallback case in your models to make it work
	QVariant data(const QModelIndex& index, int role) const;
	bool setData(const QModelIndex& index, const QVariant& value, int role);
	Qt::ItemFlags flags(const QModelIndex& index) const;

	static CryIcon GetFavoriteIcon(bool isActive);

private:

	void Load();
	void Save();

	const QString m_key;
	int m_favIdRole;
	int m_favColumn;
	QList<QVariant> m_favorites;
};
