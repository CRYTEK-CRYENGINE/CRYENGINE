// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc2/IFramework.h>

#include "Bridge_ICompiler.h"
#include "Bridge_IDomainContext.h"
#include "Bridge_IScriptRegistry.h"

namespace Bridge {

struct IFramework : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(Bridge::IFramework, "{C2D28CFF-542F-448E-9499-653C4077F28F}"_cry_guid);

	// #SchematycTODO : Clean up this interface!

	virtual void SetGUIDGenerator(const Schematyc2::GUIDGenerator& guidGenerator) = 0;
	virtual Schematyc2::SGUID CreateGUID() const = 0;

	virtual Schematyc2::IStringPool& GetStringPool() = 0;
	virtual Schematyc2::IEnvRegistry& GetEnvRegistry() = 0;
	virtual Schematyc2::ILibRegistry& GetLibRegistry() = 0;

	virtual const char* GetFileFormat() const = 0;
	virtual const char* GetRootFolder() const = 0;
	virtual const char* GetOldScriptsFolder() const = 0; 
	virtual const char* GetOldScriptExtension() const = 0;
	virtual const char* GetScriptsFolder() const = 0; // #SchematycTODO : Do we really need access to this outside script registry?
	virtual const char* GetSettingsFolder() const = 0; // #SchematycTODO : Do we really need access to this outside env registry?
	virtual const char* GetSettingsExtension() const = 0; // #SchematycTODO : Use GetFileFormat() instead?
	virtual bool IsExperimentalFeatureEnabled(const char* szFeatureName) const = 0;

	virtual             IScriptRegistry& GetScriptRegistry() = 0;
	virtual             ICompiler& GetCompiler() = 0;
	virtual Schematyc2::IObjectManager& GetObjectManager() = 0;
	virtual Schematyc2::ILog& GetLog() = 0;
	virtual Schematyc2::ILogRecorder& GetLogRecorder() = 0;
	virtual Schematyc2::IUpdateScheduler& GetUpdateScheduler() = 0;
	virtual Schematyc2::ITimerSystem& GetTimerSystem() = 0;

	virtual Schematyc2::ISerializationContextPtr CreateSerializationContext(const Schematyc2::SSerializationContextParams& params) const = 0;		
	virtual             IDomainContextPtr CreateDomainContext(const SDomainContextScope& scope) const = 0;
	virtual             IDomainContextPtr CreateDomainContext(const Schematyc2::IScriptElement* element) = 0;
	virtual Schematyc2::IValidatorArchivePtr CreateValidatorArchive(const Schematyc2::SValidatorArchiveParams& params) const = 0;

	virtual Schematyc2::IGameResourceListPtr CreateGameResoucreList() const = 0;
	virtual Schematyc2::IResourceCollectorArchivePtr CreateResourceCollectorArchive(Schematyc2::IGameResourceListPtr pResourceList) const = 0;

	virtual void RefreshLogFileSettings() = 0;
	virtual void RefreshEnv() = 0;

	virtual Schematyc2::SFrameworkSignals& Signals() = 0;

	virtual void PrePhysicsUpdate() = 0;
	virtual void Update() = 0;
	virtual void SetUpdateRelevancyContext(Schematyc2::CUpdateRelevanceContext& context) = 0;
};

}
