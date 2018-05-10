// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Item.h"

#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
class CProjectLoader final
{
public:

	explicit CProjectLoader(string const& sAssetsPath, CItem& rootItem);

	CProjectLoader() = delete;

private:

	CItem* CreateItem(string const& name, string const& path, EItemType const type, CItem& rootItem);
	void   LoadFolder(string const& folderPath, CItem& parent);

	string const m_assetsPath;
};
} // namespace PortAudio
} // namespace Impl
} // namespace ACE

