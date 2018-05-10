// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySchematyc2/IFramework.h>

#include "Bridge_Framework.h"
#include "Bridge_DomainContext.h"

namespace Bridge {

//////////////////////////////////////////////////////////////////////////
bool CFramework::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CFramework::SetGUIDGenerator(const Schematyc2::GUIDGenerator& guidGenerator)
{
	Delegate()->SetGUIDGenerator(guidGenerator);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::SGUID CFramework::CreateGUID() const
{
	return Delegate()->CreateGUID();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IStringPool& CFramework::GetStringPool()
{
	return Delegate()->GetStringPool();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IEnvRegistry& CFramework::GetEnvRegistry()
{
	return Delegate()->GetEnvRegistry();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ILibRegistry& CFramework::GetLibRegistry()
{
	return Delegate()->GetLibRegistry();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetFileFormat() const
{
	return Delegate()->GetFileFormat();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetRootFolder() const
{
	return Delegate()->GetRootFolder();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetOldScriptsFolder() const
{
	return Delegate()->GetOldScriptsFolder();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetOldScriptExtension() const
{
	return Delegate()->GetOldScriptExtension();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetScriptsFolder() const
{
	return Delegate()->GetScriptsFolder();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetSettingsFolder() const
{
	return Delegate()->GetSettingsFolder();
}

//////////////////////////////////////////////////////////////////////////
const char* CFramework::GetSettingsExtension() const
{
	return Delegate()->GetSettingsExtension();
}

//////////////////////////////////////////////////////////////////////////
bool CFramework::IsExperimentalFeatureEnabled(const char* szFeatureName) const
{
	return Delegate()->IsExperimentalFeatureEnabled(szFeatureName);
}

//////////////////////////////////////////////////////////////////////////
IScriptRegistry& CFramework::GetScriptRegistry()
{
	return m_scriptRegistry;
}

//////////////////////////////////////////////////////////////////////////
ICompiler& CFramework::GetCompiler()
{
	return m_compiler;
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IObjectManager& CFramework::GetObjectManager()
{
	return Delegate()->GetObjectManager();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ILog& CFramework::GetLog()
{
	return Delegate()->GetLog();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ILogRecorder& CFramework::GetLogRecorder()
{
	return Delegate()->GetLogRecorder();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IUpdateScheduler& CFramework::GetUpdateScheduler()
{
	return Delegate()->GetUpdateScheduler();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ITimerSystem& CFramework::GetTimerSystem()
{
	return Delegate()->GetTimerSystem();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::ISerializationContextPtr CFramework::CreateSerializationContext(const Schematyc2::SSerializationContextParams& params) const
{
	return Delegate()->CreateSerializationContext(params);
}

//////////////////////////////////////////////////////////////////////////
IDomainContextPtr CFramework::CreateDomainContext(const SDomainContextScope& bridgeScope) const
{
	Schematyc2::SDomainContextScope schematycScope(bridgeScope.pScriptFile->GetDelegate(), bridgeScope.guid);
	return IDomainContextPtr(new CDomainContext(Delegate()->CreateDomainContext(schematycScope), bridgeScope));
}

//////////////////////////////////////////////////////////////////////////
IDomainContextPtr CFramework::CreateDomainContext(const Schematyc2::IScriptElement* element)
{
	SDomainContextScope bridgeScope(m_scriptRegistry.Wrapfile(const_cast<Schematyc2::IScriptFile*>(&element->GetFile())), element->GetGUID());		
	return CreateDomainContext(bridgeScope);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IValidatorArchivePtr CFramework::CreateValidatorArchive(const Schematyc2::SValidatorArchiveParams& params) const
{
	return Delegate()->CreateValidatorArchive(params);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IGameResourceListPtr CFramework::CreateGameResoucreList() const
{
	return Delegate()->CreateGameResoucreList();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IResourceCollectorArchivePtr CFramework::CreateResourceCollectorArchive(Schematyc2::IGameResourceListPtr pResourceList) const
{
	return Delegate()->CreateResourceCollectorArchive(pResourceList);
}

//////////////////////////////////////////////////////////////////////////
void CFramework::RefreshLogFileSettings()
{
	return Delegate()->RefreshLogFileSettings();
}

//////////////////////////////////////////////////////////////////////////
void CFramework::RefreshEnv()
{
	Delegate()->RefreshEnv();
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::SFrameworkSignals& CFramework::Signals()
{
	return Delegate()->Signals();
}

//////////////////////////////////////////////////////////////////////////
void CFramework::PrePhysicsUpdate()
{
	Delegate()->PrePhysicsUpdate();
}

//////////////////////////////////////////////////////////////////////////
void CFramework::Update()
{
	Delegate()->Update();
}

//////////////////////////////////////////////////////////////////////////
void CFramework::SetUpdateRelevancyContext(Schematyc2::CUpdateRelevanceContext& context)
{
	Delegate()->SetUpdateRelevancyContext(context);
}

//////////////////////////////////////////////////////////////////////////
Schematyc2::IFramework* CFramework::Delegate() const
{
	CRY_ASSERT(gEnv->pSchematyc2);
	return gEnv->pSchematyc2;
}

}

CRYREGISTER_SINGLETON_CLASS(Bridge::CFramework)
