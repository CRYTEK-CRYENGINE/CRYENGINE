// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplItem.h>
#include "ImplTypes.h"

namespace ACE
{
namespace Wwise
{
class CImplControl final : public CImplItem
{
public:

	CImplControl() = default;
	CImplControl(string const& name, CID const id, ItemType const type);
};
} // namespace Wwise
} // namespace ACE
