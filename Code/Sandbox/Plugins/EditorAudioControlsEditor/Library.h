// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

#include <PoolObject.h>

namespace ACE
{
class CLibrary final : public CAsset, public CryAudio::CPoolObject<CLibrary, stl::PSyncNone>
{
public:

	CLibrary() = delete;
	CLibrary(CLibrary const&) = delete;
	CLibrary(CLibrary&&) = delete;
	CLibrary& operator=(CLibrary const&) = delete;
	CLibrary& operator=(CLibrary&&) = delete;

	explicit CLibrary(string const& name, ControlId const id)
		: CAsset(name, id, EAssetType::Library)
		, m_pakStatus(EPakStatus::None)
	{}

	virtual ~CLibrary() override = default;

	// CAsset
	virtual void SetModified(bool const isModified, bool const isForced = false) override;
	// ~CAsset

	EPakStatus GetPakStatus() const { return m_pakStatus; }
	void       SetPakStatus(EPakStatus const pakStatus, bool const exists);

private:

	EPakStatus m_pakStatus;
};
} // namespace ACE
