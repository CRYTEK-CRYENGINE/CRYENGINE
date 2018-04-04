// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>
#include <CryNetwork/INetwork.h>

#include "Wrappers/MonoObject.h"
#include "Wrappers/AppDomain.h"

#include "NativeComponents/EntityComponentFactory.h"

#include <CrySchematyc/CoreAPI.h>

class CMonoLibrary;

// Main entry-point handler for managed code, indirectly created by the plugin manager
class CManagedPlugin final
	: public ICryPlugin
	, public ISystemEventListener
	, public INetworkedClientListener
{
	// Package for Schematyc to register entity components and nodes coming from this plug-in
	class CSchematycPackage final : public Schematyc::IEnvPackage
	{
	public:
		CSchematycPackage(const CManagedPlugin& plugin)
			: m_plugin(plugin) {}

		// Schematyc::IEnvPackage
		virtual CryGUID GetGUID() const override { return m_plugin.GetGUID(); }
		virtual const char* GetName() const override { return m_plugin.GetName(); }
		virtual const char* GetAuthor() const override { return ""; }
		virtual const char* GetDescription() const override { return ""; }
		virtual Schematyc::EnvPackageCallback GetCallback() const { return [this](Schematyc::IEnvRegistrar& registrar) { m_plugin.RegisterSchematycPackageContents(registrar); }; }
		// ~Schematyc::IEnvPackage

	protected:
		const CManagedPlugin& m_plugin;
	};

public:
	CManagedPlugin(const char* szBinaryPath);
	CManagedPlugin(CMonoLibrary* pLibrary);
	virtual ~CManagedPlugin();

	// ICryUnknown
	virtual ICryFactory* GetFactory() const override { return nullptr; }

	virtual void* QueryInterface(const CryInterfaceID& iid) const override { return nullptr; }
	virtual void* QueryComposite(const char* name) const override { return nullptr; }
	// ~ICryUnknown

	// ICryPlugin
	virtual const char* GetName() const override { return m_name; }
	virtual const char* GetCategory() const override { return "Managed"; }

	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override { return true; }

	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
	// ~ICryPlugin

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	void Load(CAppDomain* pDomain);

	// INetworkedClientListener
	virtual void OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) {}

	virtual bool OnClientConnectionReceived(int channelId, bool bIsReset) override;
	virtual bool OnClientReadyForGameplay(int channelId, bool bIsReset) override;
	virtual void OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override;
	virtual bool OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override { return true; }
	// ~INetworkedClientListener

	void RegisterSchematycPackageContents(Schematyc::IEnvRegistrar& registrar) const;
	const CryGUID& GetGUID() const { return m_guid; }

	typedef std::map <MonoInternals::MonoReflectionType*, std::shared_ptr <CManagedEntityComponentFactory>> TComponentFactoryMap;

	// The plug-in that is currently registering types in CManagedPlugin::InitializePlugin
	static TComponentFactoryMap* s_pCurrentlyRegisteringFactory;

protected:
	void InitializePlugin();

protected:
	CMonoLibrary* m_pLibrary;

	string m_name;
	std::shared_ptr<CMonoObject> m_pMonoObject;

	string m_libraryPath;
	CryGUID m_guid;

	// Map containing entity component factories for this module
	TComponentFactoryMap m_entityComponentFactoryMap;
};