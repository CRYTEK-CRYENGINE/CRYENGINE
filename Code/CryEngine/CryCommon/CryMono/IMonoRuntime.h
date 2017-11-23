// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include <CrySystem/ICryPlugin.h>

class CRootMonoDomain;
class CAppDomain;
class CMonoDomain;
class CMonoLibrary;

struct IMonoNativeToManagedInterface;

namespace BehaviorTree { class Node; }

struct IManagedPlugin
{
	virtual ~IManagedPlugin() {}

	virtual void Load(CAppDomain* pDomain) = 0;
	virtual void OnCoreLibrariesDeserialized() = 0;
	virtual void OnPluginLibrariesDeserialized() = 0;
};

struct IMonoListener
{
	virtual ~IMonoListener() {}

	//! Forwarded from CrySystem
	//! \param flags One or more flags from ESystemUpdateFlags structure.
	//! \param nPauseMode 0=normal(no pause), 1=menu/pause, 2=cutscene.
	virtual void OnUpdate(int updateFlags, int nPauseMode) = 0;
};

struct IMonoCompileListener
{
	virtual ~IMonoCompileListener() {}

	virtual void OnCompileFinished(const char* szCompileMessage) = 0;
};

struct IManagedNodeCreator
{
	IManagedNodeCreator() {}

	virtual BehaviorTree::Node* Create() = 0;
};

struct IMonoEngineModule : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(IMonoEngineModule, "ae47c989-0ffa-4876-b0b5-fbb833c2b4ef"_cry_guid);

	virtual void                        Shutdown() = 0;

	virtual std::shared_ptr<Cry::IEnginePlugin> LoadBinary(const char* szBinaryPath) = 0;
	
	virtual void                        Update(int updateFlags = 0, int nPauseMode = 0) = 0;

	virtual void                        RegisterListener(IMonoListener* pListener) = 0;
	virtual void                        UnregisterListener(IMonoListener* pListener) = 0;

	virtual CRootMonoDomain*            GetRootDomain() = 0;
	virtual CMonoDomain*                GetActiveDomain() = 0;
	virtual CAppDomain*                 CreateDomain(char* name, bool bActivate = false) = 0;
	virtual void                        ReloadPluginDomain() = 0;

	virtual CMonoLibrary*               GetCryCommonLibrary() const = 0;
	virtual CMonoLibrary*               GetCryCoreLibrary() const = 0;

	virtual void                        RegisterNativeToManagedInterface(IMonoNativeToManagedInterface& managedInterface) = 0;

	virtual void                        RegisterManagedNodeCreator(const char* szClassName, IManagedNodeCreator* pCreator) = 0;

	virtual void                        RegisterCompileListener(IMonoCompileListener* pListener) = 0;
	virtual void                        UnregisterCompileListener(IMonoCompileListener* pListener) = 0;

	virtual const char*                 GetLatestCompileMessage() = 0;
};