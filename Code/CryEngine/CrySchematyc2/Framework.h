// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ClassWeaver.h>
#include <CrySchematyc2/IFramework.h>

#include "Compiler.h"
#include "LibRegistry.h"
#include "ObjectManager.h"
#include "Deprecated/StringPool.h"
#include "Env/EnvRegistry.h"
#include "Script/ScriptRegistry.h"
#include "Services/Log.h"
#include "Services/TimerSystem.h"
#include "Services/UpdateScheduler.h"

namespace SchematycBaseEnv
{
	class CBaseEnv;
}

namespace Schematyc2
{
	class CFramework final : public IFramework
	{
		CRYINTERFACE_BEGIN()
			CRYINTERFACE_ADD(Cry::IDefaultModule)
			CRYINTERFACE_ADD(CFramework)
		CRYINTERFACE_END()

		CRYGENERATE_SINGLETONCLASS_GUID(CFramework, "EngineModule_Schematyc2", "{5E7CED55-5C30-44CF-9AD5-A80B025B4800}"_cry_guid)

		CFramework();
		~CFramework();

	public:
		virtual const char* GetName() const override { return "CrySchematyc2"; }
		virtual const char* GetCategory() const override { return "CryEngine"; }

		virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

		// IFramework

		virtual void SetGUIDGenerator(const GUIDGenerator& guidGenerator) override;
		virtual SGUID CreateGUID() const override;
		
		virtual IStringPool& GetStringPool() override;
		virtual IEnvRegistry& GetEnvRegistry() override;
		virtual SchematycBaseEnv::IBaseEnv& GetBaseEnv() override;
		virtual ILibRegistry& GetLibRegistry() override;
		
		virtual const char* GetFileFormat() const override;
		virtual const char* GetRootFolder() const override;
		virtual const char* GetOldScriptsFolder() const override;
		virtual const char* GetOldScriptExtension() const override;
		virtual const char* GetScriptsFolder() const override;
		virtual const char* GetSettingsFolder() const override;
		virtual const char* GetSettingsExtension() const override;
		virtual bool IsExperimentalFeatureEnabled(const char* szFeatureName) const override;

		virtual IScriptRegistry& GetScriptRegistry() override;
		virtual ICompiler& GetCompiler() override;
		virtual IObjectManager& GetObjectManager() override;
		virtual ILog& GetLog() override;
		virtual ILogRecorder& GetLogRecorder() override;
		virtual IUpdateScheduler& GetUpdateScheduler() override;
		virtual ITimerSystem& GetTimerSystem() override;
		
		virtual ISerializationContextPtr CreateSerializationContext(const SSerializationContextParams& params) const override;
		virtual IDomainContextPtr CreateDomainContext(const SDomainContextScope& scope) const override;
		virtual IValidatorArchivePtr CreateValidatorArchive(const SValidatorArchiveParams& params) const override;

		virtual IGameResourceListPtr CreateGameResoucreList() const override;
		virtual IResourceCollectorArchivePtr CreateResourceCollectorArchive(IGameResourceListPtr pResourceList) const override;
		
		virtual void RefreshLogFileSettings() override;
		virtual void RefreshEnv() override;

		virtual SFrameworkSignals& Signals() override;

		virtual void PrePhysicsUpdate() override;
		virtual void Update() override;
		virtual void SetUpdateRelevancyContext(CUpdateRelevanceContext& context) override { m_updateRelevanceContext = context; }
		
		// ~IFramework

		void RefreshLogFileConfiguration();
		void RefreshLogFileStreams();
		void RefreshLogFileMessageTypes();

	private:

		GUIDGenerator     m_guidGenerator;
		CStringPool       m_stringPool;
		CEnvRegistry      m_envRegistry;
		CLibRegistry      m_libRegistry;
		CScriptRegistry   m_scriptRegistry;
		mutable string    m_oldScriptsFolder; // #SchematycTODO : How can we avoid making this mutable?
		mutable string    m_scriptsFolder; // #SchematycTODO : How can we avoid making this mutable?
		mutable string    m_settingsFolder; // #SchematycTODO : How can we avoid making this mutable?
		CCompiler         m_compiler;
		CTimerSystem      m_timerSystem; // #SchematycTODO : Shutdown is reliant on order of destruction so we should formalize this rather than relying on placement within CFramework class.
		CObjectManager    m_objectManager;
		CLog              m_log;
		ILogOutputPtr     m_pLogFileOutput;
		CLogRecorder      m_logRecorder;
		CUpdateScheduler  m_updateScheduler;
		SFrameworkSignals m_signals;
		std::unique_ptr<SchematycBaseEnv::CBaseEnv> m_pBaseEnv;
		CUpdateRelevanceContext m_updateRelevanceContext;
	};
}
