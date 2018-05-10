// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Folder.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CFolder::CFolder(string const& name)
	: CAsset(name, EAssetType::Folder)
{}
} // namespace ACE
