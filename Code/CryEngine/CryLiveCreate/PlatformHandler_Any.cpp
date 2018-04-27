// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "PlatformHandler_Any.h"

#if defined(LIVECREATE_FOR_PC) && !defined(NO_LIVECREATE)

namespace LiveCreate
{

//-----------------------------------------------------------------------------

PlatformHandler_Any::PlatformHandler_Any(IPlatformHandlerFactory* pFactory, const char* szTargetName)
	: PlatformHandlerBase(pFactory, szTargetName)
{
}

PlatformHandler_Any::~PlatformHandler_Any()
{
}

void PlatformHandler_Any::Delete()
{
	delete this;
}

bool PlatformHandler_Any::Launch(const char* pExeFilename, const char* pWorkingFolder, const char* pArgs) const
{
	return false;
}

bool PlatformHandler_Any::Reset(EResetMode aMode) const
{
	return false;
}

bool PlatformHandler_Any::IsOn() const
{
	return true;
}

bool PlatformHandler_Any::ScanFolder(const char* pFolder, IPlatformHandlerFolderScan& outResult) const
{
	return false;
}

const char* PlatformHandler_Any::GetRootPath() const
{
	return "";
}

//-----------------------------------------------------------------------------

const char* PlatformHandlerFactory_Any::GetPlatformName() const
{
	return "Any";
}

IPlatformHandler* PlatformHandlerFactory_Any::CreatePlatformHandlerInstance(const char* pTargetMachineName)
{
	return new PlatformHandler_Any(this, pTargetMachineName);
}

bool PlatformHandlerFactory_Any::ResolveAddress(const char* pTargetMachineName, char* pOutIpAddress, uint32 aMaxOutIpAddressSize)
{
	// make sure it's an IP
	int val[4];
	if (4 != sscanf(pTargetMachineName, "%d.%d.%d.%d", &val[0], &val[1], &val[2], &val[3]))
	{
		return false;
	}

	// check ip range
	if (val[0] < 0 || val[0] > 255) return false;
	if (val[1] < 0 || val[1] > 255) return false;
	if (val[2] < 0 || val[2] > 255) return false;
	if (val[3] < 0 || val[3] > 255) return false;

	cry_sprintf(pOutIpAddress, aMaxOutIpAddressSize, "%d.%d.%d.%d", val[0], val[1], val[2], val[3]);
	return true;
}

uint32 PlatformHandlerFactory_Any::ScanForTargets(TargetInfo* outTargets, const uint maxTargets)
{
	return 0;
}

//-----------------------------------------------------------------------------

}
#endif
