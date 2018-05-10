// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

namespace ACE
{
enum class EPakStatus
{
	None   = 0,
	InPak  = BIT(0),
	OnDisk = BIT(1),
};
CRY_CREATE_ENUM_FLAG_OPERATORS(EPakStatus);

class CLibrary final : public CAsset
{
public:

	explicit CLibrary(string const& name);

	CLibrary() = delete;

	// CAsset
	virtual void SetModified(bool const isModified, bool const isForced = false) override;
	// ~CAsset

	EPakStatus GetPakStatus() const { return m_pakStatusFlags; }
	void       SetPakStatus(EPakStatus const pakStatus, bool const exists);

private:

	EPakStatus m_pakStatusFlags;
};
} // namespace ACE
