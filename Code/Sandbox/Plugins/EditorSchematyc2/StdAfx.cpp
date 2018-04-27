// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef CRY_USE_SCHEMATYC2_BRIDGE
#include "Bridge_Framework.h"
Bridge::IFramework* GetBridge()
{
	static std::shared_ptr<Bridge::IFramework> pFramework(new Bridge::CFramework());
	return pFramework.get();
}
#endif //CRY_USE_SCHEMATYC2_BRIDGE