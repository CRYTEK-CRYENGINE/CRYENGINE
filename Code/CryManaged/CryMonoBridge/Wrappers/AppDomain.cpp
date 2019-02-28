// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AppDomain.h"

#include "MonoRuntime.h"
#include "MonoLibrary.h"
#include "CompiledMonoLibrary.h"
#include "MonoClass.h"
#include "MonoMethod.h"

#include "RootMonoDomain.h"
#include <CrySystem/ISystem.h>
#include <CrySystem/IProjectManager.h>

CAppDomain::CAppDomain(char *name, bool bActivate)
	: m_name(name)
{
	CreateDomain(name, bActivate);

	m_bNativeDomain = true;

	char executableFolder[_MAX_PATH];
	CryGetExecutableFolder(_MAX_PATH, executableFolder);

	CleanTempDirectory();

	string libraryPath = PathUtil::Make(executableFolder, "CryEngine.Common");
	m_pLibCommon = LoadLibrary(libraryPath);
	if (m_pLibCommon == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to load managed common library!");
		return;
	}

	libraryPath = PathUtil::Make(executableFolder, "CryEngine.Core");
	m_pLibCore = LoadLibrary(libraryPath);
	if (m_pLibCore == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Failed to load managed core library!");
		return;
	}

	m_pVector2Class = m_pLibCore->GetClass("CryEngine", "Vector2");
	m_pVector3Class = m_pLibCore->GetClass("CryEngine", "Vector3");
	m_pVector4Class = m_pLibCore->GetClass("CryEngine", "Vector4");
	m_pQuaternionClass = m_pLibCore->GetClass("CryEngine", "Quaternion");
	m_pAngles3Class = m_pLibCore->GetClass("CryEngine", "Angles3");
	m_pColorClass = m_pLibCore->GetClass("CryEngine", "Color");
}

CAppDomain::CAppDomain(MonoInternals::MonoDomain* pMonoDomain)
{
	m_pDomain = pMonoDomain;

#ifndef _RELEASE
	MonoInternals::mono_debug_domain_create(m_pDomain);
#endif

	m_bNativeDomain = false;
}

CAppDomain::~CAppDomain()
{
	if (m_pLibCore != nullptr)
	{
		// Get the equivalent of gEnv
		std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
		CRY_ASSERT(pEngineClass != nullptr);

		// Call the static Shutdown function
		if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethod("OnEngineShutdown").lock())
		{
			pMethod->Invoke();
		}
	}
}

void CAppDomain::Initialize()
{
	CacheObjectMethods();

	if (m_pLibCore != nullptr)
	{
		// Get the equivalent of gEnv
		std::shared_ptr<CMonoClass> pEngineClass = m_pLibCore->GetTemporaryClass("CryEngine", "Engine");
		CRY_ASSERT(pEngineClass != nullptr);

		// Call the static Initialize function
		if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethod("OnEngineStart").lock())
		{
			pMethod->Invoke();
		}
	}
}

void CAppDomain::CreateDomain(char *name, bool bActivate)
{
	m_pDomain = MonoInternals::mono_domain_create_appdomain(name, nullptr);

	char baseDirectory[_MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(baseDirectory), baseDirectory);

	string sConfigFile;
	sConfigFile.Format("%smono\\etc\\mono\\4.5\\machine.config", baseDirectory);

	char szEngineBinaryDir[_MAX_PATH];
	CryGetExecutableFolder(CRY_ARRAY_COUNT(szEngineBinaryDir), szEngineBinaryDir);

	MonoInternals::mono_domain_set_config(m_pDomain, szEngineBinaryDir, sConfigFile);

	if (bActivate)
	{
		Activate();
	}

#ifndef _RELEASE
	MonoInternals::mono_debug_domain_create(m_pDomain);
#endif
}

bool CAppDomain::Reload()
{
	m_isReloading = true;

	// Trigger removal of C++ listeners, since the memory they belong to will be invalid shortly
	std::shared_ptr<CMonoClass> pEngineClass = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine", "Engine");
	CRY_ASSERT(pEngineClass != nullptr);

	// Call the static Shutdown function
	if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethod("OnUnloadStart").lock())
	{
		pMethod->Invoke();
	}

	// Make note of whether we were active or not, so we can reapply state to the new domain
	bool bWasActive = IsActive();

	std::vector<char> serializedData;

	// Serialize statics and objects in the assemblies
	SerializeDomainData(serializedData);

	// Unload assemblies
	UnloadAssemblies();

	// Kill the domain
	Unload();

	// Now start recreating the domain
	// Copy the name, Mono doesn't actually touch the char array in memory but let's be safe.
	string name = m_name;
	CreateDomain(const_cast<char*>(name.c_str()), bWasActive);

	// Reload assemblies
	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		pLibrary->Reload();
	}

	CacheObjectMethods();

	// Notify plug-ins of reload
	pEngineClass->ReloadClass();

	// Create the deserializer
	m_serializationTicks = 0;
	std::shared_ptr<CMonoObject> pSerializer = CreateDeserializer(serializedData);

	// Deserialize our internal libraries before notifying the run-time
	// This ensures that we can register components before deserializing them
	m_pLibCommon->Deserialize(pSerializer.get());
	m_pLibCore->Deserialize(pSerializer.get());

	// Now notify the run-time
	GetMonoRuntime()->OnCoreLibrariesDeserialized();

	// Deserialize other libraries
	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		if (pLibrary.get() != m_pLibCommon && pLibrary.get() != m_pLibCore)
		{
			pLibrary->Deserialize(pSerializer.get());
		}
	}

	GetMonoRuntime()->OnPluginLibrariesDeserialized();

	CryLogAlways("Total de-serialization time: %f ms", (1000.f * gEnv->pTimer->TicksToSeconds(m_serializationTicks)));

	// Notify the framework so that internal listeners etc. can be added again.
	if (std::shared_ptr<CMonoMethod> pMethod = pEngineClass->FindMethod("OnReloadDone").lock())
	{
		pMethod->Invoke();
	}

	m_isReloading = false;

	return true;
}

CMonoLibrary* CAppDomain::CompileFromSource(const char* szDirectory)
{
	m_loadedLibraries.emplace_back(stl::make_unique<CCompiledMonoLibrary>(szDirectory, this));
	return m_loadedLibraries.back().get();
}

CMonoLibrary* CAppDomain::GetCompiledLibrary()
{
	m_loadedLibraries.emplace_back(stl::make_unique<CCompiledMonoLibrary>(this));
	return m_loadedLibraries.back().get();
}

void CAppDomain::SerializeDomainData(std::vector<char>& serializedData)
{
	// Create the shared serialization utilities that may be called by libraries and objects
	// Now serialize the statics contained inside this library
	CMonoLibrary& netCoreLibrary = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary();

	std::shared_ptr<CMonoClass> pMemoryStreamClass = netCoreLibrary.GetTemporaryClass("System.IO", "MemoryStream");
	std::shared_ptr<CMonoObject> pMemoryStream = std::static_pointer_cast<CMonoObject>(pMemoryStreamClass->CreateInstance());

	m_serializationTicks = 0;

	std::shared_ptr<CMonoClass> pCryObjectWriterClass = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Serialization", "ObjectWriter");

	void* writerConstructorArgs[1];
	writerConstructorArgs[0] = pMemoryStream->GetManagedObject();

	std::shared_ptr<CMonoObject> pSerializer = std::static_pointer_cast<CMonoObject>(pCryObjectWriterClass->CreateInstance(writerConstructorArgs, 1));

	for (const std::unique_ptr<CMonoLibrary>& pLibrary : m_loadedLibraries)
	{
		pLibrary->Serialize(pSerializer.get());
	}

	// Grab the serialized buffer
	if (std::shared_ptr<CMonoMethod> pMethod = pMemoryStream->GetClass()->FindMethod("GetBuffer").lock())
	{
		std::shared_ptr<CMonoObject> pManagedBuffer = pMethod->Invoke(pMemoryStream.get());

		serializedData.resize(pManagedBuffer->GetArraySize());
		char* arrayBuffer = pManagedBuffer->GetArrayAddress(sizeof(char), 0);

		memcpy(serializedData.data(), arrayBuffer, serializedData.size());

		CryLogAlways("Total serialization time: %f ms, %iMB used", (1000.f * gEnv->pTimer->TicksToSeconds(m_serializationTicks)), serializedData.size() / (1024 * 1024));
	}
	else
	{
		CRY_ASSERT(false);
	}
}

std::shared_ptr<CMonoObject> CAppDomain::CreateDeserializer(const std::vector<char>& serializedData)
{
	std::shared_ptr<CMonoClass> pCryObjectReaderClass = GetMonoRuntime()->GetCryCoreLibrary()->GetTemporaryClass("CryEngine.Serialization", "ObjectReader");

	CMonoDomain* pDomain = pCryObjectReaderClass->GetAssembly()->GetDomain();

	CMonoLibrary& netCoreLibrary = GetMonoRuntime()->GetRootDomain()->GetNetCoreLibrary();

	MonoInternals::MonoArray* pMonoManagedBuffer = MonoInternals::mono_array_new(pDomain->GetMonoDomain(), MonoInternals::mono_get_byte_class(), serializedData.size());
	std::shared_ptr<CMonoClass> pBufferClass = netCoreLibrary.GetClassFromMonoClass(MonoInternals::mono_object_get_class((MonoInternals::MonoObject*)pMonoManagedBuffer));

	std::shared_ptr<CMonoObject> pManagedBuffer = std::make_shared<CMonoObject>((MonoInternals::MonoObject*)pMonoManagedBuffer, pBufferClass);
	char* arrayBuffer = pManagedBuffer->GetArrayAddress(sizeof(char), 0);

	memcpy(arrayBuffer, serializedData.data(), serializedData.size());

	void* memoryStreamCtorArgs[2];
	memoryStreamCtorArgs[0] = pMonoManagedBuffer;

	bool bWritable = false;
	MonoInternals::MonoObject* pBoolObject = MonoInternals::mono_value_box(pDomain->GetMonoDomain(), MonoInternals::mono_get_boolean_class(), &bWritable);
	memoryStreamCtorArgs[1] = pBoolObject;

	std::shared_ptr<CMonoClass> pMemoryStreamClass = netCoreLibrary.GetTemporaryClass("System.IO", "MemoryStream");
	std::shared_ptr<CMonoObject> pMemoryStream = std::static_pointer_cast<CMonoObject>(pMemoryStreamClass->CreateInstance(memoryStreamCtorArgs, 2));

	void* readerConstructorParams[1];
	readerConstructorParams[0] = pMemoryStream->GetManagedObject();

	return std::static_pointer_cast<CMonoObject>(pCryObjectReaderClass->CreateInstance(readerConstructorParams, 1));
}

void CAppDomain::SerializeObject(CMonoObject* pSerializer, MonoInternals::MonoObject* pObject, bool bIsAssembly)
{
	if (std::shared_ptr<CMonoMethod> pMethod = pSerializer->GetClass()->FindMethod(bIsAssembly ? "WriteStatics" : "Write", 1).lock())
	{
		void* writeParams[1];
		writeParams[0] = pObject;

		int64 startTime = CryGetTicks();
		pMethod->Invoke(pSerializer, writeParams);
		m_serializationTicks += CryGetTicks() - startTime;
	}
	else
	{
		CRY_ASSERT(false);
	}
}

std::shared_ptr<CMonoObject> CAppDomain::DeserializeObject(CMonoObject* pSerializer, const CMonoClass* const pObjectClass)
{
	// Now serialize the statics contained inside this library
	if (std::shared_ptr<CMonoMethod> pMethod = pSerializer->GetClass()->FindMethod(pObjectClass == nullptr ? "ReadStatics" : "Read").lock())
	{
		int64 startTime = CryGetTicks();
		std::shared_ptr<CMonoObject> pReadResult = pMethod->Invoke(pSerializer);
		m_serializationTicks += CryGetTicks() - startTime;

		return pReadResult;
	}

	return nullptr;
}

void CAppDomain::DeserializeDeletedObject(CMonoObject* pSerializer)
{
	// Now serialize the statics contained inside this library
	if (std::shared_ptr<CMonoMethod> pMethod = pSerializer->GetClass()->FindMethod("Read").lock())
	{
		int64 startTime = CryGetTicks();
		pMethod->Invoke(pSerializer);
		m_serializationTicks += CryGetTicks() - startTime;
	}
}
