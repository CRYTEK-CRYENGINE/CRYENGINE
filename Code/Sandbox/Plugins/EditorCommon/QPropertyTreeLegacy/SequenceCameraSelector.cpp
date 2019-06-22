// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/SequenceCamerasModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"

#include <CryAnimation/ICryAnimation.h>

SResourceSelectionResult SequenceCameraSelector(const SResourceSelectorContext& context, const char* previousValue, IAnimSequence* pSequence)
{
	SResourceSelectionResult result{ false, previousValue };

	if (!pSequence)
	{
		return result;
	}

	CSequenceCamerasModel* pModel = new CSequenceCamerasModel(*pSequence);
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
		}
	}

	return result;
}

SResourceValidationResult ValidateSequenceCamera(const SResourceSelectorContext& context, const char* newValue, const char* previousValue, IAnimSequence* pSequence)
{
	SResourceValidationResult result{ false, previousValue };

	if (!newValue || !*newValue)
	{
		result.validatedResource = "";
		return result;
	}

	if (!pSequence)
	{
		return result;
	}

	CSequenceCamerasModel* pModel = new CSequenceCamerasModel(*pSequence);
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

REGISTER_RESOURCE_VALIDATING_SELECTOR("SequenceCamera", SequenceCameraSelector, ValidateSequenceCamera, "")
