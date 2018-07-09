// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/ICore.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySystem/ISystem.h"

namespace Schematyc
{

// Forward declare interfaces.
struct ILogOutput;
// Forward declare classes.
class CCompiler;
class CEnvRegistry;
class CLog;
class CLogRecorder;
class CObjectPool;
class CRuntimeRegistry;
class CScriptRegistry;
class CSettingsManager;
class CTimerSystem;
class CUpdateScheduler;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(ILogOutput)

class CCore : public ICrySchematycCore, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(Cry::IDefaultModule)
	CRYINTERFACE_ADD(ICrySchematycCore)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CCore, "EngineModule_SchematycCore", "96d98d98-35aa-4fb6-830b-53dbfe71908d"_cry_guid)

public:

	CCore();

	virtual ~CCore();

	// Cry::IDefaultModule
	virtual const char* GetName() const override;
	virtual const char* GetCategory() const override;
	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~Cry::IDefaultModule

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// ICrySchematycCore
	virtual void                     SetGUIDGenerator(const GUIDGenerator& guidGenerator) override;
	virtual CryGUID                  CreateGUID() const override;

	virtual const char*              GetRootFolder() const override;
	virtual const char*              GetScriptsFolder() const override;
	virtual const char*              GetSettingsFolder() const override;
	virtual bool                     IsExperimentalFeatureEnabled(const char* szFeatureName) const override;

	virtual IEnvRegistry&            GetEnvRegistry() override;
	virtual IScriptRegistry&         GetScriptRegistry() override;
	virtual IRuntimeRegistry&        GetRuntimeRegistry() override;
	virtual ICompiler&               GetCompiler() override;
	virtual ILog&                    GetLog() override;
	virtual ILogRecorder&            GetLogRecorder() override;
	virtual ISettingsManager&        GetSettingsManager() override;
	virtual IUpdateScheduler&        GetUpdateScheduler() override;
	virtual ITimerSystem&            GetTimerSystem() override;

	virtual IValidatorArchivePtr     CreateValidatorArchive(const SValidatorArchiveParams& params) const override;
	virtual ISerializationContextPtr CreateSerializationContext(const SSerializationContextParams& params) const override;
	virtual IScriptViewPtr           CreateScriptView(const CryGUID& scopeGUID) const override;

	virtual bool                     CreateObject(const Schematyc::SObjectParams& params, IObject*& pObjectOut) override;
	virtual IObject*                 GetObject(ObjectId objectId) override;
	virtual void                     DestroyObject(ObjectId objectId) override;
	virtual void                     SendSignal(ObjectId objectId, const SObjectSignal& signal) override;
	virtual void                     BroadcastSignal(const SObjectSignal& signal) override;

	virtual void                     RefreshLogFileSettings() override;
	virtual void                     RefreshEnv() override;

	virtual void                     PrePhysicsUpdate() override;
	virtual void                     Update() override;
	// ~ICrySchematycCore

	void              RefreshLogFileStreams();
	void              RefreshLogFileMessageTypes();

	CRuntimeRegistry& GetRuntimeRegistryImpl();
	CCompiler&        GetCompilerImpl();

	static CCore&     GetInstance();

private:

	void LoadProjectFiles();

private:

	static CCore*                     s_pInstance;

	GUIDGenerator                     m_guidGenerator;
	mutable string                    m_scriptsFolder;    // #SchematycTODO : How can we avoid making this mutable?
	mutable string                    m_settingsFolder;   // #SchematycTODO : How can we avoid making this mutable?
	std::unique_ptr<CEnvRegistry>     m_pEnvRegistry;
	std::unique_ptr<CScriptRegistry>  m_pScriptRegistry;
	std::unique_ptr<CRuntimeRegistry> m_pRuntimeRegistry;
	std::unique_ptr<CCompiler>        m_pCompiler;
	std::unique_ptr<CObjectPool>      m_pObjectPool;
	std::unique_ptr<CTimerSystem>     m_pTimerSystem;
	std::unique_ptr<CLog>             m_pLog;
	ILogOutputPtr                     m_pLogFileOutput;
	std::unique_ptr<CLogRecorder>     m_pLogRecorder;
	std::unique_ptr<CSettingsManager> m_pSettingsManager;
	std::unique_ptr<CUpdateScheduler> m_pUpdateScheduler;
};

} // Schematyc
