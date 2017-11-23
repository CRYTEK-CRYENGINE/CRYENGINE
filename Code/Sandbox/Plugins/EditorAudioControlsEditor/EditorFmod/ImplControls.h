// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ImplItem.h>
#include "ImplTypes.h"

namespace ACE
{
namespace Fmod
{
class CImplControl final : public CImplItem
{
public:

	CImplControl() = default;
	CImplControl(string const& name, CID const id, ItemType const type);
};

class CImplFolder final : public CImplItem
{
public:

	CImplFolder(string const& name, CID const id)
		: CImplItem(name, id, static_cast<ItemType>(EImpltemType::Folder))
	{
		SetContainer(true);
	}
};

class CImplGroup final : public CImplItem
{
public:

	CImplGroup(string const& name, CID const id)
		: CImplItem(name, id, static_cast<ItemType>(EImpltemType::Group))
	{
		SetContainer(true);
	}
};
} // namespace Fmod
} // namespace ACE
