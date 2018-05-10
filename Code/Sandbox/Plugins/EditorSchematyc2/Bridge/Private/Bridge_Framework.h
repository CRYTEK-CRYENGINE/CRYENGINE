// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ClassWeaver.h>

#include "Bridge_Compiler.h"
#include "Bridge_IFramework.h"
#include "Script/Bridge_ScriptRegistry.h"


namespace Schematyc2 {

struct IFramework;

};

namespace Bridge {

class CFramework final : public IFramework
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(Cry::IDefaultModule)
		CRYINTERFACE_ADD(Bridge::CFramework)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(Bridge::CFramework, "Bridge", "{5E7CED55-5C30-44CF-9AD5-A80B025B4801}"_cry_guid)

public:

	virtual const char* GetName() const override { return "Bridge"; }
	virtual const char* GetCategory() const override { return "Plugin"; }

	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

	// IFramework

	virtual void SetGUIDGenerator(const Schematyc2::GUIDGenerator& guidGenerator) override;
	virtual Schematyc2::SGUID CreateGUID() const override;

	virtual Schematyc2::IStringPool& GetStringPool() override;
	virtual Schematyc2::IEnvRegistry& GetEnvRegistry() override;
	virtual Schematyc2::ILibRegistry& GetLibRegistry() override;

	virtual const char* GetFileFormat() const override;
	virtual const char* GetRootFolder() const override;
	virtual const char* GetOldScriptsFolder() const override;
	virtual const char* GetOldScriptExtension() const override;
	virtual const char* GetScriptsFolder() const override;
	virtual const char* GetSettingsFolder() const override;
	virtual const char* GetSettingsExtension() const override;
	virtual bool IsExperimentalFeatureEnabled(const char* szFeatureName) const override;

	virtual             IScriptRegistry& GetScriptRegistry() override;
	virtual             ICompiler& GetCompiler() override;
	virtual Schematyc2::IObjectManager& GetObjectManager() override;
	virtual Schematyc2::ILog& GetLog() override;
	virtual Schematyc2::ILogRecorder& GetLogRecorder() override;
	virtual Schematyc2::IUpdateScheduler& GetUpdateScheduler() override;
	virtual Schematyc2::ITimerSystem& GetTimerSystem() override;

	virtual Schematyc2::ISerializationContextPtr CreateSerializationContext(const Schematyc2::SSerializationContextParams& params) const override;
	virtual             IDomainContextPtr CreateDomainContext(const SDomainContextScope& scope) const override;
	virtual             IDomainContextPtr CreateDomainContext(const Schematyc2::IScriptElement* element) override;
	virtual Schematyc2::IValidatorArchivePtr CreateValidatorArchive(const Schematyc2::SValidatorArchiveParams& params) const override;

	virtual Schematyc2::IGameResourceListPtr CreateGameResoucreList() const override;
	virtual Schematyc2::IResourceCollectorArchivePtr CreateResourceCollectorArchive(Schematyc2::IGameResourceListPtr pResourceList) const override;

	virtual void RefreshLogFileSettings() override;
	virtual void RefreshEnv() override;

	virtual Schematyc2::SFrameworkSignals& Signals() override;

	virtual void PrePhysicsUpdate() override;
	virtual void Update() override;
	virtual void SetUpdateRelevancyContext(Schematyc2::CUpdateRelevanceContext& context) override;

private:
	Schematyc2::IFramework* Delegate() const;

private:
	CCompiler       m_compiler;
	CScriptRegistry m_scriptRegistry;

};

}