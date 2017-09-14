// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ResourceSelectorModel.h"
#include "QAudioControlEditorIcons.h"

#include <QtUtil.h>

#include "AudioAssets.h"
#include "IEditor.h"
#include "QAudioControlTreeWidget.h"

#include <CryString/CryPath.h>
#include <DragDrop.h>

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
QVariant CResourceControlModel::data(QModelIndex const& index, int role) const
{
	CAudioLibrary* const pLibrary = static_cast<CAudioLibrary*>(index.internalPointer());

	if (pLibrary != nullptr)
	{
		switch (role)
		{
		case Qt::DisplayRole:
			return QtUtil::ToQStringSafe(pLibrary->GetName());
			break;

		case Qt::DecorationRole:
			return GetItemTypeIcon(EItemType::eItemType_Library);
			break;

		case EDataRole::eDataRole_ItemType:
			return EItemType::eItemType_Library;
			break;

		case EDataRole::eDataRole_InternalPointer:
			return reinterpret_cast<intptr_t>(pLibrary);
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CResourceControlModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	if (index.isValid())
	{
		IAudioAsset* const pItem = static_cast<IAudioAsset*>(index.internalPointer());

		if (pItem != nullptr)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CResourceControlModel::flags(QModelIndex const& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsSelectable;
	}

	return Qt::NoItemFlags;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CResourceControlModel::supportedDropActions() const
{
	return Qt::IgnoreAction;
}

//////////////////////////////////////////////////////////////////////////
QVariant CResourceLibraryModel::data(QModelIndex const& index, int role) const
{
	if (!index.isValid())
	{
		return QVariant();
	}

	IAudioAsset* const pItem = static_cast<IAudioAsset*>(index.internalPointer());

	if (pItem != nullptr)
	{
		EItemType const itemType = pItem->GetType();

		switch (role)
		{
		case Qt::DisplayRole:
			return QtUtil::ToQStringSafe(pItem->GetName());
			break;

		case Qt::DecorationRole:
			return GetItemTypeIcon(itemType);
			break;

		case EDataRole::eDataRole_ItemType:
			return pItem->GetType();
			break;

		case EDataRole::eDataRole_InternalPointer:
			return reinterpret_cast<intptr_t>(pItem);
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
bool CResourceLibraryModel::setData(QModelIndex const& index, QVariant const& value, int role)
{
	if (index.isValid())
	{
		IAudioAsset* const pItem = static_cast<IAudioAsset*>(index.internalPointer());

		if (pItem != nullptr)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
Qt::ItemFlags CResourceLibraryModel::flags(QModelIndex const& index) const
{
	if (index.isValid())
	{
		return QAbstractItemModel::flags(index) | Qt::ItemIsSelectable;
	}

	return Qt::NoItemFlags;
}

//////////////////////////////////////////////////////////////////////////
Qt::DropActions CResourceLibraryModel::supportedDropActions() const
{
	return Qt::IgnoreAction;
}
} // namespace ACE
