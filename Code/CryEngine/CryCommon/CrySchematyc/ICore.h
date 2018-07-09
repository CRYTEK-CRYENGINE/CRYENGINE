// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySystem/ICryPlugin.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Delegate.h"

namespace Schematyc
{

// Forward declare interfaces.
struct ICompiler;
struct IScriptView;
struct IEnvRegistrar;
struct IEnvRegistry;
struct ILog;
struct ILogRecorder;
struct IObject;
struct IRuntimeRegistry;
struct IScriptRegistry;
struct ISerializationContext;
struct ISettingsManager;
struct ITimerSystem;
struct IUpdateScheduler;
struct IValidatorArchive;
struct IObjectProperties;
// Forward declare structures.
struct SObjectParams;
struct SObjectSignal;
struct SSerializationContextParams;
struct SValidatorArchiveParams;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IScriptView)
DECLARE_SHARED_POINTERS(ISerializationContext)
DECLARE_SHARED_POINTERS(IValidatorArchive)
DECLARE_SHARED_POINTERS(IObjectProperties);

typedef std::function<CryGUID()> GUIDGenerator;

struct SObjectParams
{
	CryGUID              classGUID;
	void*                pCustomData = nullptr;
	IObjectPropertiesPtr pProperties;
	IEntity*             pEntity = nullptr;
};

} // Schematyc

struct ICrySchematycCore : public Cry::IDefaultModule
{
	CRYINTERFACE_DECLARE_GUID(ICrySchematycCore, "041b8bda-35d7-4341-bde7-f0ca69be2595"_cry_guid)

	virtual void                                SetGUIDGenerator(const Schematyc::GUIDGenerator& guidGenerator) = 0;
	virtual CryGUID                             CreateGUID() const = 0;

	virtual const char*                         GetRootFolder() const = 0;
	virtual const char*                         GetScriptsFolder() const = 0;     // #SchematycTODO : Do we really need access to this outside script registry?
	virtual const char*                         GetSettingsFolder() const = 0;    // #SchematycTODO : Do we really need access to this outside env registry?
	virtual bool                                IsExperimentalFeatureEnabled(const char* szFeatureName) const = 0;

	virtual Schematyc::IEnvRegistry&            GetEnvRegistry() = 0;
	virtual Schematyc::IScriptRegistry&         GetScriptRegistry() = 0;
	virtual Schematyc::IRuntimeRegistry&        GetRuntimeRegistry() = 0;
	virtual Schematyc::ICompiler&               GetCompiler() = 0;
	virtual Schematyc::ILog&                    GetLog() = 0;
	virtual Schematyc::ILogRecorder&            GetLogRecorder() = 0;
	virtual Schematyc::ISettingsManager&        GetSettingsManager() = 0;
	virtual Schematyc::IUpdateScheduler&        GetUpdateScheduler() = 0;
	virtual Schematyc::ITimerSystem&            GetTimerSystem() = 0;

	virtual Schematyc::IValidatorArchivePtr     CreateValidatorArchive(const Schematyc::SValidatorArchiveParams& params) const = 0;
	virtual Schematyc::ISerializationContextPtr CreateSerializationContext(const Schematyc::SSerializationContextParams& params) const = 0;
	virtual Schematyc::IScriptViewPtr           CreateScriptView(const CryGUID& scopeGUID) const = 0;

	virtual bool                                CreateObject(const Schematyc::SObjectParams& params, Schematyc::IObject*& pObjectOut) = 0;
	virtual Schematyc::IObject*                 GetObject(Schematyc::ObjectId objectId) = 0;
	virtual void                                DestroyObject(Schematyc::ObjectId objectId) = 0;
	virtual void                                SendSignal(Schematyc::ObjectId objectId, const Schematyc::SObjectSignal& signal) = 0;
	virtual void                                BroadcastSignal(const Schematyc::SObjectSignal& signal) = 0;

	virtual void                                RefreshLogFileSettings() = 0;
	virtual void                                RefreshEnv() = 0;

	virtual void                                PrePhysicsUpdate() = 0;
	virtual void                                Update() = 0;
};
