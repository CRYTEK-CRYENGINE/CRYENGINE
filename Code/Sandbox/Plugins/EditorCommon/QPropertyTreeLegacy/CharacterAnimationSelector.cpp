// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"

#include <Models/CharacterAnimationsModel.h>
#include <Dialogs/TreeViewDialog.h>
#include "IResourceSelectorHost.h"

#include <CryAnimation/ICryAnimation.h>
#include <CryEntitySystem/IEntity.h>

SResourceSelectionResult CharacterAnimationSelector(const SResourceSelectorContext& context, const char* previousValue, IEntity* pEntity)
{
	SResourceSelectionResult result { false, previousValue };

	if (!pEntity || !pEntity->GetCharacter(0) || !pEntity->GetCharacter(0)->GetIAnimationSet())
	{
		return result;
	}

	IAnimationSet* pAnimSet = pEntity->GetCharacter(0)->GetIAnimationSet();
	CCharacterAnimationsModel* pModel = new CCharacterAnimationsModel(*pAnimSet);
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

SResourceValidationResult ValidateCharacterAnimation(const SResourceSelectorContext& context, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	SResourceValidationResult result{ false, previousValue };
	if (!newValue || !*newValue)
	{
		result.validatedResource = "";
		return result;
	}

	if (!pEntity || !pEntity->GetCharacter(0) || !pEntity->GetCharacter(0)->GetIAnimationSet())
	{
		return result;
	}

	IAnimationSet* pAnimSet = pEntity->GetCharacter(0)->GetIAnimationSet();
	CCharacterAnimationsModel* pModel = new CCharacterAnimationsModel(*pAnimSet);
	int itemCount = pModel->rowCount();
	for (int i = 0; i < itemCount; ++i)
	{
		QModelIndex index = pModel->index(i, 0);
		if (pModel->data(index, Qt::DisplayRole).value<QString>() == newValue)
		{
			result.validatedResource = newValue;
			result.isValid = true;
			return result;
		}
	}

	return result;
}

SResourceValidationResult ValidateTrackCharacterAnimation(const SResourceSelectorContext& context, const char* newValue, const char* previousValue, IEntity* pEntity)
{
	return { true, newValue };
}

REGISTER_RESOURCE_VALIDATING_SELECTOR("TrackCharacterAnimation", CharacterAnimationSelector, ValidateTrackCharacterAnimation, "")
REGISTER_RESOURCE_VALIDATING_SELECTOR("CharacterAnimation", CharacterAnimationSelector, ValidateCharacterAnimation, "")
