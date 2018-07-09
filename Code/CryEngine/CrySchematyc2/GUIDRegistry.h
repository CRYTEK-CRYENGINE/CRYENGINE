// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/ClassWeaver.h>
#include <CrySchematyc2/IFramework.h>

#include "CrySchematyc2/Compiler.h"
#include "CrySchematyc2/DocManager.h"
#include "CrySchematyc2/EnvRegistry.h"
#include "CrySchematyc2/LibRegistry.h"
#include "CrySchematyc2/Log.h"
#include "CrySchematyc2/ObjectManager.h"
#include "CrySchematyc2/TimerSystem.h"
#include "CrySchematyc2/StringPool.h"
#include "CrySchematyc2/UpdateScheduler.h"

#define SCHEMATYC_FRAMEWORK_CLASS_NAME "GameExtension_SchematycFramework"

namespace Schematyc
{
	// Framework.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CFramework : public IFramework
	{
		CRYINTERFACE_SIMPLE(IFramework)
		CRYGENERATE_CLASS_GUID(CFramework, SCHEMATYC_FRAMEWORK_CLASS_NAME, "96d98d98-35aa-4fb6-830b-53dbfe71908d"_cry_guid)

	public:

		void Init();

		// IFramework
		virtual void SetGUIDGenerator(const TGUIDGenerator& guidGenerator) OVERRIDE;
		virtual SGUID CreateGUID() const OVERRIDE;
		virtual IStringPool& GetStringPool() OVERRIDE;
		virtual IEnvRegistry& GetEnvRegistry() OVERRIDE;
		virtual ILibRegistry& GetLibRegistry() OVERRIDE;
		virtual const char* GetRootFolder() const OVERRIDE;
		virtual const char* GetDocFolder() const OVERRIDE;
		virtual const char* GetDocExtension() const OVERRIDE;
		virtual const char* GetSettingsFolder() const OVERRIDE;
		virtual const char* GetSettingsExtension() const OVERRIDE;
		virtual IDocManager& GetDocManager() OVERRIDE;
		virtual ICompiler& GetCompiler() OVERRIDE;
		virtual IObjectManager& GetObjectManager() OVERRIDE;
		virtual ILog& GetLog() OVERRIDE;
		virtual IUpdateScheduler& GetUpdateScheduler() OVERRIDE;
		virtual ITimerSystem& GetTimerSystem() OVERRIDE;
		virtual IRefreshEnvSignal& GetRefreshEnvSignal() OVERRIDE;
		virtual void RefreshEnv() OVERRIDE;
		// ~IFramework

	private:

		typedef TemplateUtils::CSignal<void ()> TRefreshEnvSignal;

		TGUIDGenerator		m_guidGenerator;
		CStringPool				m_stringPool;
		CEnvRegistry			m_envRegistry;
		CLibRegistry			m_libRegistry;
		CDocManager				m_docManager;
		mutable string		m_docFolder;
		mutable string		m_settingsFolder;
		CCompiler					m_compiler;
		CTimerSystem			m_timerSystem;	// TODO : Shutdown is reliant on order of destruction so we should formalize this rather than relying on placement within CFramework class.
		CObjectManager		m_objectManager;
		CLog							m_log;
		CUpdateScheduler	m_updateScheduler;
		TRefreshEnvSignal	m_refreshEnvSignal;
	};
}
