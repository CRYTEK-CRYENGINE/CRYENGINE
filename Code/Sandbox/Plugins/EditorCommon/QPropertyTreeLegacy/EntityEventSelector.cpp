// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/EntityEventsModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"

#include <IEditor.h>

#include <CryEntitySystem/IEntityClass.h>
#include <CryEntitySystem/IEntity.h>

SResourceSelectionResult EntityEventSelector(const SResourceSelectorContext& context, const char* previousValue, IEntity* pEntity)
{
	SResourceSelectionResult result{ false, previousValue };
	if (!pEntity)
	{
		return result;
	}

	IEntityClass* pClass = pEntity->GetClass();
	if (!pClass)
	{
		return result;
	}

	CEntityEventsModel* pModel = new CEntityEventsModel(*pClass);
	CTreeViewDialog dialog(context.parentWidget);
	QString selectedValue(previousValue);

	dialog.Initialize(pModel, 0);

	for (int32 row = pModel->rowCount(); row--;)
	{
		const QModelIndex index = pModel->index(row, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == selectedValue)
		{
			dialog.SetSelectedValue(index, false);
		}
	}

	result.selectionAccepted = dialog.exec();

	if (result.selectionAccepted)
	{
		QModelIndex index = dialog.GetSelected();
		if (index.isValid())
		{
			result.selectedResource = index.data().value<QString>().toLocal8Bit().data();
			return result;
		}
	}

	return result;
}

SResourceValidationResult ValidateEntityEvent(const SResourceSelectorContext& x, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	SResourceValidationResult result{ false, previousValue };
	if (!newValue || !*newValue)
	{
		result.validatedResource = "";
		return result;
	}

	if (!pEntity)
	{
		return result;
	}

	IEntityClass* pClass = pEntity->GetClass();
	if (!pClass)
	{
		return result;
	}

	CEntityEventsModel* pModel = new CEntityEventsModel(*pClass);
	int itemCount = pModel->rowCount();
	for (int i = 0; i < itemCount; ++i)
	{
		QModelIndex index = pModel->index(i, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == newValue)
		{
			result.validatedResource = newValue;
			result.isValid = true;
		}
	}

	return result;
}

REGISTER_RESOURCE_VALIDATING_SELECTOR("EntityEvent", EntityEventSelector, ValidateEntityEvent, "")
