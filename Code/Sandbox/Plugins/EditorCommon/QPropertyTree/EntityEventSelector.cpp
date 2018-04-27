// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/EntityEventsModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"
#include <CryEntitySystem/IEntityClass.h>
#include <CryEntitySystem/IEntity.h>

dll_string EntityEventSelector(const SResourceSelectorContext& x, const char* previousValue, IEntity* pEntity)
{
	if (!pEntity)
		return previousValue;

	IEntityClass* pClass = pEntity->GetClass();
	if (!pClass)
		return previousValue;

	CEntityEventsModel* pModel = new CEntityEventsModel(*pClass);
	CTreeViewDialog dialog(x.parentWidget);
	QString selectedValue(previousValue);

	dialog.Initialize(pModel, 0);

	for (int32 row = pModel->rowCount(); row--; )
	{
		const QModelIndex index = pModel->index(row, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == selectedValue)
		{
			dialog.SetSelectedValue(index, false);
		}
	}

	if (dialog.exec())
	{
		QModelIndex index = dialog.GetSelected();
		if (index.isValid())
		{
			return index.data().value<QString>().toLocal8Bit().data();
		}
	}

	return previousValue;
}

dll_string ValidateEntityEvent(const SResourceSelectorContext& x, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	if (!newValue || !*newValue)
		return dll_string();

	if (!pEntity)
		return previousValue;

	IEntityClass* pClass = pEntity->GetClass();
	if (!pClass)
		return previousValue;

	CEntityEventsModel* pModel = new CEntityEventsModel(*pClass);
	int itemCount = pModel->rowCount();
	for (int i = 0; i < itemCount; ++i)
	{
		QModelIndex index = pModel->index(i, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == newValue)
			return newValue;
	}

	return previousValue;
}

REGISTER_RESOURCE_VALIDATING_SELECTOR("EntityEvent", EntityEventSelector, ValidateEntityEvent, "")

