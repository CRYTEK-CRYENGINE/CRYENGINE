// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/XML/IXml.h>
#include "ACETypes.h"

namespace ACE
{
class IAudioSystemItem;

class CProjectLoader
{
public:
	CProjectLoader(const string& sAssetsPath, IAudioSystemItem& rootItem);

private:
	IAudioSystemItem* CreateItem(const string& name, const string& path, ItemType type, IAudioSystemItem& rootItem);
	void              LoadFolder(const string& folderPath, IAudioSystemItem& parent);

	string m_assetsPath;
};
}
