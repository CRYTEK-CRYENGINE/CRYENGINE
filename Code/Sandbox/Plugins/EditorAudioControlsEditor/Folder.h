// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

#include <PoolObject.h>

namespace ACE
{
class CFolder final : public CAsset, public CryAudio::CPoolObject<CFolder, stl::PSyncNone>
{
public:

	CFolder() = delete;
	CFolder(CFolder const&) = delete;
	CFolder(CFolder&&) = delete;
	CFolder& operator=(CFolder const&) = delete;
	CFolder& operator=(CFolder&&) = delete;

	explicit CFolder(string const& name, ControlId const id)
		: CAsset(name, id, EAssetType::Folder)
	{}

	virtual ~CFolder() override = default;

};
} // namespace ACE
