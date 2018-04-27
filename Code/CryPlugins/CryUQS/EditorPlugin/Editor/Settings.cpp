// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Settings.h"

bool SDocumentSettings::operator==(const SDocumentSettings& other) const
{
	return ::memcmp(this, &other, sizeof(SDocumentSettings)) == 0;
}

bool SDocumentSettings::operator!=(const SDocumentSettings& other) const
{
	return !(*this == other);
}
