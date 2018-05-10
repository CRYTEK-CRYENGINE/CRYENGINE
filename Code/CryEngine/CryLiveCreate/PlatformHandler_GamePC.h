// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _LIVECREATE_PLATFORMHANDLER_GAMEPC_H_
#define _LIVECREATE_PLATFORMHANDLER_GAMEPC_H_

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <CryLiveCreate/ILiveCreateCommon.h>
#include "Platform_Common/PlatformHandlerBase.h"

#if defined(LIVECREATE_FOR_PC) && !defined(NO_LIVECREATE)

namespace LiveCreate
{

// Local platform wrapper, assumes game is launched from the same directory as editor
class PlatformHandler_GamePC : public PlatformHandlerBase
{
private:
	string m_exeDirectory;
	string m_baseDirectory;
	string m_rootDirectory; // only the drive name

public:
	PlatformHandler_GamePC(IPlatformHandlerFactory* pFactory, const char* szTargetName);
	virtual ~PlatformHandler_GamePC();

	// IPlatformHandler interface implementation
	virtual void        Delete();
	virtual bool        IsOn() const;
	virtual bool        Launch(const char* pExeFilename, const char* pWorkingFolder, const char* pArgs) const;
	virtual bool        Reset(EResetMode aMode) const;
	virtual bool        ScanFolder(const char* pFolder, IPlatformHandlerFolderScan& outResult) const;
	virtual const char* GetRootPath() const;
};

// Local platform factory wrapper, supports local PC only
class PlatformHandlerFactory_GamePC : public IPlatformHandlerFactory
{
public:
	// IPlatformHandlerFactory interface implementation
	virtual const char*       GetPlatformName() const;
	virtual IPlatformHandler* CreatePlatformHandlerInstance(const char* pTargetMachineName);
	virtual uint32            ScanForTargets(TargetInfo* outTargets, const uint maxTargets);
	virtual bool              ResolveAddress(const char* pTargetMachineName, char* pOutIpAddress, uint32 aMaxOutIpAddressSize);
};

}

#endif
#endif
