// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CrySchematyc2/Framework.h"

#if defined(WIN32) || defined(WIN64)
#include <Objbase.h>
#endif

#include <CryExtension/CryCreateClassInstance.h>
#include <CrySchematyc2/SerializationUtils.h>
#include <CrySchematyc2/TypeDesc.h>

#include "CrySchematyc2/CVars.h"
#include "CrySchematyc2/Log.h"

#define SCHEMATYC_FRAMEWORK_CREATOR_CLASS_NAME	"GameExtension_SchematycFrameworkCreator"

namespace Schematyc
{
	namespace
	{
		static const char* DOC_SUB_FOLDER				= "docs";
		static const char* DOC_EXTENSION				= "xml";
		static const char* SETTINGS_SUB_FOLDER	= "Settings";
		static const char* SETTINGS_EXTENSION		= "xml";

		static const SGUID	BOOL_TYPE_GUID									= "03211ee1-b8d3-42a5-bfdc-296fc535fe43";
		static const SGUID	INT32_TYPE_GUID									= "8bd755fd-ed92-42e8-a488-79e7f1051b1a";
		static const SGUID	UINT32_TYPE_GUID								= "db4b8137-5150-4246-b193-d8d509bec2d4";
		static const SGUID	FLOAT_TYPE_GUID									= "03d99d5a-cf2c-4f8a-8489-7da5b515c202";
		static const SGUID	STRING_TYPE_GUID								= "02b79308-c51c-4841-99ec-c887577217a7";
		static const SGUID	OBJECT_ID_TYPE_GUID							= "95b8918e-9e65-4b6c-9c48-8899754f9d3c";
		static const SGUID	ENTITY_ID_TYPE_GUID							= "00782e22-3188-4538-b4f2-8749b8a9dc48";
		static const SGUID	VEC2_TYPE_GUID									= "6c607bf5-76d7-45d0-9b34-9d81a13c3c89";
		static const SGUID	VEC3_TYPE_GUID									= "e01bd066-2a42-493d-bc43-436b233833d8";
		static const SGUID	ANG3_TYPE_GUID									= "5aafd383-4b89-4249-91d3-0e42f3a094a5";
		static const SGUID	QROTATION_TYPE_GUID							= "991960c1-3b34-45cc-ac3c-f3f0e55ed7ef";
		static const SGUID	CHARACTER_FILE_NAME_TYPE_GUID		= "cb3189c1-92de-4851-b26a-22894ec039b0";
		static const SGUID	GEOMETRY_FILE_NAME_TYPE_GUID		= "bd6f2953-1127-4cdd-bfe7-79f98c97058c";
		static const SGUID	PARTICLE_EFFECT_NAME_TYPE_GUID	= "a6f326e7-d3d3-428a-92c9-97185f003bec";
		static const SGUID	SOUND_NAME_TYPE_GUID						= "328e6265-5d53-4d87-8fdd-8b7c0176299a";
		static const SGUID	DIALOG_NAME_TYPE_GUID						= "839adde8-cc62-4636-9f26-16dd4236855f";
		static const SGUID	FORCE_FEEDBACK_ID_TYPE_GUID			= "40dce92c-9532-4c02-8752-4afac04331ef";

		class CSchematycFrameworkCreator : public IGameFrameworkExtensionCreator
		{
			CRYINTERFACE_SIMPLE(IGameFrameworkExtensionCreator)

			CRYGENERATE_SINGLETONCLASS_GUID(CSchematycFrameworkCreator, SCHEMATYC_FRAMEWORK_CREATOR_CLASS_NAME, "0f296b01-ff3f-4f55-aa10-a724a89c73cd"_cry_guid)

			virtual ICryUnknown* Create(IGameFramework* pIGameFramework) OVERRIDE
			{
				// Create Schematyc framework.
				boost::shared_ptr<CFramework> pSchematycFramework;
				if(CryCreateClassInstance(SCHEMATYC_FRAMEWORK_CLASS_NAME, pSchematycFramework) == true)
				{
					// Register Schematyc with game framework
					pIGameFramework->RegisterExtension(pSchematycFramework);
					// Call GetSchematycFramework() to ensure registration was successful and cache the framework pointer locally.					
					GetSchematycFramework();
					// Initialize Schematyc framework.
					pSchematycFramework->Init();
					CryLogAlways("Schematyc framework initialized");
					return pSchematycFramework.get();
				}
				return NULL;
			}
		};

		CSchematycFrameworkCreator::CSchematycFrameworkCreator() {}

		CSchematycFrameworkCreator::~CSchematycFrameworkCreator() {}

		CRYREGISTER_SINGLETON_CLASS(CSchematycFrameworkCreator)
	}

	//////////////////////////////////////////////////////////////////////////
	IFramework* CreateFramework(IGameFramework* pIGameFramework)
	{
		IGameFrameworkExtensionCreatorPtr	pCreator;
		if(CryCreateClassInstance(SCHEMATYC_FRAMEWORK_CREATOR_CLASS_NAME, pCreator) == true)
		{
			return cryinterface_cast<IFramework>(pCreator->Create(pIGameFramework));
		}
		return NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	CFramework::CFramework() {}

	//////////////////////////////////////////////////////////////////////////
	CFramework::~CFramework() {}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::Init()
	{
		// Initialise framework.
		Schematyc::CVars::Register();
		m_log.Init();
		SCHEMATYC_COMMENT("Initializing Schematyc framework");
		// Refresh environment.
		RefreshEnv();
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::SetGUIDGenerator(const TGUIDGenerator& guidGenerator)
	{
		m_guidGenerator = guidGenerator;
	}

	//////////////////////////////////////////////////////////////////////////
	SGUID CFramework::CreateGUID() const
	{
		return m_guidGenerator.Empty() == false ? m_guidGenerator() : SGUID();
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

	//////////////////////////////////////////////////////////////////////////
	ILibRegistry& CFramework::GetLibRegistry()
	{
		return m_libRegistry;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetRootFolder() const
	{
		return CVars::GetStringSafe(CVars::sc_RootFolder);
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetDocFolder() const
	{
		m_docFolder = GetRootFolder();
		m_docFolder.append("/");
		m_docFolder.append(DOC_SUB_FOLDER);
		return m_docFolder.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetDocExtension() const
	{
		return DOC_EXTENSION;
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetSettingsFolder() const
	{
		m_settingsFolder = GetRootFolder();
		m_settingsFolder.append("/");
		m_settingsFolder.append(SETTINGS_SUB_FOLDER);
		return m_settingsFolder.c_str();
	}

	//////////////////////////////////////////////////////////////////////////
	const char* CFramework::GetSettingsExtension() const
	{
		return SETTINGS_EXTENSION;
	}

	//////////////////////////////////////////////////////////////////////////
	IDocManager& CFramework::GetDocManager()
	{
		return m_docManager;
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
	IFramework::IRefreshEnvSignal& CFramework::GetRefreshEnvSignal()
	{
		return m_refreshEnvSignal;
	}

	//////////////////////////////////////////////////////////////////////////
	void CFramework::RefreshEnv()
	{
		// Clear registries.
		m_libRegistry.Clear();
		m_envRegistry.Clear();
		// Register core types.
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(BOOL_TYPE_GUID, "Bool", "Boolean", TypeCategory::PRIMITIVE, bool()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(INT32_TYPE_GUID, "Int32", "Signed 32bit integer", TypeCategory::PRIMITIVE, int32()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(UINT32_TYPE_GUID, "UInt32", "Unsigned 32bit integer", TypeCategory::PRIMITIVE, uint32()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(FLOAT_TYPE_GUID, "Float", "32bit floating point number", TypeCategory::PRIMITIVE, float()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(STRING_TYPE_GUID, "String", "String", TypeCategory::PRIMITIVE, CPoolString()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(OBJECT_ID_TYPE_GUID, "ObjectId", "Object id", TypeCategory::CLASS, TObjectId()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(ENTITY_ID_TYPE_GUID, "EntityId", "Entity id", TypeCategory::CLASS, TEntityId()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(VEC2_TYPE_GUID, "Vec2", "2d vector", TypeCategory::CLASS, Vec2(ZERO)));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(VEC3_TYPE_GUID, "Vec3", "3d vector", TypeCategory::CLASS, Vec3(ZERO)));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(ANG3_TYPE_GUID, "Ang3", "3d euler angle", TypeCategory::CLASS, Ang3(ZERO)));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(QROTATION_TYPE_GUID, "QRotation", "Quaternion rotation", TypeCategory::CLASS, QRotation()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(CHARACTER_FILE_NAME_TYPE_GUID, "CharacterFileName", "Character file name", TypeCategory::CLASS, SCharacterFileName()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(GEOMETRY_FILE_NAME_TYPE_GUID, "GeometryFileName", "Geometry file name", TypeCategory::CLASS, SGeometryFileName()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(PARTICLE_EFFECT_NAME_TYPE_GUID, "ParticleEffectName", "Particle effect name", TypeCategory::CLASS, SParticleEffectName()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(SOUND_NAME_TYPE_GUID, "SoundName", "Sound name", TypeCategory::CLASS, SSoundName()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(DIALOG_NAME_TYPE_GUID, "DialogName", "Dialog name", TypeCategory::CLASS, SDialogName()));
		m_envRegistry.RegisterTypeDesc(MakeTypeDescShared(FORCE_FEEDBACK_ID_TYPE_GUID, "ForceFeedbackId", "Force feedback id", TypeCategory::CLASS, SForceFeedbackId()));
		// Send signal to refresh environment.
		m_refreshEnvSignal.Send();
	}

	CRYREGISTER_CLASS(CFramework)
}
