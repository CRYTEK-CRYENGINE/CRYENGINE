// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SharedData.h"
#include <CryIcon.h>

namespace ACE
{
struct SControlInfo
{
	SControlInfo(string const& name_, ControlId const id_, CryIcon const& icon_)
		: name(name_)
		, id(id_)
		, icon(icon_)
	{}

	string const&   name;
	ControlId const id;
	CryIcon const&  icon;
};

using SControlInfos = std::vector<SControlInfo>;
} // namespace ACE
