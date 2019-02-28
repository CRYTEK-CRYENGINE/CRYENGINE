// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IResourceCompiler;
class XmlNodeRef;

namespace AssetManager
{

bool CollectDDSImageDetails(XmlNodeRef& xmlnode, const char* szFilename, IResourceCompiler* pRc);

}
