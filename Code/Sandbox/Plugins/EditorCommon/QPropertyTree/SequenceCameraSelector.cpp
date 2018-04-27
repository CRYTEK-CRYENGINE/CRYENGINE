// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/SequenceCamerasModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"

#include <CryAnimation/ICryAnimation.h>

dll_string SequenceCameraSelector(const SResourceSelectorContext& x, const char* previousValue, IAnimSequence* pSequence)
{
	if (!pSequence)
		return previousValue;

	CSequenceCamerasModel* pModel = new CSequenceCamerasModel(*pSequence);
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

dll_string ValidateSequenceCamera(const SResourceSelectorContext& x, const char* newValue, const char* previousValue, IAnimSequence* pSequence)
{
	if (!newValue || !*newValue)
		return dll_string();

	if (!pSequence)
		return previousValue;

	CSequenceCamerasModel* pModel = new CSequenceCamerasModel(*pSequence);
	int itemCount = pModel->rowCount();
	for (int i = 0; i < itemCount; ++i)
	{
		QModelIndex index = pModel->index(i, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == newValue)
			return newValue;
	}

	return previousValue;
}

REGISTER_RESOURCE_VALIDATING_SELECTOR("SequenceCamera", SequenceCameraSelector, ValidateSequenceCamera, "")

