// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Asset.h"

namespace ACE
{
class CLibrary final : public CAsset
{
public:

	explicit CLibrary(string const& name);

	CLibrary() = delete;

	// CAsset
	virtual void SetModified(bool const isModified, bool const isForced = false) override;
	// ~CAsset

	EPakStatus GetPakStatus() const { return m_pakStatus; }
	void       SetPakStatus(EPakStatus const pakStatus, bool const exists);

private:

	EPakStatus m_pakStatus;
};
} // namespace ACE
