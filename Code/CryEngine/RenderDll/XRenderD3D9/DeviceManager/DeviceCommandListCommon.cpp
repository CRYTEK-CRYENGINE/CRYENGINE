// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <StdAfx.h>

#include "DeviceCommandListCommon.h"

void CDeviceCommandList::Close()
{
#if 0
	ERenderListID renderList = EFSLIST_INVALID; // TODO: set to correct renderlist
	bool bAsynchronous = false; // TODO set correct asynchronous mode

	gcpRendD3D->AddRecordedProfilingStats(EndProfilingSection(), renderList, bAsynchronous)
#endif

	CloseImpl();
}
