// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_ILIVECREATEPLATFORM_H_
#define _H_ILIVECREATEPLATFORM_H_

#include "ILiveCreateCommon.h"

namespace LiveCreate
{
//! How to reset the target platform.
enum EResetMode
{
	eResetMode_Soft,
	eResetMode_Title,
	eResetMode_Hard
};

//! Folder scan interface.
struct IPlatformHandlerFolderScan
{
	virtual ~IPlatformHandlerFolderScan() {};
	virtual void OnFolder(const char* szBasePath, const char* szEntryName) = 0;
	virtual void OnFile(const char* szBasePath, const char* szEntryName, bool bIsExecutable) = 0;
};

//! This interface is managing a target machine capable of running the game.
struct IPlatformHandler
{
	enum EFlags
	{
		//! This is set when the console platform has a slow power state check function, when the IsPoweredOn() call is slow.
		//! Used by editor to put these kind of checks on a thread.
		eFlag_HasSlowPowerStateCheck = 1 << 0,

		//! This flags tells us that the handler is not capable to find the game builds on the machine.
		eFlag_NoAutoFindBuilds = 1 << 1,

		//! Is the platform BigEndian?
		eFlag_BigEndian = 1 << 2,

		//! Does the platform require a reset before launching the game?
		eFlag_RequiresResetBeforeLaunch = 1 << 3,

		//! Is platform using shared data directory with the Editor?
		eFlag_SharedDataDirectory = 1 << 4,
	};

	// <interfuscator:shuffle>
	virtual ~IPlatformHandler(){}

	//! Check if platform flag is set.
	virtual bool IsFlagSet(uint32 aFlag) const = 0;

	//! Check if the target machine is on (sometimes slow to very slow).
	virtual bool IsOn() const = 0;

	//! Get target host name.
	virtual const char* GetTargetName() const = 0;

	//! Get platform name.
	virtual const char* GetPlatformName() const = 0;

	//! Get platform factory that created this platform handler.
	virtual IPlatformHandlerFactory* GetFactory() const = 0;

	//! Launch remote executable with given parameters.
	virtual bool Launch(const char* pExeFilename, const char* pWorkingFolder, const char* pArgs) const = 0;

	//! Try to resolve the target name to a valid IP address (very slow).
	virtual bool ResolveAddress(char* pOutIpAddress, uint32 aMaxOutIpAddressSize) const = 0;

	//! Reset target.
	virtual bool Reset(EResetMode aMode) const = 0;

	//! Get platform root path (absolute directory considered as the root of the platform filesystem).
	virtual const char* GetRootPath() const = 0;

	//! Scan content of given path on target. This collects all the folder entries in one call (slow).
	virtual bool ScanFolder(const char* pFolder, IPlatformHandlerFolderScan& outResult) const = 0;

	//! Add object reference (Reference counted interface).
	virtual void AddRef() = 0;

	//! Remove object reference.
	virtual void Release() = 0;
};

//! Factory for platform handlers.
struct IPlatformHandlerFactory
{
	struct TargetInfo
	{
		static const int         kMaxTargetNameSize = 64;

		IPlatformHandlerFactory* pFactory;
		char                     targetName[kMaxTargetNameSize];
	};

	// <interfuscator:shuffle>
	virtual ~IPlatformHandlerFactory() {}

	//! Returns the platform name handled by this factory (GamePC, EditorPC, etc.).
	virtual const char* GetPlatformName() const = 0;

	//! Find the machine's IP address by its target name, returns true if resolving was successful.
	virtual bool ResolveAddress(const char* pTargetMachineName, char* pOutIpAddress, uint32 aMaxOutIpAddressSize) = 0;

	//! Create a platform handler for given target name, address.
	virtual IPlatformHandler* CreatePlatformHandlerInstance(const char* pTargetMachineName) = 0;

	//! Gather possible targets for this platform, returns number of targets discovered.
	virtual uint32 ScanForTargets(TargetInfo* outTargets, const uint maxTargets) = 0;

	// </interfuscator:shuffle>
};
}

#endif
