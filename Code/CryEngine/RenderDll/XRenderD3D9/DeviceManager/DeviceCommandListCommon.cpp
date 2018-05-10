// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include <StdAfx.h>

#include "DeviceCommandListCommon.h"

void CDeviceCommandList::Close()
{
#if 0
	ERenderListID renderList = EFSLIST_INVALID; // TODO: set to correct renderlist

#if defined(ENABLE_PROFILING_CODE)
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nNumPSOSwitches, &m_numPSOSwitches);
	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nNumLayoutSwitches, &m_numLayoutSwitches);
	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nNumResourceSetSwitches, &m_numResourceSetSwitches);
	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nNumInlineSets, &m_numInlineSets);
	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nDIPs, &m_numDIPs);
	CryInterlockedAdd(&gRenDev->m_RP.m_PS[gRenDev->GetRenderThreadID()].m_nPolygons[renderList], &m_numPolygons);
	gcpRendD3D->GetGraphicsPipeline().IncrementNumInvalidDrawcalls(m_numInvalidDIPs);
#endif
#endif

	CloseImpl();
}

