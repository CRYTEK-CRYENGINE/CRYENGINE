// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IResourceSelectorHost.h"

#include "PreviewToolTip.h"

bool SStaticResourceSelectorEntry::UsesInputField() const
{
	return validate || validateWithContext;
}

void SStaticResourceSelectorEntry::EditResource(const SResourceSelectorContext& context, const char* value) const
{
	if (edit)
		edit(context, value);
}

SResourceValidationResult SStaticResourceSelectorEntry::ValidateValue(const SResourceSelectorContext& context, const char* newValue, const char* previousValue) const
{
	SResourceValidationResult result{ false, previousValue };
	if (validate)
	{
		result = validate(context, newValue, previousValue);
	}
	else if (validateWithContext)
	{
		result = validateWithContext(context, newValue, previousValue, context.contextObject);
	}

	return result;
}

SResourceSelectionResult SStaticResourceSelectorEntry::SelectResource(const SResourceSelectorContext& context, const char* previousValue) const
{
	SResourceSelectionResult result;

	if (function)
	{
		result = function(context, previousValue);
	}
	else if (functionWithContext)
	{
		result = functionWithContext(context, previousValue, context.contextObject);
	}

	return result;
}

bool SStaticResourceSelectorEntry::ShowTooltip(const SResourceSelectorContext& context, const char* value) const
{
	return CPreviewToolTip::ShowTrackingToolTip(value, context.parentWidget);
}

void SStaticResourceSelectorEntry::HideTooltip(const SResourceSelectorContext& context, const char* value) const
{
	QTrackingTooltip::HideTooltip();
}