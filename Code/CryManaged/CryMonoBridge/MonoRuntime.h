// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMono/IMonoRuntime.h>

#include <CrySystem/IConsole.h>
#include <CryCore/Containers/CryListenerSet.h>
#include <CryAISystem/BehaviorTree/IBehaviorTree.h>
#include <CryAISystem/BehaviorTree/Node.h>

#include <CrySystem/IEngineModule.h>
#include <CryExtension/ICryFactory.h>
#include <CryExtension/ClassWeaver.h>

#include "Wrappers/MonoObject.h"

#include "NativeComponents/ManagedEntityComponent.h"

class CMonoDomain;
class CRootMonoDomain;
class CAppDomain;

class CMonoLibrary;

class CManagedPlugin;

typedef CListenerSet<IMonoListener*>            MonoListeners;

enum EMonoLogLevel
{
	eMLL_NULL = 0,
	eMLL_Error,
	eMLL_Critical,
	eMLL_Warning,
	eMLL_Message,
	eMLL_Info,
	eMLL_Debug
};

class CManagedNodeCreatorProxy : public BehaviorTree::INodeCreator
{
public:
	CManagedNodeCreatorProxy(const char* szTypeName, IManagedNodeCreator* pCreator)
		: m_pCreator(pCreator)
		, m_pNodeFactory(nullptr) 
	{ 
		cry_strcpy(m_szTypeName, szTypeName);
	}

	// INodeCreator
	virtual ~CManagedNodeCreatorProxy()
	{
		delete m_pCreator;
	}
	virtual BehaviorTree::INodePtr    Create() override
	{
		return BehaviorTree::INodePtr(m_pCreator->Create());
	}
	virtual void        Trim() override {}
	virtual const char* GetTypeName() const override { return m_szTypeName; }
	virtual size_t      GetNodeClassSize() const override { return 4; }
	virtual size_t      GetSizeOfImmutableDataForAllAllocatedNodes() const override { return 4; }
	virtual size_t      GetSizeOfRuntimeDataForAllAllocatedNodes() const override { return 4; }
	virtual void*       AllocateRuntimeData(const BehaviorTree::RuntimeDataID runtimeDataID) override { return nullptr; }
	virtual void*       GetRuntimeData(const BehaviorTree::RuntimeDataID runtimeDataID) const override { return nullptr; }
	virtual void        FreeRuntimeData(const BehaviorTree::RuntimeDataID runtimeDataID) override { }
	virtual void        SetNodeFactory(BehaviorTree::INodeFactory* nodeFactory) override { m_pNodeFactory = nodeFactory; }
	// ~INodeCreator

private:
	IManagedNodeCreator* m_pCreator;
	char m_szTypeName[64];
	BehaviorTree::INodeFactory* m_pNodeFactory;
};

class CMonoRuntime final
	: public IMonoEngineModule
	, public IManagedConsoleCommandListener
	, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(IMonoEngineModule)
	CRYINTERFACE_END()
	CRYGENERATE_SINGLETONCLASS_GUID(CMonoRuntime, "EngineModule_CryMonoBridge", "2b4615a5-7152-4d67-920d-c857f8503b3a"_cry_guid)

public:
	CMonoRuntime();
	virtual ~CMonoRuntime();

	// IMonoEngineModule
	virtual const char* GetName() const override { return "CryMonoBridge"; }
	virtual const char* GetCategory() const override { return "CryEngine"; }

	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	virtual void Shutdown() override;

	virtual std::shared_ptr<ICryPlugin> LoadBinary(const char* szBinaryPath) override;

	virtual void                        Update(int updateFlags, int nPauseMode) override;

	virtual void                        RegisterListener(IMonoListener* pListener) override { m_listeners.Add(pListener); }
	virtual void                        UnregisterListener(IMonoListener* pListener) override { m_listeners.Remove(pListener); }

	virtual CRootMonoDomain*            GetRootDomain() override;
	virtual CMonoDomain*                GetActiveDomain() override;
	virtual CAppDomain*                 CreateDomain(char* name, bool bActivate = false) override;
	virtual void                        ReloadPluginDomain() override;

	virtual CMonoLibrary*               GetCryCommonLibrary() const override;
	virtual CMonoLibrary*               GetCryCoreLibrary() const override;

	virtual void						RegisterNativeToManagedInterface(IMonoNativeToManagedInterface& interface) override;

	virtual void                        RegisterManagedNodeCreator(const char* szClassName, IManagedNodeCreator* pCreator) override;
	// ~IMonoEngineModule

	// IManagedConsoleCommandListener
	virtual void OnManagedConsoleCommandEvent(const char* szCommandName, IConsoleCmdArgs* pConsoleCommandArguments) override;
	// ~IManagedConsoleCommandListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	CMonoDomain* FindDomainByHandle(MonoInternals::MonoDomain* pDomain);

	CAppDomain*  LaunchPluginDomain();
	CAppDomain*  GetPluginDomain() const { return m_pPluginDomain; }

	void         HandleException(MonoInternals::MonoException* pException);

private:
	static void MonoLogCallback(const char* szLogDomain, const char* szLogLevel, const char* szMessage, MonoInternals::mono_bool is_fatal, void* pUserData);
	static void MonoPrintCallback(const char* szMessage, MonoInternals::mono_bool is_stdout);
	static void MonoPrintErrorCallback(const char* szMessage, MonoInternals::mono_bool is_stdout);

	void RegisterInternalInterfaces();

	void InvokeManagedConsoleCommandNotification(const char* szCommandName, IConsoleCmdArgs* pConsoleCommandArguments);

private:
	std::unordered_map<MonoInternals::MonoDomain*, std::shared_ptr<CMonoDomain>> m_domainLookupMap;
	std::vector<std::shared_ptr<CManagedNodeCreatorProxy>> m_nodeCreators;

	std::shared_ptr<CRootMonoDomain>         m_pRootDomain;
	CAppDomain*              m_pPluginDomain;
	std::vector<std::weak_ptr<CManagedPlugin>> m_plugins;

	// Plug-in compiled from source code in assets directory
	std::shared_ptr<CManagedPlugin> m_pAssetsPlugin;
	
	CMonoLibrary*            m_pLibCommon;
	CMonoLibrary*            m_pLibCore;

	MonoListeners            m_listeners;
};

inline static CMonoRuntime* GetMonoRuntime()
{
	return static_cast<CMonoRuntime*>(gEnv->pMonoRuntime);
}