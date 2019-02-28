// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Framework.h"

#include <CryCore/Platform/platform_impl.inl>

#include <CryExtension/CryCreateClassInstance.h>
#include <CrySchematyc2/EnvTypeDesc.h>
#include <CrySchematyc2/Serialization/SerializationEnums.inl>
#include <CrySchematyc2/Serialization/SerializationUtils.h>

#include "CVars.h"
#include "DomainContext.h"
#include "Serialization/SerializationContext.h"
#include "Serialization/ValidatorArchive.h"
#include "Serialization/Resources/GameResourceList.h"
#include "Serialization/Resources/ResourceCollectorArchive.h"
#include "Services/Log.h"
#include "UnitTests/UnitTestRegistrar.h"

#include "BaseEnv/BaseEnv_BaseEnv.h"

namespace Schematyc2
{
	namespace
	{
		// #SchematycTODO : Pass settings from game to framework during initialization?
		static const char* OLD_SCRIPT_SUB_FOLDER = "docs";
		static const char* OLD_SCRIPT_EXTENSION  = "xml";
		static const char* g_szScriptsFolder     = "scripts";
		static const char* g_szScriptExtension   = "ssf";
		static const char* g_szSettingsFolder    = "settings";
		static const char* g_szSettingsExtension = "xml";

		static const SGUID	BOOL_TYPE_GUID                 = "03211ee1-b8d3-42a5-bfdc-296fc535fe43";
		static const SGUID	INT32_TYPE_GUID                = "8bd755fd-ed92-42e8-a488-79e7f1051b1a";
		static const SGUID	UINT32_TYPE_GUID               = "db4b8137-5150-4246-b193-d8d509bec2d4";
		static const SGUID	FLOAT_TYPE_GUID                = "03d99d5a-cf2c-4f8a-8489-7da5b515c202";
		static const SGUID	STRING_TYPE_GUID               = "02b79308-c51c-4841-99ec-c887577217a7";
		static const SGUID	OBJECT_ID_TYPE_GUID            = "95b8918e-9e65-4b6c-9c48-8899754f9d3c";
		static const SGUID	ENTITY_ID_TYPE_GUID            = "00782e22-3188-4538-b4f2-8749b8a9dc48";
		static const SGUID	VEC2_TYPE_GUID                 = "6c607bf5-76d7-45d0-9b34-9d81a13c3c89";
		static const SGUID	VEC3_TYPE_GUID                 = "e01bd066-2a42-493d-bc43-436b233833d8";
		static const SGUID	ANG3_TYPE_GUID                 = "5aafd383-4b89-4249-91d3-0e42f3a094a5";
		static const SGUID	QROTATION_TYPE_GUID            = "991960c1-3b34-45cc-ac3c-f3f0e55ed7ef";
		static const SGUID	CHARACTER_FILE_NAME_TYPE_GUID  = "cb3189c1-92de-4851-b26a-22894ec039b0";
		static const SGUID	GEOMETRY_FILE_NAME_TYPE_GUID   = "bd6f2953-1127-4cdd-bfe7-79f98c97058c";
		static const SGUID	PARTICLE_EFFECT_NAME_TYPE_GUID = "a6f326e7-d3d3-428a-92c9-97185f003bec";
		static const SGUID	SOUND_NAME_TYPE_GUID           = "328e6265-5d53-4d87-8fdd-8b7c0176299a";
		static const SGUID	DIALOG_NAME_TYPE_GUID          = "839adde8-cc62-4636-9f26-16dd4236855f";
		static const SGUID	FORCE_FEEDBACK_ID_TYPE_GUID    = "40dce92c-9532-4c02-8752-4afac04331ef";

		//////////////////////////////////////////////////////////////////////////
		static void OnLogToFileChange(ICVar* pCVar)
		{
			static_cast<CFramework*>(gEnv->pSchematyc2)->RefreshLogFileConfiguration();
		}

		//////////////////////////////////////////////////////////////////////////
		static void OnLogFileStreamsChange(ICVar* pCVar)
		{
			static_cast<CFramework*>(gEnv->pSchematyc2)->RefreshLogFileStreams();
		}

		//////////////////////////////////////////////////////////////////////////
		static void OnLogFileMessageTypesChange(ICVar* pCVar)
		{ 
			static_cast<CFramework*>(gEnv->pSchematyc2)->RefreshLogFileMessageTypes();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	CFramework::CFramework()
		: m_updateRelevanceContext(nullptr, 0, 0.f, 0.f) {}

	//////////////////////////////////////////////////////////////////////////
	CFramework::~CFramework() 
	{
		Schematyc2::CVars::Unregister();

		// TODO: Currently set to null in CSystem::UnloadSchematyc() funcion. Remove it there as soon as we have fixed internal Schematyc shutdown order.
		//gEnv->pSchematyc2 = nullptr;
		// ~TODO
	}

	//////////////////////////////////////////////////////////////////////////
	bool CFramework::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
	{
		env.pSchematyc2 = this;

		Schematyc2::CVars::Register();
		// Initialise log and create output file.
		m_log.Init();
		{
			stack_string logFileName;
			const int    applicationInstance = gEnv->pSystem->GetApplicationInstance();
			if(applicationInstance)
			{
				logFileName.Format("schematyc_legacy(%d).log", applicationInstance);
			}
			else
			{
				logFileName = "schematyc_legacy.log";
			}
			m_pLogFileOutput = m_log.CreateFileOutput(logFileName.c_str());
			SCHEMATYC2_SYSTEM_ASSERT(m_pLogFileOutput);
			CVars::sc_LogToFile->SetOnChangeCallback(OnLogToFileChange);
			RefreshLogFileStreams();
			CVars::sc_LogFileStreams->SetOnChangeCallback(OnLogFileStreamsChange);
			RefreshLogFileMessageTypes();
			CVars::sc_LogFileMessageTypes->SetOnChangeCallback(OnLogFileMessageTypesChange);
		}
		SCHEMATYC2_SYSTEM_COMMENT("Initializing Schematyc framework");
		// Refresh environment.
		RefreshEnv();
		// Run unit tests.
		if(CVars::sc_RunUnitTests)
		{
			CUnitTestRegistrar::RunUnitTests();
		}

		m_pBaseEnv.reset(new SchematycBaseEnv::CBaseEnv());

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::SetGUIDGenerator(const GUIDGenerator& guidGenerator)
	{
		m_guidGenerator = guidGenerator;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CFramework::CreateGUID() const
	{
		return m_guidGenerator ? m_guidGenerator() : SGUID();
	}

	//////////////////////////////////////////////////////////////////////////
	IStringPool& CFramework::GetStringPool()
	{
		return m_stringPool;
	}

	//////////////////////////////////////////////////////////////////////////
	IEnvRegistry& CFramework::GetEnvRegistry()
	{
		return m_envRegistry;
	}

	SchematycBaseEnv::IBaseEnv& CFramework::GetBaseEnv()
	{
		return *m_pBaseEnv.get();
	}

	//////////////////////////////////////////////////////////////////////////
	ILibRegistry& CFramework::GetLibRegistry()
	{
		return m_libRegistry;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetFileFormat() const
	{
		return CVars::GetStringSafe(CVars::sc_FileFormat);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetRootFolder() const
	{
		return CVars::GetStringSafe(CVars::sc_RootFolder);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetOldScriptsFolder() const
	{
		m_oldScriptsFolder = GetRootFolder();
		m_oldScriptsFolder.append("/");
		m_oldScriptsFolder.append(OLD_SCRIPT_SUB_FOLDER);
		return m_oldScriptsFolder.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetOldScriptExtension() const
	{
		return OLD_SCRIPT_EXTENSION;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetScriptsFolder() const
	{
		m_scriptsFolder = GetRootFolder();
		m_scriptsFolder.append("/");
		m_scriptsFolder.append(g_szScriptsFolder);
		return m_scriptsFolder.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetSettingsFolder() const
	{
		m_settingsFolder = GetRootFolder();
		m_settingsFolder.append("/");
		m_settingsFolder.append(g_szSettingsFolder);
		return m_settingsFolder.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetSettingsExtension() const
	{
		return g_szSettingsExtension;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CFramework::IsExperimentalFeatureEnabled(const char* szFeatureName) const
	{
		return CryStringUtils::stristr(CVars::GetStringSafe(CVars::sc_ExperimentalFeatures), szFeatureName) != nullptr;
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
	IObjectManager& CFramework::GetObjectManager()
	{
		return m_objectManager;
	}

	//////////////////////////////////////////////////////////////////////////
	ILog& CFramework::GetLog()
	{
		return m_log;
	}

	//////////////////////////////////////////////////////////////////////////
	ILogRecorder& CFramework::GetLogRecorder()
	{
		return m_logRecorder;
	}

	//////////////////////////////////////////////////////////////////////////
	IUpdateScheduler& CFramework::GetUpdateScheduler()
	{
		return m_updateScheduler;
	}

	//////////////////////////////////////////////////////////////////////////
	ITimerSystem& CFramework::GetTimerSystem()
	{
		return m_timerSystem;
	}

	//////////////////////////////////////////////////////////////////////////
	ISerializationContextPtr CFramework::CreateSerializationContext(const SSerializationContextParams& params) const
	{
		return ISerializationContextPtr(std::make_shared<CSerializationContext>(params));
	}

	//////////////////////////////////////////////////////////////////////////
	IDomainContextPtr CFramework::CreateDomainContext(const SDomainContextScope& scope) const
	{
		return IDomainContextPtr(std::make_shared<CDomainContext>(scope));
	}

	//////////////////////////////////////////////////////////////////////////
	IValidatorArchivePtr CFramework::CreateValidatorArchive(const SValidatorArchiveParams& params) const
	{
		return IValidatorArchivePtr(std::make_shared<CValidatorArchive>(params));
	}

	//////////////////////////////////////////////////////////////////////////
	IGameResourceListPtr CFramework::CreateGameResoucreList() const
	{
		return IGameResourceListPtr(std::make_shared<CGameResourceList>());
	}

	//////////////////////////////////////////////////////////////////////////
	IResourceCollectorArchivePtr CFramework::CreateResourceCollectorArchive(IGameResourceListPtr pResourceList) const
	{
		return IResourceCollectorArchivePtr(std::make_shared<CResourceCollectorArchive>(pResourceList));
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshLogFileSettings()
	{
		RefreshLogFileStreams();
		RefreshLogFileMessageTypes();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshEnv()
	{
		// Clear registries.
		m_libRegistry.Clear();
		m_envRegistry.Clear();
		// Register core types.
		// #SchematycTODO : Move some/most of these to base env!
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(BOOL_TYPE_GUID, "Bool", "Boolean", bool(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(INT32_TYPE_GUID, "Int32", "Signed 32bit integer", int32(), EEnvTypeFlags::Switchable));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(UINT32_TYPE_GUID, "UInt32", "Unsigned 32bit integer", uint32(), EEnvTypeFlags::Switchable));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(FLOAT_TYPE_GUID, "Float", "32bit floating point number", float(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(STRING_TYPE_GUID, "String", "String", CPoolString(), EEnvTypeFlags::Switchable));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(OBJECT_ID_TYPE_GUID, "ObjectId", "Object id", ObjectId(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(ENTITY_ID_TYPE_GUID, "EntityId", "Entity id", ExplicitEntityId::s_invalid, EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(VEC2_TYPE_GUID, "Vector2", "2d vector", Vec2(ZERO), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(VEC3_TYPE_GUID, "Vector3", "3d vector", Vec3(ZERO), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(ANG3_TYPE_GUID, "Angle3", "3d euler angle", Ang3(ZERO), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(QROTATION_TYPE_GUID, "QRotation", "Quaternion rotation", QRotation(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(CHARACTER_FILE_NAME_TYPE_GUID, "CharacterFileName", "Character file name", SCharacterFileName(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(GEOMETRY_FILE_NAME_TYPE_GUID, "GeometryFileName", "Geometry file name", SGeometryFileName(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(PARTICLE_EFFECT_NAME_TYPE_GUID, "ParticleEffectName", "Particle effect name", SParticleEffectName(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(SOUND_NAME_TYPE_GUID, "SoundName", "Sound name", SSoundName(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(DIALOG_NAME_TYPE_GUID, "DialogName", "Dialog name", SDialogName(), EEnvTypeFlags::None));
		m_envRegistry.RegisterTypeDesc(MakeEnvTypeDescShared(FORCE_FEEDBACK_ID_TYPE_GUID, "ForceFeedbackId", "Force feedback id", SForceFeedbackId(), EEnvTypeFlags::None));
		// Send signal to refresh environment.
		m_signals.envRefresh.Send();
	}

	//////////////////////////////////////////////////////////////////////////
	SFrameworkSignals& CFramework::Signals()
	{
		return m_signals;
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::PrePhysicsUpdate()
	{
		m_pBaseEnv->PrePhysicsUpdate();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::Update()
	{
		m_pBaseEnv->Update(&m_updateRelevanceContext);
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshLogFileConfiguration()
	{
		if (m_pLogFileOutput)
		{
			int sc_LogToFile = CVars::sc_LogToFile->GetIVal();
			m_pLogFileOutput->ConfigureFileOutput(sc_LogToFile == 1, sc_LogToFile == 2);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshLogFileStreams()
	{
		if(m_pLogFileOutput)
		{
			m_pLogFileOutput->ClearStreams();

			stack_string logFileStreams = CVars::GetStringSafe(CVars::sc_LogFileStreams);
			const size_t length = logFileStreams.length();
			int          pos = 0;
			do
			{
				stack_string      token = logFileStreams.Tokenize(" ", pos);
				const LogStreamId logStreamId = gEnv->pSchematyc2->GetLog().GetStreamId(token.c_str());
				if(logStreamId != LogStreamId::s_invalid)
				{
					m_pLogFileOutput->EnableStream(logStreamId);
				}
			} while(pos < length);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshLogFileMessageTypes()
	{
		if(m_pLogFileOutput)
		{
			m_pLogFileOutput->ClearMessageTypes();

			stack_string logFileMessageTypes = CVars::GetStringSafe(CVars::sc_LogFileMessageTypes);
			const size_t length = logFileMessageTypes.length();
			int          pos = 0;
			do
			{
				stack_string          token = logFileMessageTypes.Tokenize(" ", pos);
				const ELogMessageType logMessageType = gEnv->pSchematyc2->GetLog().GetMessageType(token.c_str());
				if(logMessageType != ELogMessageType::Invalid)
				{
					m_pLogFileOutput->EnableMessageType(logMessageType);
				}
			} while(pos < length);
		}
	}

	CRYREGISTER_SINGLETON_CLASS(CFramework)
}
