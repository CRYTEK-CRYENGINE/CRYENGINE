// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

namespace ACE
{
class CFolder final : public CAsset
{
public:

	explicit CFolder(string const& name);

	CFolder() = delete;
};
} // namespace ACE
